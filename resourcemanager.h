#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <iostream>
#include <list>
#include <utility>
#include <cstring>


class ResourceManager;
class Viewer;


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

	// document not open?
	bool is_null() const;

	// page (meta)data
	QImage *get_page(int page, int newWidth);
	float get_page_width(int page) const;
	float get_page_height(int page) const;
	float get_page_aspect(int page) const;
	int get_page_count() const;

	void unlock_page(int page) const;
	void set_cache_size(unsigned int size);

	void join_threads();
	void setFinishedCallback(void (*_callback)(Viewer *), Viewer *arg);

private:
	void enqueue(int page, int width);
	bool render(int offset);

	// sadly, poppler's renderToImage only supports one thread per document
	Worker worker;

	Poppler::Document *doc;
	QMutex *imgMutex;
	QMutex requestMutex;
	QMutex garbageMutex;
	QSemaphore requestSemaphore;
	std::list<std::pair<int,int> > requests;
	std::list<int> garbage;
	unsigned int cache_size;

	QImage **image;
	int *image_status;

	friend class Worker;

	float *page_width, *page_height;
	void (*callback)(Viewer *);
	Viewer *caller;
};

#endif

