#include "resourcemanager.h"
#include "canvas.h"
#include <iostream>

using namespace std;


// TODO put in a config source file
#define SMOOTH_DOWNSCALING true // filter when creating thumbnail image
#define THUMBNAIL_SIZE 32


//==[ Katarakt Page ]==========================================================
KPage::KPage() :
		links(NULL),
		status(0),
		rotation(0) {
}

KPage::~KPage() {
	delete links;
}

const QImage *KPage::get_image() const {
	if (img.isNull()) {
		if (thumbnail.isNull()) {
			return NULL;
		}
		return &thumbnail;
	}
	return &img;
}

int KPage::get_width() const {
	return status;
}

char KPage::get_rotation() const {
	return rotation;
}


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

		// get next page to render
		res->requestMutex.lock();
		int page, width;
		map<int,int>::iterator less = res->requests.lower_bound(res->center_page);
		map<int,int>::iterator greater = less--;

		if (greater != res->requests.end()) {
			if (greater != res->requests.begin()) {
				// favour nearby page, go down first
				if (greater->first + less->first <= res->center_page * 2) {
					page = greater->first;
					width = greater->second;
					res->requests.erase(greater);
				} else {
					page = less->first;
					width = less->second;
					res->requests.erase(less);
				}
			} else {
				page = greater->first;
				width = greater->second;
				res->requests.erase(greater);
			}
		} else {
			page = less->first;
			width = less->second;
			res->requests.erase(less);
		}
		res->requestMutex.unlock();

		// check for duplicate requests
		res->k_page[page].mutex.lock();
		if (res->k_page[page].status == width &&
				res->k_page[page].rotation == res->rotation) {
			res->k_page[page].mutex.unlock();
			continue;
		}
		int rotation = res->rotation;
		res->k_page[page].mutex.unlock();

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
		QImage img = p->renderToImage(dpi, dpi, -1, -1, -1, -1,
				static_cast<Poppler::Page::Rotation>(rotation));

		if (img.isNull()) {
			cerr << "failed to render page " << page << endl;
			continue;
		}

		// put page
		res->k_page[page].mutex.lock();
		if (!res->k_page[page].img.isNull()) {
			res->k_page[page].img = QImage(); // assign null image
		}
		res->k_page[page].img = img;
		res->k_page[page].status = width;
		res->k_page[page].rotation = rotation;
		res->k_page[page].mutex.unlock();

		res->garbageMutex.lock();
		res->garbage.insert(page);
		res->garbageMutex.unlock();

		emit page_rendered(page);

		// collect goto links
		res->link_mutex.lock();
		if (res->k_page[page].links == NULL) {
			res->link_mutex.unlock();

			list<Poppler::LinkGoto *> *lg = new list<Poppler::LinkGoto *>;
			Q_FOREACH(Poppler::Link *l, p->links()) {
				if (l->linkType() == Poppler::Link::Goto) {
					lg->push_back(static_cast<Poppler::LinkGoto *>(l));
				}
			}

			res->link_mutex.lock();
			res->k_page[page].links = lg;
		}
		res->link_mutex.unlock();

		delete p;
	}
}


//==[ ResourceManager ]========================================================
ResourceManager::ResourceManager(QString file) :
		center_page(0),
		rotation(0) {
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

	k_page = new KPage[get_page_count()];
	for (int i = 0; i < get_page_count(); i++) {
		Poppler::Page *p = doc->page(i);
		if (p == NULL) {
			cerr << "failed to load page " << i << endl;
			continue;
		}
		k_page[i].width = p->pageSizeF().width();
		k_page[i].height = p->pageSizeF().height();
		delete p;
	}

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
	garbage.clear();
	garbageMutex.unlock();
	requests.clear();
	requestSemaphore.acquire(requestSemaphore.available());
	if (doc == NULL) {
		return;
	}
	delete doc;
	delete[] k_page;
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

const KPage *ResourceManager::get_page(int page, int width) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}

	// page not available or wrong size/rotation
	k_page[page].mutex.lock();
	if (k_page[page].img.isNull() ||
			k_page[page].status != width ||
			k_page[page].rotation != rotation) {
		enqueue(page, width);
	}
	return &k_page[page];
}

void ResourceManager::collect_garbage(int keep_min, int keep_max) {
	requestMutex.lock();
	center_page = (keep_min + keep_max) / 2;
	requestMutex.unlock();
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
		k_page[page].mutex.lock();
		// create thumbnail
		if (k_page[page].thumbnail.isNull()) {
			Qt::TransformationMode mode = Qt::FastTransformation;
			if (SMOOTH_DOWNSCALING) {
				mode = Qt::SmoothTransformation;
			}
			// scale
			k_page[page].thumbnail = k_page[page].img.scaled(
					QSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE),
					Qt::IgnoreAspectRatio, mode);
			// rotate
			if (k_page[page].rotation != 0) {
				QTransform trans;
				trans.rotate(-k_page[page].rotation * 90);
				k_page[page].thumbnail = k_page[page].thumbnail.transformed(
						trans);
			}
		}
		k_page[page].img = QImage();
		k_page[page].status = 0;
		k_page[page].rotation = 0;
		k_page[page].mutex.unlock();
	}
	garbageMutex.unlock();

	// keep the request list small
	if (keep_max < keep_min) {
		return;
	}
	requestMutex.lock();
	for (map<int,int>::iterator it = requests.begin(); it != requests.end(); ) {
		if (it->first < keep_min || it->first > keep_max) {
			requestSemaphore.acquire(1);
			requests.erase(it++);
		} else {
			++it;
		}
	}
	requestMutex.unlock();
}

int ResourceManager::get_rotation() const {
	return rotation;
}

void ResourceManager::rotate(int value, bool relative) {
	value %= 4;
	if (relative) {
		rotation = (rotation + value + 4) % 4;
	} else {
		rotation = value;
	}
}

void ResourceManager::unlock_page(int page) const {
	k_page[page].mutex.unlock();
}

void ResourceManager::enqueue(int page, int width) {
	requestMutex.lock();
	map<int,int>::iterator it = requests.find(page);
	if (it == requests.end()) {
		requests[page] = width;
		requestSemaphore.release(1);
	} else {
		it->second = width;
	}
	requestMutex.unlock();
}

float ResourceManager::get_page_width(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	if (rotation == 0 || rotation == 2) {
		return k_page[page].width;
	}
	// swap if rotated by 90 or 270 degrees
	return k_page[page].height;
}

float ResourceManager::get_page_height(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	if (rotation == 0 || rotation == 2) {
		return k_page[page].height;
	}
	return k_page[page].width;
}

float ResourceManager::get_page_aspect(int page) const {
	if (page < 0 || page >= get_page_count()) {
		return -1;
	}
	if (rotation == 0 || rotation == 2) {
		return k_page[page].width / k_page[page].height;
	}
	return k_page[page].height / k_page[page].width;
}

int ResourceManager::get_page_count() const {
	return page_count;
}

const list<Poppler::LinkGoto *> *ResourceManager::get_links(int page) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}
	link_mutex.lock();
	list<Poppler::LinkGoto *> *l = k_page[page].links;
	link_mutex.unlock();
	return l;
}

void ResourceManager::join_threads() {
	worker->die = true;
	requestSemaphore.release(1);
	worker->wait();
}

