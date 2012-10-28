#include <iostream>
#include <QFileInfo>
#include <QAction>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#ifdef __linux__
#include <sys/inotify.h>
#endif
#include "viewer.h"
#include "resourcemanager.h"
#include "canvas.h"
#include "search.h"
#include "config.h"

using namespace std;


int Viewer::sig_fd[2];

Viewer::Viewer(QString _file, QWidget *parent) :
		QWidget(parent),
		file(_file),
		res(NULL),
		canvas(NULL),
		search_bar(NULL),
		layout(NULL),
		sig_notifier(NULL),
#ifdef __linux__
		i_notifier(NULL),
#endif
		valid(true) {
	res = new ResourceManager(file);
	if (!res->is_valid()) {
		valid = false;
		return;
	}

	canvas = new Canvas(this, this);
	if (!canvas->is_valid()) {
		valid = false;
		return;
	}
	res->connect_canvas(canvas);

	search_bar = new SearchBar(file, this, this);
	if (!search_bar->is_valid()) {
		valid = false;
		return;
	}
	search_bar->connect_canvas(canvas);

	// setup signal handling
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sig_fd) == -1) {
		cerr << "socketpair: " << strerror(errno) << endl;
		valid = false;
		return;
	}
	sig_notifier = new QSocketNotifier(sig_fd[1], QSocketNotifier::Read, this);
	connect(sig_notifier, SIGNAL(activated(int)), this, SLOT(signal_slot()),
			Qt::UniqueConnection);

	struct sigaction usr;
	usr.sa_handler = Viewer::signal_handler;
	sigemptyset(&usr.sa_mask);
	usr.sa_flags = SA_RESTART;

	if (sigaction(SIGUSR1, &usr, 0) > 0) {
		cerr << "sigaction: " << strerror(errno) << endl;
		valid = false;
		return;
	}

	// setup keys
	add_action("toggle_fullscreen", SLOT(toggle_fullscreen()));
	add_action("close_search", SLOT(close_search()));
	add_action("reload", SLOT(reload()));

	layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(canvas);
	layout->addWidget(search_bar);
	setLayout(layout);

	QFileInfo info(file);
	// setup inotify
#ifdef __linux__
	inotify_fd = inotify_init();
	if (inotify_fd == -1) {
		cerr << "inotify_init: " << strerror(errno) << endl;
	} else {
		inotify_wd  = inotify_add_watch(inotify_fd, info.path().toUtf8().constData(), IN_CLOSE_WRITE | IN_MOVED_TO);
		if (inotify_wd == -1) {
			cerr << "inotify_add_watch: " << strerror(errno) << endl;
		} else {
			i_notifier = new QSocketNotifier(inotify_fd, QSocketNotifier::Read, this);
			connect(i_notifier, SIGNAL(activated(int)), this, SLOT(inotify_slot()),
					Qt::UniqueConnection);
		}
	}
#endif

	setWindowTitle(QString::fromUtf8("%1 \u2014 katarakt").arg(info.fileName()));
	setMinimumSize(50, 50);
	resize(500, 500);
	show();
	search_bar->hide();

	// apply start options
	if (CFG::get_instance()->get_value("fullscreen").toBool()) {
		toggle_fullscreen();
	}
}

Viewer::~Viewer() {
#ifdef __linux__
	::close(inotify_fd);
	delete i_notifier;
#endif
	::close(sig_fd[0]);
	::close(sig_fd[1]);
	delete sig_notifier;
	delete layout;
	delete search_bar;
	delete canvas;
	delete res;
}

bool Viewer::is_valid() const {
	return valid;
}

void Viewer::focus_search() {
	search_bar->focus();
}

void Viewer::reload() {
#ifdef DEBUG
	cerr << "reloading file " << file.toUtf8().constData() << endl;
#endif

	res->load(file);
	// do not connect non-existing worker to canvas
	if (res->is_valid()) {
		res->connect_canvas(canvas);
	}

	search_bar->load(file);

	canvas->reload();
}

ResourceManager *Viewer::get_res() const {
	return res;
}

Canvas *Viewer::get_canvas() const {
	return canvas;
}

void Viewer::signal_slot() {
	sig_notifier->setEnabled(false);
	char tmp;
	if (read(sig_fd[1], &tmp, sizeof(char)) < 0) {
		cerr << "read: " << strerror(errno) << endl;
	}

	reload();

	sig_notifier->setEnabled(true);
}

void Viewer::signal_handler(int /*unused*/) {
	char tmp = '1';
	if (write(sig_fd[0], &tmp, sizeof(char)) < 0) {
		cerr << "write: " << strerror(errno) << endl;
	}
}

void Viewer::inotify_slot() {
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
				reload();
				i_notifier->setEnabled(true);
				return;
			}

			offset += sizeof(struct inotify_event) + event->len;
		}
	}

	i_notifier->setEnabled(true);
#endif
}

void Viewer::toggle_fullscreen() {
	setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void Viewer::close_search() {
	canvas->setFocus(Qt::OtherFocusReason);
	search_bar->hide();
}

void Viewer::add_action(const char *action, const char *slot) {
	QStringListIterator i(CFG::get_instance()->get_keys(action));
	while (i.hasNext()) {
		QAction *a = new QAction(this);
		a->setShortcut(QKeySequence(i.next()));
		addAction(a);
		connect(a, SIGNAL(triggered()), this, slot);
	}
}

