#include "download.h"
#include <iostream>
#include <QEventLoop>
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QByteArray>
#include <QDir>
#include <QFileInfo>

using namespace std;


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
	if (url.isLocalFile() || url.isRelative()) {
		// found local file, do not download
		return QDir::toNativeSeparators(url.toLocalFile());
	}

	QEventLoop loop;
	QObject::connect(manager, SIGNAL(finished(QNetworkReply *)), &loop, SLOT(quit()));
	QNetworkReply *reply = manager->get(QNetworkRequest(url));
	QObject::connect(reply, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(progress(qint64, qint64)));
	loop.exec();
#ifdef DEBUG
	cerr << "File downloaded" << endl;
#endif

	// find unique temporary filename
	QFileInfo fileInfo(url.path());
	QString fileName = fileInfo.fileName();
	delete file;
	file = new QTemporaryFile(QDir::tempPath() + QDir::separator() + fileName);
	file->setAutoRemove(true);
	file->open();

	if (reply->error() != QNetworkReply::NoError || !reply->isReadable()) {
		cerr << reply->errorString().toStdString() << endl;
		return QString();
	}

	// store data in tempfile
	QByteArray data = reply->readAll();
	reply->deleteLater();
	file->write(data);
	file->close();
#ifdef DEBUG
	cerr << "filename: " << file->fileName().toStdString() << endl;
#endif
	return file->fileName();
}

void Download::progress(qint64 bytes_received, qint64 bytes_total) {
	cout.precision(1);
	cout << fixed << (bytes_received / 1024.0f) << "/";
	cout << (bytes_total / 1024.0f) << "KB downloaded\r";
}

