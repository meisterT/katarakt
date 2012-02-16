#include "resourcemanager.h"

using namespace std;


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
/*		res->imgMutex.lock();
		if (res->image[page] != NULL) {
			res->imgMutex.unlock();
			continue;
		}
		res->imgMutex.unlock(); */

		// open page
		cout << "    rendering page " << page << endl;
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
		// TODO this might be slow
		for (list<int>::iterator it = res->garbage.begin(); it != res->garbage.end(); ++it) {
			if (*it == page) {
				res->garbage.erase(it);
				break;
			}
		}
		res->garbage.push_back(page);
		res->garbageMutex.unlock();
		res->callback(res->caller);
	}
}


//==[ ResourceManager ]========================================================
ResourceManager::ResourceManager(QString file) :
		cache_size(5) {
	doc = Poppler::Document::load(file);
	if (doc == NULL) {
		cerr << "failed to open file" << endl;
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

	worker.setResManager(this);
	worker.start();
}

ResourceManager::~ResourceManager() {
	garbageMutex.lock();
	while (!garbage.empty()) {
		int page = garbage.front();
		garbage.pop_front();
		garbageMutex.unlock();

		imgMutex[page].lock();
		delete image[page];
		image[page] = NULL;
		imgMutex[page].unlock();

		garbageMutex.lock();
	}
	garbageMutex.unlock();
	if (doc == NULL) {
		return;
	}
	delete doc;
	delete[] imgMutex;
	delete[] image;
	delete[] image_status;
	delete[] page_width;
	delete[] page_height;
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

	// "garbage collection" - keep only the cache_size newest pages
	// TODO also call when the page is available, but has to be scaled
	// TODO selection is bad
	garbageMutex.lock();
	while (garbage.size() > cache_size) {
		int page = garbage.front();
		garbage.pop_front();
		garbageMutex.unlock();

		cout << "    removing page " << page << endl;
		imgMutex[page].lock();
		delete image[page];
		image[page] = NULL;
		image_status[page] = 0;
		imgMutex[page].unlock();

		garbageMutex.lock();
	}
	garbageMutex.unlock();

	return NULL;
}

void ResourceManager::unlock_page(int page) const {
	imgMutex[page].unlock();
}

void ResourceManager::set_cache_size(unsigned int size) {
	cache_size = size;
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
	// TODO overthink this
	if (requests.size() > cache_size) {
		requests.pop_front();
	} else {
		requestSemaphore.release(1);
	}
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
	worker.die = true;
	requestSemaphore.release(1);
	worker.wait();
}

