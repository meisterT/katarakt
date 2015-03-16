#include "dbus.h"
#include "source_correlate.h"
#include "../viewer.h"

#include <QDBusConnection>
#include <QApplication>

#include <iostream>

using namespace std;

void dbus_init(Viewer *viewer)
{
	/* Add dbus interfaces to the viewer object.
	 *
	 * These are automatically destroyed, if the parent object is.
	 *
	 * At present, there is only the SourceCorrelate interface.
	 */
	new SourceCorrelate(viewer);

	QString bus_name = QString("katarakt.pid%1")
	                     .arg(QApplication::applicationPid());

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
