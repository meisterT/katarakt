#include <iostream>
#include <limits>
#include <cerrno>
#include <unistd.h>
#include <QSocketNotifier>
#include <QFileInfo>
#ifdef __linux__
#include <sys/inotify.h>
#endif
#include "resourcemanager.h"
#include "canvas.h"
#include "config.h"
#include "util.h"
#include "kpage.h"
#include "worker.h"
#include "viewer.h"
#include "beamerwindow.h"
#include "layout/layout.h"

using namespace std;


ResourceManager::ResourceManager(const QString &file, Viewer *v) :
		viewer(v),
		file(file),
		doc(NULL),
		center_page(0),
		rotation(0),
#ifdef __linux__
		i_notifier(NULL),
#endif
		inverted_colors(false),
		cur_jump_pos(jumplist.end()) {
	// load config options
	CFG *config = CFG::get_instance();
	smooth_downscaling = config->get_value("smooth_downscaling").toBool();
	thumbnail_size = config->get_value("thumbnail_size").toInt();

	initialize(file, QByteArray());
}

void ResourceManager::initialize(const QString &file, const QByteArray &password) {
	page_count = 0;
	k_page = NULL;

	doc = Poppler::Document::load(file, QByteArray(), password);

	worker = new Worker(this);
	if (viewer->get_canvas() != NULL) {
		// on first start the canvas has not yet been constructed
		connect(worker, SIGNAL(page_rendered(int)), viewer->get_canvas(), SLOT(page_rendered(int)), Qt::UniqueConnection);
		connect(worker, SIGNAL(page_rendered(int)), viewer->get_beamer(), SLOT(page_rendered(int)), Qt::UniqueConnection);
	}
	worker->start();

	// setup inotify
#ifdef __linux__
	QFileInfo info(file);
	inotify_fd = inotify_init();
	if (inotify_fd == -1) {
		cerr << "inotify_init: " << strerror(errno) << endl;
	} else {
		i_notifier = new QSocketNotifier(inotify_fd, QSocketNotifier::Read, this);
		connect(i_notifier, SIGNAL(activated(int)), this, SLOT(inotify_slot()),
				Qt::UniqueConnection);

		inotify_wd  = inotify_add_watch(inotify_fd, info.path().toUtf8().constData(), IN_CLOSE_WRITE | IN_MOVED_TO);
		if (inotify_wd == -1) {
			cerr << "inotify_add_watch: " << strerror(errno) << endl;
		}
	}
#endif

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
#ifdef __linux__
	::close(inotify_fd);
	delete i_notifier;
	i_notifier = NULL;
#endif
	delete doc;
	delete[] k_page;
	delete worker;
}

void ResourceManager::load(const QString &file, const QByteArray &password) {
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

const QString &ResourceManager::get_file() const {
	return file;
}

void ResourceManager::set_file(const QString &new_file) {
	file = new_file;
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
	if (inverted_colors != k_page[page].inverted_colors) {
		k_page[page].inverted_colors = inverted_colors;
		for (int i = 0; i < 3; i++) {
			k_page[page].img[i].invertPixels();
		}
		k_page[page].thumbnail.invertPixels();
	}
	return &k_page[page];
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

void ResourceManager::invert_colors() {
	inverted_colors = !inverted_colors;
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
//					k_page[page].inverted_colors = inverted_colors;
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

void ResourceManager::connect_canvas() const {
	connect(worker, SIGNAL(page_rendered(int)), viewer->get_canvas(), SLOT(page_rendered(int)), Qt::UniqueConnection);
	connect(worker, SIGNAL(page_rendered(int)), viewer->get_beamer(), SLOT(page_rendered(int)), Qt::UniqueConnection);
}

void ResourceManager::store_jump(int page) {
	map<int,list<int>::iterator>::iterator it = jump_map.find(page);
	if (it != jump_map.end()) {
		jumplist.erase(it->second);
	}
	jumplist.push_back(page);
	jump_map[page] = --jumplist.end();
	cur_jump_pos = jumplist.end();

//	cerr << "jumplist: ";
//	for (list<int>::iterator it = jumplist.begin(); it != jumplist.end(); ++it) {
//		if (it == cur_jump_pos) {
//			cerr << "*";
//		}
//		cerr << *it << " ";
//	}
//	cerr << endl;
}

void ResourceManager::clear_jumps() {
	jumplist.clear();
	jump_map.clear();
	cur_jump_pos = jumplist.end();
}

int ResourceManager::jump_back() {
	if (cur_jump_pos == jumplist.begin()) {
		return -1;
	}
	if (cur_jump_pos == jumplist.end()) {
		store_jump(viewer->get_canvas()->get_layout()->get_page());
		--cur_jump_pos;
	}
	--cur_jump_pos;
	return *cur_jump_pos;
}

int ResourceManager::jump_forward() {
	if (cur_jump_pos == jumplist.end() || cur_jump_pos == --jumplist.end()) {
		return -1;
	}
	++cur_jump_pos;
	return *cur_jump_pos;
}

Poppler::LinkDestination *ResourceManager::resolve_link_destination(const QString &name) const {
	return doc->linkDestination(name);
}

void ResourceManager::inotify_slot() {
#ifdef __linux__
	i_notifier->setEnabled(false);

	size_t event_size = sizeof(struct inotify_event) + NAME_MAX + 1;
	// take care of alignment
	struct inotify_event i_buf[event_size / sizeof(struct inotify_event) + 1]; // has at least event_size
	char *buf = reinterpret_cast<char *>(i_buf);

	ssize_t bytes = read(inotify_fd, buf, sizeof(i_buf));
	if (bytes == -1) {
		cerr << "read: " << strerror(errno) << endl;
	} else {
		ssize_t offset = 0;
		while (offset < bytes) {
			struct inotify_event *event = reinterpret_cast<struct inotify_event *>(&buf[offset]);

			QFileInfo info(file);
			if (info.fileName() == event->name) {
				viewer->reload(false); // don't clamp
				i_notifier->setEnabled(true);
				return;
			}

			offset += sizeof(struct inotify_event) + event->len;
		}
	}

	i_notifier->setEnabled(true);
#endif
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

QDomDocument *ResourceManager::get_toc() const {
	return doc->toc();
}

void ResourceManager::join_threads() {
	worker->die = true;
	requestSemaphore.release(1);
	worker->wait();
}

