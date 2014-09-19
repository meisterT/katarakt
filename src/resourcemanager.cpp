#include <iostream>
#include <limits>
#include "resourcemanager.h"
#include "canvas.h"
#include "config.h"
#include "util.h"

using namespace std;


//==[ Katarakt Page ]==========================================================
KPage::KPage() :
		links(NULL) {
	for (int i = 0; i < 3; i++) {
		status[i] = 0;
		rotation[i] = 0;
	}
}

KPage::~KPage() {
	if (links != NULL) {
		for (list<Poppler::LinkGoto *>::iterator it = links->begin();
				it != links->end(); ++it) {
			delete *it;
		}
	}
	delete links;
}

const QImage *KPage::get_image(int index) const {
	// return any available image, try the right index first
	for (int i = 3; i > 0; i--) {
		if (!img[(index + i) % 3].isNull()) {
			return &img[(index + i) % 3];
		}
	}
	if (thumbnail.isNull()) {
		return NULL;
	} else {
		return &thumbnail;
	}
}

int KPage::get_width(int index) const {
	return status[index];
}

char KPage::get_rotation(int index) const {
	return rotation[index];
}

//QString KPage::get_label() const {
//	return label;
//}


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
		int page, width, index;
		map<int,pair<int,int> >::iterator less = res->requests.lower_bound(res->center_page);
		map<int,pair<int,int> >::iterator greater = less--;

		if (greater != res->requests.end()) {
			if (greater != res->requests.begin()) {
				// favour nearby page, go down first
				if (greater->first + less->first <= res->center_page * 2) {
					page = greater->first;
					index = greater->second.first;
					width = greater->second.second;
					res->requests.erase(greater);
				} else {
					page = less->first;
					index = less->second.first;
					width = less->second.second;
					res->requests.erase(less);
				}
			} else {
				page = greater->first;
				index = greater->second.first;
				width = greater->second.second;
				res->requests.erase(greater);
			}
		} else {
			page = less->first;
			index = less->second.first;
			width = less->second.second;
			res->requests.erase(less);
		}
		res->requestMutex.unlock();

		// check for duplicate requests
		res->k_page[page].mutex.lock();
		if (res->k_page[page].status[index] == width &&
				res->k_page[page].rotation[index] == res->rotation) {
			res->k_page[page].mutex.unlock();
			continue;
		}
		int rotation = res->rotation;
		res->k_page[page].mutex.unlock();

		// open page
#ifdef DEBUG
		cerr << "    rendering page " << page << " for index " << index<< endl;
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
		if (!res->k_page[page].img[index].isNull()) {
			res->k_page[page].img[index] = QImage(); // assign null image
		}
		res->k_page[page].img[index] = img;
		res->k_page[page].status[index] = width;
		res->k_page[page].rotation[index] = rotation;
		res->k_page[page].mutex.unlock();

		res->garbageMutex.lock();
		res->garbage.insert(page); // TODO add index information?
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
				} else {
					delete l;
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
		doc(NULL),
		center_page(0),
		rotation(0) {
	// load config options
	CFG *config = CFG::get_instance();
	smooth_downscaling = config->get_value("smooth_downscaling").toBool();
	thumbnail_size = config->get_value("thumbnail_size").toInt();

	initialize(file, QByteArray());
}

void ResourceManager::initialize(QString &file, const QByteArray &password) {
	page_count = 0;
	k_page = NULL;

	doc = Poppler::Document::load(file, QByteArray(), password);

	worker = new Worker();
	worker->setResManager(this);
	worker->start();

	if (doc == NULL) {
		// poppler already prints a debug message
//		cerr << "failed to open file" << endl;
		return;
	}
	if (doc->isLocked()) {
		// poppler already prints a debug message
//		cerr << "missing password" << endl;
		return;
	}
	doc->setRenderHint(Poppler::Document::Antialiasing, true);
	doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
	doc->setRenderHint(Poppler::Document::TextHinting, true);
#if POPPLER_VERSION >= POPPLER_VERSION_CHECK(0, 18, 0)
	doc->setRenderHint(Poppler::Document::TextSlightHinting, true);
#endif
#if POPPLER_VERSION >= POPPLER_VERSION_CHECK(0, 22, 0)
//	doc->setRenderHint(Poppler::Document::OverprintPreview, true); // TODO what is this?
#endif
#if POPPLER_VERSION >= POPPLER_VERSION_CHECK(0, 24, 0)
	doc->setRenderHint(Poppler::Document::ThinLineSolid, true); // TODO what's the difference between ThinLineSolid and ThinLineShape?
#endif

	page_count = doc->numPages();

	min_aspect = numeric_limits<float>::max();
	max_aspect = numeric_limits<float>::min();

	k_page = new KPage[get_page_count()];
	for (int i = 0; i < get_page_count(); i++) {
		Poppler::Page *p = doc->page(i);
		if (p == NULL) {
			cerr << "failed to load page " << i << endl;
			continue;
		}
		k_page[i].width = p->pageSizeF().width();
		k_page[i].height = p->pageSizeF().height();

		float aspect = k_page[i].width / k_page[i].height;
		if (aspect < min_aspect) {
			min_aspect = aspect;
		}
		if (aspect > max_aspect) {
			max_aspect = aspect;
		}

//		k_page[i].label = p->label();
//		if (k_page[i].label != QString::number(i + 1)) {
//			cout << i << endl;
//		}
		delete p;
	}
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
	delete doc;
	delete[] k_page;
	delete worker;
}

void ResourceManager::load(QString &file, const QByteArray &password) {
	shutdown();
	initialize(file, password);
}

bool ResourceManager::is_valid() const {
	return (doc != NULL);
}

bool ResourceManager::is_locked() const {
	if (doc == NULL) {
		return false;
	}
	return doc->isLocked();
}

void ResourceManager::connect_canvas(Canvas *c) const {
	worker->connect_signal(c);
}

const KPage *ResourceManager::get_page(int page, int width, int index) {
	if (page < 0 || page >= get_page_count()) {
		return NULL;
	}

	// page not available or wrong size/rotation
	k_page[page].mutex.lock();
	if (k_page[page].img[index].isNull() ||
			k_page[page].status[index] != width ||
			k_page[page].rotation[index] != rotation) {
		enqueue(page, width, index);
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
			if (smooth_downscaling) {
				mode = Qt::SmoothTransformation;
			}
			// find the index of the rendered image
			for (int i = 0; i < 3; i++) {
				if (!k_page[page].img[i].isNull()) {
					// scale
					k_page[page].thumbnail = k_page[page].img[i].scaled(
							QSize(thumbnail_size, thumbnail_size),
							Qt::IgnoreAspectRatio, mode);
					// rotate
					if (k_page[page].rotation[i] != 0) {
						QTransform trans;
						trans.rotate(-k_page[page].rotation[i] * 90);
						k_page[page].thumbnail = k_page[page].thumbnail.transformed(
								trans);
					}
					break;
				}
			}
		}
		for (int i = 0; i < 3; i++) {
			k_page[page].img[i] = QImage();
			k_page[page].status[i] = 0;
			k_page[page].rotation[i] = 0;
		}
		k_page[page].mutex.unlock();
	}
	garbageMutex.unlock();

	// keep the request list small
	if (keep_max < keep_min) {
		return;
	}
	requestMutex.lock();
	for (map<int,pair<int,int> >::iterator it = requests.begin(); it != requests.end(); ) {
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

void ResourceManager::enqueue(int page, int width, int index) {
	requestMutex.lock();
	map<int,pair<int,int> >::iterator it = requests.find(page);
	if (it == requests.end()) {
		requests[page] = make_pair(index, width);
		requestSemaphore.release(1);
	} else {
		if (index <= it->second.first) {
			it->second = make_pair(index, width);
		}
	}
	requestMutex.unlock();
}

//QString ResourceManager::get_page_label(int page) const {
//	if (page < 0 || page >= get_page_count()) {
//		return QString();
//	}
//	return k_page[page].label;
//}

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

float ResourceManager::get_min_aspect() const {
	if (rotation == 0 || rotation == 2) {
		return min_aspect;
	} else {
		return 1.0f / max_aspect;
	}
}

float ResourceManager::get_max_aspect() const {
	if (rotation == 0 || rotation == 2) {
		return max_aspect;
	} else {
		return 1.0f / min_aspect;
	}
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

