#include "download.h"

#include <iostream>

#include <QEventLoop>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>


Download::Download() :
	manager(new QNetworkAccessManager()),
	file(NULL) {
}

Download::~Download() {
	delete manager;
	delete file;
}



QString Download::load(QString f) {
	QUrl url(f);

	if( url.isLocalFile() || url.isRelative() ) {
		// found local file
		// do not download
		return QDir::toNativeSeparators(url.toLocalFile());
	}

	QEventLoop loop;
	QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), &loop, SLOT(quit()));
	QNetworkRequest request(url);
	QNetworkReply *reply = manager->get(request);
	loop.exec();
#ifdef DEBUG
	std::cerr << "File downloaded" << std::endl;
#endif

	// find uniq temporary filename
	QFileInfo fileInfo(url.path());
	QString fileName = fileInfo.fileName();
	delete file;
	file = new QTemporaryFile(QDir::tempPath() + QDir::separator() + fileName);
	file->setAutoRemove(true);
	file->open();

	if(reply->error() != QNetworkReply::NoError || !reply->isReadable()) {
		std::cerr << "Download failed" << std::cerr;
		return NULL;
	}

	// store data in tempfile
	QByteArray data = reply->readAll();
	qint64 len = file->write(data);
	file->close();
#ifdef DEBUG
	std::cerr << "filename: " << file.toStdString() << std::endl;
#endif
	return file->fileName();
}

