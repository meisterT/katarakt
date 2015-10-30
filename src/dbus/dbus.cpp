#include "dbus.h"
#include "source_correlate.h"
#include "../viewer.h"

#include <QDBusConnection>
#include <QApplication>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QRegExp>
#include <QString>
#include <QStringList>
#include <QFileInfo>


#include <iostream>

using namespace std;

void dbus_init(Viewer *viewer) {
	/* Add dbus interfaces to the viewer object.
	 *
	 * These are automatically destroyed, if the parent object is.
	 *
	 * At present, there is only the SourceCorrelate interface.
	 */
	new SourceCorrelate(viewer);

	QString bus_name = QString("katarakt.pid%1").arg(QApplication::applicationPid());

	if (!QDBusConnection::sessionBus().registerService(bus_name)) {
#ifdef DEBUG
		cerr << "Failed to register DBus service" << endl;
#endif	
	} else {
		if (!QDBusConnection::sessionBus().registerObject("/", viewer)) {
#ifdef DEBUG
			cerr << "Failed to register viewer object on DBus" << endl;
#endif
		}
	}
}

bool activate_katarakt_with_file(QString file) {
	if (file.isNull()) {
		// always start a new instance if no argument was given
		return false;
	}

	QString filepath = QFileInfo(file).absoluteFilePath();
	QDBusConnection bus = QDBusConnection::sessionBus();
	QStringList services = bus.interface()->registeredServiceNames().value();
	QStringList katarakts = services.filter(QRegExp("^katarakt\\.pid"));
	foreach (const QString& katarakt_service, katarakts) {
		QDBusInterface dbus_iface(katarakt_service, "/", "katarakt.SourceCorrelate", bus);
		QDBusReply<QString> reply = dbus_iface.call("filepath");
		if (reply.isValid()) {
			if (reply.value() == filepath) {
				dbus_iface.call("focus");
				return true;
			}
		}
	}
	return false;
}
