#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include <QString>
#include <QNetworkAccessManager>
#include <QTemporaryFile>


class Download : public QObject {
	Q_OBJECT

public:
	Download();
	~Download();

	QString load(QString);

private slots:
	void progress(qint64 bytes_received, qint64 bytes_total);

private:
	QNetworkAccessManager *manager;
	QTemporaryFile *file;
};

#endif

