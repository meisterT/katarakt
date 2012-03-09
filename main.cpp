#include <QApplication>
#include <QImage>
#include <QString>
#include <QPalette>
#include <iostream>

#include "resourcemanager.h"
#include "viewer.h"

int main(int argc, char *argv[]) {
	using namespace std;
	QApplication app(argc, argv);
	app.setPalette(QPalette(QColor(255, 255, 255), QColor(0, 0, 0)));

	if (argc != 2) {
		cerr << "usage: " << argv[0] << " <path>" << endl;
		return 1;
	}

	ResourceManager res(QString::fromUtf8(argv[1]));
	if (res.is_null()) {
		return 1;
	}
	Viewer pdf(&res);
	pdf.show();

	return app.exec();
}

