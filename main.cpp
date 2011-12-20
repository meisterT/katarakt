#include <QApplication>
#include <QImage>
#include <QString>
#include <iostream>

#include "resourcemanager.h"
#include "pdfviewer.h"

int main(int argc, char *argv[]) {
	using namespace std;
	QApplication app(argc, argv);

	if (argc != 2) {
		cerr << "usage: " << argv[0] << " <path>" << endl;
		return 1;
	}

	ResourceManager res(argv[1]);
	if (res.isNull())
		return 1;
	PdfViewer pdf(&res);
	pdf.show();

	return app.exec();
}

