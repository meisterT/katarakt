#include "resourcemanager.h"

using namespace std;


#define DEBUG
//#undef DEBUG


//==[ Worker ]=================================================================
Worker::Worker() :
		die(false) {
}

void Worker::setResManager(ResourceManager *rm) {
	res = rm;
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
		delete p;

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

		res->callback(res->caller);
	}
}


//==[ ResourceManager ]========================================================
ResourceManager::ResourceManager(QString _file) :
		worker(NULL),
		file(_file) {
	initialize();
}

void ResourceManager::initialize() {
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
//	if (POPPLER_CHECK_VERSION(0, 18, 0)) {
		doc->setRenderHint(Poppler::Document::TextSlightHinting, true);
//	}

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
	if (doc == NULL) {
		return;
	}
	delete doc;
	delete[] imgMutex;
	delete[] image;
	delete[] image_status;
	delete[] page_width;
	delete[] page_height;
	delete worker;
}

void ResourceManager::reload_document() {
	shutdown();
	requests.clear();
#ifdef DEBUG
	cerr << "reloading file " << file.toUtf8().constData() << endl;
#endif
	requestSemaphore.acquire(requestSemaphore.available());
	initialize();
}

bool ResourceManager::is_null() const {
	return (doc == NULL);
}

void ResourceManager::setFinishedCallback(void (*_callback)(Viewer *), Viewer *arg) {
	callback = _callback;
	caller = arg;
}

QImage *ResourceManager::get_page(int page, int width) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}

	// is page available?
	imgMutex[page].lock();
	if (image[page] != NULL) {
		QImage *img = image[page];

		// scale image
		if (img->width() != width) {
			QImage *tmp = new QImage;
			*tmp = img->scaledToWidth(width);
			delete img;

			image[page] = tmp;
			img = tmp;
			image_status[page] = -width;

			enqueue(page, width);
		// redo dropped requests because of fast scrolling
		// this does not really apply any more
		} else if (image_status[page] == -width) {
			enqueue(page, width);
		}
		// the page is still locked
		// draw it, then call unlock_page
		return img;
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
	return doc->numPages();
}

void ResourceManager::join_threads() {
	worker->die = true;
	requestSemaphore.release(1);
	worker->wait();
}

