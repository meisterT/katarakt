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


class ResourceManager;
class Canvas;


class KPage {
private:
	KPage();
	~KPage();

public:
	const QImage *get_image() const;
	int get_width() const;
	char get_rotation() const;

private:
	float width;
	float height;
	QImage img;
	QImage thumbnail;
	std::list<Poppler::LinkGoto *> *links;
	QMutex mutex;
	int status;
	char rotation;

	friend class Worker;
	friend class ResourceManager;
};


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
	const KPage *get_page(int page, int newWidth);
	float get_page_width(int page) const;
	float get_page_height(int page) const;
	float get_page_aspect(int page) const;
	int get_page_count() const;
	const std::list<Poppler::LinkGoto *> *get_links(int page);

	int get_rotation() const;
	void rotate(int value, bool relative = true);
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
	QMutex requestMutex;
	QMutex garbageMutex;
	QSemaphore requestSemaphore;
	int center_page;
	std::map<int,int> requests;
	std::set<int> garbage;
	QMutex link_mutex;

	KPage *k_page;

	friend class Worker;

	int page_count;
	int rotation;

	// config options
	bool smooth_downscaling;
	int thumbnail_size;
};

#endif

