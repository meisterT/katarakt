#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QString>
#include <QNetworkAccessManager>
#include <QTemporaryFile>


class Download {

public:
	Download();
	~Download();

	QString load(QString);

private:
	QNetworkAccessManager *manager;
	QTemporaryFile *file;

};

#endif
