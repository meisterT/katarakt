#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <iostream>
#include <queue>
#include <cstring>


class ResourceManager;
class PdfViewer;


class Worker : public QThread {

public:
	Worker();
	void setResManager(ResourceManager *res);
	void run();

	volatile bool die;

private:
	ResourceManager *res;
};


class ResourceManager {

public:
	ResourceManager(QString file);
	~ResourceManager();

	bool isNull() const;
	QImage *getPage(int page, int newWidth);
	float getPageAspect(int page) const;
	int numPages() const;
	void wait();
	void setFinishedCallback(void (*_callback)(PdfViewer *), PdfViewer *arg);

private:
	bool render(int offset);

	Worker worker; // sadly, poppler's renderToImage only supports one thread

	Poppler::Document *doc;
	QMutex imgMutex;
	QMutex requestMutex;
	QMutex garbageMutex;
	QSemaphore requestSemaphore;
	std::queue<int> requests;
	std::queue<int> garbage;

	volatile int width; // TODO locking
	QImage **image;

	friend class Worker;

	float *pageAspect;
	void (*callback)(PdfViewer *);
	PdfViewer *caller;
};

#endif

