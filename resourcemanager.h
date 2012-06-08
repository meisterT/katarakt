#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QSemaphore>
#include <list>
#include <set>
#include <utility>
#include <cstring>


class ResourceManager;
class Canvas;


class Worker : public QThread {
	Q_OBJECT

public:
	Worker();
	void setResManager(ResourceManager *_res);
	void connect_signal(Canvas *c);
	void run();

	volatile bool die;

signals:
	void page_rendered(int page);

private:
	ResourceManager *res;
};


class ResourceManager {
public:
	ResourceManager(QString file);
	~ResourceManager();

	void load(QString file);

	// document opened correctly?
	bool is_valid() const;

	// page (meta)data
	QImage *get_page(int page, int newWidth);
	float get_page_width(int page) const;
	float get_page_height(int page) const;
	float get_page_aspect(int page) const;
	int get_page_count() const;
	const std::list<Poppler::LinkGoto *> *get_links(int page);

	void unlock_page(int page) const;

	void collect_garbage(int keep_min, int keep_max);

	void connect_canvas(Canvas *c) const;

private:
	void enqueue(int page, int width);
	bool render(int offset);

	void initialize(QString file);
	void join_threads();
	void shutdown();

	// sadly, poppler's renderToImage only supports one thread per document
	Worker *worker;

	Poppler::Document *doc;
	QMutex *imgMutex;
	QMutex requestMutex;
	QMutex garbageMutex;
	QSemaphore requestSemaphore;
	std::list<std::pair<int,int> > requests;
	std::set<int> garbage;
	std::list<Poppler::LinkGoto *> **links;
	QMutex link_mutex;

	QImage **image;
	int *image_status;

	friend class Worker;

	float *page_width, *page_height;
	int page_count;
};

#endif

