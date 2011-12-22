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
		int page = res->requests.front();
		res->requests.pop();
		res->requestMutex.unlock();

		// check for duplicate requests
		res->imgMutex.lock();
		if (res->image[page] != NULL) {
			res->imgMutex.unlock();
			continue;
		}
		res->imgMutex.unlock();

		// open page
		cout << "    rendering page " << page << endl;
		Poppler::Page *p = res->doc->page(page);
		if (p == NULL) {
			cerr << "failed to load page " << page << endl;
			continue;
		}

		// render page
		float dpi = 72.0 * res->width / p->pageSizeF().width();
		QImage img = p->renderToImage(dpi, dpi);
		delete p;

		if (img.isNull()) {
			cerr << "failed to render page " << page << endl;
			continue;
		}

		// put page
		QImage *tmp = new QImage;
		*tmp = img;
		res->imgMutex.lock();
		res->image[page] = tmp;
		res->imgMutex.unlock();

		res->garbageMutex.lock();
		res->garbage.push(page);
		res->garbageMutex.unlock();
	}
}


//==[ ResourceManager ]========================================================
ResourceManager::ResourceManager(QString file) {
	doc = Poppler::Document::load(file);
	doc->setRenderHint(Poppler::Document::Antialiasing, true);
	doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
	doc->setRenderHint(Poppler::Document::TextHinting, true);
//	if (POPPLER_CHECK_VERSION(0, 18, 0)) {
		doc->setRenderHint(Poppler::Document::TextSlightHinting, true);
//	}
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

	pageAspect = new float[doc->numPages()];
	for (int i = 0; i < doc->numPages(); ++i) {
		Poppler::Page *p = doc->page(i);
		if (p == NULL) {
			cerr << "failed to load page " << i << endl;
			continue;
		}
		pageAspect[i] = p->pageSizeF().width() / p->pageSizeF().height();
		delete p;
	}

	image = new QImage *[doc->numPages()];
	memset(image, 0, doc->numPages() * sizeof(QImage *));

	worker.setResManager(this);
	worker.start();
}

ResourceManager::~ResourceManager() {
	delete doc;
	delete image; // TODO free single pages
	delete pageAspect;
}

bool ResourceManager::isNull() const {
	return (doc == NULL);
}

QImage *ResourceManager::getPage(int page, int newWidth) {
	if (newWidth != width) {
		garbageMutex.lock();
		while (!garbage.empty()) {
			int page = garbage.front();
			garbage.pop();
			garbageMutex.unlock();

			imgMutex.lock();
			delete image[page];
			image[page] = NULL;
			imgMutex.unlock();

			garbageMutex.lock();
		}
		garbageMutex.unlock();
	}

	width = newWidth;
	if (page < 0 || page >= doc->numPages()) {
		return NULL;
	}

	imgMutex.lock();
	if (image[page] != NULL) {
		QImage *tmp = image[page];
		imgMutex.unlock();
		return tmp;
	}
	imgMutex.unlock();

	requestMutex.lock();
	requests.push(page);
	requestSemaphore.release(1);
	requestMutex.unlock();

	// "garbage collection" - keep only the 5 newest pages
	garbageMutex.lock();
	cout << "currently rendered images: " << garbage.size() << endl;
	while (garbage.size() > 5) {
		int page = garbage.front();
		garbage.pop();
		garbageMutex.unlock();

		imgMutex.lock();
		delete image[page];
		image[page] = NULL;
		imgMutex.unlock();

		garbageMutex.lock();
	}
	garbageMutex.unlock();

	return NULL;
}

float ResourceManager::getPageAspect(int page) const {
	if (page < 0 || page >= doc->numPages()) {
		return 1;
	}
	return pageAspect[page];
}

int ResourceManager::numPages() const {
	return doc->numPages();
}

void ResourceManager::wait() {
	worker.die = true;
	requestSemaphore.release(1);
	worker.wait();
}

