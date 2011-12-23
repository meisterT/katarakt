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
		res->callback(res->caller);
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
	delete doc;
	delete image;
	delete pageAspect;
}

bool ResourceManager::isNull() const {
	return (doc == NULL);
}

void ResourceManager::setFinishedCallback(void (*_callback)(PdfViewer *), PdfViewer *arg) {
	callback = _callback;
	caller = arg;
}

QImage *ResourceManager::getPage(int page, int newWidth) {
	if (page < 0 || page >= doc->numPages()) {
		return NULL;
	}

	// new image size, old images are obsolete
	if (newWidth != width) {
		cout << "width changed from " << width << " to " << newWidth << endl;
		width = newWidth;

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

	imgMutex.lock();
	if (image[page] != NULL) {
		QImage *tmp = image[page];
		imgMutex.unlock();
		return tmp;
	}
	imgMutex.unlock();

	requestMutex.lock();
	requests.push(page);
	if (requests.size() > 5) {
		requests.pop();
	} else {
		requestSemaphore.release(1);
	}
	requestMutex.unlock();

	// "garbage collection" - keep only the 5 newest pages
	garbageMutex.lock();
	while (garbage.size() > 5) {
		int page = garbage.front();
		garbage.pop();
		garbageMutex.unlock();

		cout << "    removing page " << page << endl;
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
		return -1;
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

