#ifndef WORKER_H
#define WORKER_H

#include <QThread>


class ResourceManager;
class Canvas;


class Worker : public QThread {
	Q_OBJECT

public:
	Worker(ResourceManager *res);
	void run();

	volatile bool die;

signals:
	void page_rendered(int page);

private:
	ResourceManager *res;
};

#endif

