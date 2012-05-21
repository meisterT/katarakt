#include "viewer.h"
#include "resourcemanager.h"
#include "canvas.h"
#include "search.h"
#include <iostream>
#include <QFileInfo>
#include <csignal>
#include <cerrno>
#include <unistd.h>

using namespace std;


int Viewer::sig_fd[2];

//Viewer::Viewer(ResourceManager *res, QWidget *parent) :
Viewer::Viewer(QString _file, QWidget *parent) :
		QWidget(parent),
		file(_file),
		res(NULL),
		canvas(NULL),
		search_bar(NULL),
		layout(NULL),
		sig_notifier(NULL),
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

	search_bar = new SearchBar(file, this);
	connect(search_bar, SIGNAL(search_clear()), canvas, SLOT(search_clear()), Qt::UniqueConnection);
	connect(search_bar, SIGNAL(search_done(int, std::list<Result> *)),
			canvas, SLOT(search_done(int, std::list<Result> *)), Qt::UniqueConnection);

	// setup signal handling
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sig_fd) == -1) {
		cerr << "socketpair: " << strerror(errno) << endl;
		valid = false;
		return;
	}
	sig_notifier = new QSocketNotifier(sig_fd[1], QSocketNotifier::Read, this);
	connect(sig_notifier, SIGNAL(activated(int)), this, SLOT(signal_slot()));

	struct sigaction usr;
	usr.sa_handler = Viewer::signal_handler;
	sigemptyset(&usr.sa_mask);
	usr.sa_flags = SA_RESTART;

	if (sigaction(SIGUSR1, &usr, 0) > 0) {
		cerr << "sigaction: " << strerror(errno) << endl;
		valid = false;
		return;
	}

	// TODO these sequences conflict between widgets
	add_sequence("F", &Viewer::toggle_fullscreen);
	add_sequence("Esc", &Viewer::close_search);
	add_sequence("R", &Viewer::reload);

	layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(canvas);
	layout->addWidget(search_bar);
	setLayout(layout);

	QFileInfo info(file);
	setWindowTitle(QString::fromUtf8("%1 \u2014 katarakt").arg(info.fileName()));
	setMinimumSize(50, 50);
	resize(500, 500);
	show();
	search_bar->hide();
}

Viewer::~Viewer() {
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
	search_bar->setFocus(Qt::OtherFocusReason);
	search_bar->selectAll();
	search_bar->show();
}

void Viewer::reload() {
#ifdef DEBUG
	cerr << "reloading file " << file.toUtf8().constData() << endl;
#endif
	// TODO not nice
	delete search_bar;
	search_bar = new SearchBar(file, this);
	connect(search_bar, SIGNAL(search_clear()), canvas, SLOT(search_clear()), Qt::UniqueConnection);
	connect(search_bar, SIGNAL(search_done(int, std::list<Result> *)),
			canvas, SLOT(search_done(int, std::list<Result> *)), Qt::UniqueConnection);
	layout->addWidget(search_bar);
	search_bar->hide();

	res->load(file);
	res->connect_canvas(canvas);
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

void Viewer::toggle_fullscreen() {
	setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void Viewer::close_search() {
	canvas->setFocus(Qt::OtherFocusReason);
	search_bar->hide();
}

bool Viewer::event(QEvent *event) {
	if (event->type() == QEvent::Shortcut) {
		QShortcutEvent *s = static_cast<QShortcutEvent*>(event);
		if (sequences.find(s->key()) != sequences.end()) {
			(this->*sequences[s->key()])();
		}
		return true;
	}
	return QWidget::event(event);
}

void Viewer::add_sequence(QString key, func_t action) {
	QKeySequence s(key);
	sequences[s] = action;
	grabShortcut(s, Qt::WindowShortcut);
}
