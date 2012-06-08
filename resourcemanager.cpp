#include "resourcemanager.h"
#include "canvas.h"
#include <iostream>

using namespace std;


//#define DEBUG // qmake now takes care of this


//==[ Worker ]=================================================================
Worker::Worker() :
		die(false) {
}

void Worker::setResManager(ResourceManager *_res) {
	res = _res;
}

void Worker::connect_signal(Canvas *c) {
	connect(this, SIGNAL(page_rendered(int)), c, SLOT(page_rendered(int)), Qt::UniqueConnection);
}

void Worker::run() {
	while (1) {
		res->requestSemaphore.acquire(1);
		if (die) {
			break;
		}

		// get page number
		res->requestMutex.lock();
		int page = res->requests.front().first;
		int width = res->requests.front().second;
		res->requests.pop_front();
		res->requestMutex.unlock();

		// check for duplicate requests
		res->imgMutex[page].lock();
		if (res->image_status[page] == width) {
			res->imgMutex[page].unlock();
			continue;
		}
		res->imgMutex[page].unlock();

		// open page
#ifdef DEBUG
		cerr << "    rendering page " << page << endl;
#endif
		Poppler::Page *p = res->doc->page(page);
		if (p == NULL) {
			cerr << "failed to load page " << page << endl;
			continue;
		}

		// render page
		float dpi = 72.0 * width / res->get_page_width(page);
		QImage *img = new QImage;
		*img = p->renderToImage(dpi, dpi);

		if (img->isNull()) {
			cerr << "failed to render page " << page << endl;
			continue;
		}

		// put page
		res->imgMutex[page].lock();
		if (res->image[page] != NULL) {
			delete res->image[page];
		}
		res->image[page] = img;
		res->image_status[page] = width;
		res->imgMutex[page].unlock();

		res->garbageMutex.lock();
		res->garbage.insert(page);
		res->garbageMutex.unlock();

		emit page_rendered(page);

		// collect goto links
		res->link_mutex.lock();
		if (res->links[page] == NULL) {
			res->link_mutex.unlock();

			list<Poppler::LinkGoto *> *lg = new list<Poppler::LinkGoto *>;
			Q_FOREACH(Poppler::Link *l, p->links()) {
				if (l->linkType() == Poppler::Link::Goto) {
					lg->push_back(static_cast<Poppler::LinkGoto *>(l));
				}
			}

			res->link_mutex.lock();
			res->links[page] = lg;
		}
		res->link_mutex.unlock();

		delete p;
	}
}


//==[ ResourceManager ]========================================================
ResourceManager::ResourceManager(QString file) {
	initialize(file);
}

void ResourceManager::initialize(QString file) {
	page_count = 0;
	worker = NULL;
	doc = Poppler::Document::load(file);
	if (doc == NULL) {
		// poppler already prints a debug message
//		cerr << "failed to open file" << endl;
		return;
	}
	if (doc->isLocked()) {
		cerr << "missing password" << endl;
		delete doc;
		doc = NULL;
		return;
	}
	doc->setRenderHint(Poppler::Document::Antialiasing, true);
	doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
	doc->setRenderHint(Poppler::Document::TextHinting, true);
//	if (POPPLER_CHECK_VERSION(0, 18, 0)) { // TODO is there a working macro?
		doc->setRenderHint(Poppler::Document::TextSlightHinting, true);
//	}

	page_count = doc->numPages();
	page_width = new float[get_page_count()];
	page_height = new float[get_page_count()];
	for (int i = 0; i < get_page_count(); i++) {
		Poppler::Page *p = doc->page(i);
		if (p == NULL) {
			cerr << "failed to load page " << i << endl;
			continue;
		}
		page_width[i] = p->pageSizeF().width();
		page_height[i] = p->pageSizeF().height();
		delete p;
	}

	image = new QImage *[get_page_count()];
	memset(image, 0, get_page_count() * sizeof(QImage *));
	image_status = new int[get_page_count()];

	imgMutex = new QMutex[get_page_count()];

	links = new list<Poppler::LinkGoto *> *[get_page_count()];
	memset(links, 0, get_page_count() * sizeof(list<Poppler::LinkGoto *> *));

	worker = new Worker();
	worker->setResManager(this);
	worker->start();
}

ResourceManager::~ResourceManager() {
	shutdown();
}

void ResourceManager::shutdown() {
	if (worker != NULL) {
		join_threads();
	}
	garbageMutex.lock();
	for (set<int>::iterator it = garbage.begin(); it != garbage.end(); ++it) {
		int page = *it;
		imgMutex[page].lock();
		delete image[page];
		image[page] = NULL;
		image_status[page] = 0;
		imgMutex[page].unlock();
	}
	garbage.clear();
	garbageMutex.unlock();
	requests.clear();
	requestSemaphore.acquire(requestSemaphore.available());
	if (doc == NULL) {
		return;
	}
	delete doc;
	for (int i = 0; i < get_page_count(); i++) {
		delete links[i];
	}
	delete[] links;
	delete[] imgMutex;
	delete[] image;
	delete[] image_status;
	delete[] page_width;
	delete[] page_height;
	delete worker;
}

void ResourceManager::load(QString file) {
	shutdown();
	initialize(file);
}

bool ResourceManager::is_valid() const {
	return (doc != NULL);
}

void ResourceManager::connect_canvas(Canvas *c) const {
	worker->connect_signal(c);
}

QImage *ResourceManager::get_page(int page, int width) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}

	// is page available?
	imgMutex[page].lock();
	if (image[page] != NULL) {
		// rerender if image has wrong size
		if (image[page]->width() != width) {
			enqueue(page, width);
		}
		// the page is still locked
		// draw it, then call unlock_page
		return image[page];
	}
	image_status[page] = 0;
	imgMutex[page].unlock();

	enqueue(page, width);

	return NULL;
}

void ResourceManager::collect_garbage(int keep_min, int keep_max) {
	// free distant pages
	garbageMutex.lock();
	for (set<int>::iterator it = garbage.begin(); it != garbage.end(); /* empty */) {
		int page = *it;
		if (page >= keep_min && page <= keep_max) {
			++it; // move on
			continue;
		}
		garbage.erase(it++); // erase and move on (iterator becomes invalid)
#ifdef DEBUG
		cerr << "    removing page " << page << endl;
#endif
		imgMutex[page].lock();
		delete image[page];
		image[page] = NULL;
		image_status[page] = 0;
		imgMutex[page].unlock();
	}
	garbageMutex.unlock();

	// keep the request list small
	if (keep_max < keep_min) {
		return;
	}
	requestMutex.lock();
	while ((int) requests.size() > keep_max - keep_min) {
		requestSemaphore.acquire(1);
		requests.pop_front();
	}
	requestMutex.unlock();
}

void ResourceManager::unlock_page(int page) const {
	imgMutex[page].unlock();
}

void ResourceManager::enqueue(int page, int width) {
	requestMutex.lock();
	// TODO might be slow
	for (list<pair<int,int> >::iterator it = requests.begin(); it != requests.end(); ++it) {
		if (it->first == page) {
			requests.erase(it);
			requestSemaphore.acquire(1);
			break;
		}
	}
	requests.push_back(make_pair(page, width));
	requestSemaphore.release(1);
	requestMutex.unlock();
}

float ResourceManager::get_page_width(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	return page_width[page];
}

float ResourceManager::get_page_height(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	return page_height[page];
}

float ResourceManager::get_page_aspect(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	return page_width[page] / page_height[page];
}

int ResourceManager::get_page_count() const {
	return page_count;
}

const list<Poppler::LinkGoto *> *ResourceManager::get_links(int page) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}
	link_mutex.lock();
	list<Poppler::LinkGoto *> *l = links[page];
	link_mutex.unlock();
	return l;
}

void ResourceManager::join_threads() {
	worker->die = true;
	requestSemaphore.release(1);
	worker->wait();
}

