#include <QApplication>
#include <QString>
#include <iostream>
#include "resourcemanager.h"
#include "viewer.h"


int main(int argc, char *argv[]) {
	using namespace std;
	QApplication app(argc, argv);

	if (argc != 2) {
		cerr << "usage: " << argv[0] << " <path>" << endl;
		return 1;
	}

	Viewer katarakt(QString::fromUtf8(argv[1]));
	if (!katarakt.is_valid()) {
		return 1;
	}

	return app.exec();
}

