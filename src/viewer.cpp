#include <iostream>
#include <QFileInfo>
#include <QAction>
#include <QFileDialog>
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
		// the command line option toggles the value set in the config
		if (CFG::get_instance()->get_value("quit_on_init_fail").toBool() !=
				CFG::get_instance()->has_tmp_value("quit_on_init_fail")) {
			valid = false;
			return;
		}
	}

	canvas = new Canvas(this, this);
	if (!canvas->is_valid()) {
		valid = false;
		return;
	}
	res->connect_canvas(canvas);

	search_bar = new SearchBar(file, this, this);
	if (!search_bar->is_valid()) {
		if (CFG::get_instance()->get_value("quit_on_init_fail").toBool() !=
				CFG::get_instance()->has_tmp_value("quit_on_init_fail")) {
			valid = false;
			return;
		}
	}

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
	add_action("open", SLOT(open()));

	layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// initialize info bar
	QIcon::setThemeName(CFG::get_instance()->get_value("icon_theme").toString());

	info_label_icon.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
	info_password.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	info_widget.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	info_label_text.setWordWrap(true);
	info_label_text.setTextFormat(Qt::PlainText);
	info_password.setEchoMode(QLineEdit::Password);

	info_layout.addWidget(&info_label_icon);
	info_layout.addWidget(&info_label_text);
	info_layout.addWidget(&info_password);
	info_widget.setLayout(&info_layout);

	layout->addWidget(&info_widget);
	connect(&info_password, SIGNAL(returnPressed()), this, SLOT(reload()),
			Qt::UniqueConnection);

	update_info_widget();


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
	info_password.setFocus(Qt::OtherFocusReason); // only works if shown

	// apply start options
	if (CFG::get_instance()->get_tmp_value("fullscreen").toBool()) {
		toggle_fullscreen();
	}

	// enable transparency, but only in the right places
	setAttribute(Qt::WA_TranslucentBackground);
	search_bar->setAutoFillBackground(true);
	info_widget.setAutoFillBackground(true);
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

void Viewer::reload(bool clamp) {
#ifdef DEBUG
	cerr << "reloading file " << file.toUtf8().constData() << endl;
#endif

	res->load(file, info_password.text().toLatin1());
	res->connect_canvas(canvas);

	search_bar->reset_search(); // TODO restart search if loading the same document?
	search_bar->load(file, info_password.text().toLatin1());

	update_info_widget();

	canvas->reload(clamp);
}

void Viewer::open() {
	QString new_file = QFileDialog::getOpenFileName(this, "Open File", "", "PDF Files (*.pdf)");
	if (!new_file.isNull()) {
		file = new_file;
		QFileInfo info(file);

		// re-setup inotify watch
#ifdef __linux__
		if (inotify_fd != -1) {
			inotify_rm_watch(inotify_fd, inotify_wd);

			inotify_wd  = inotify_add_watch(inotify_fd, info.path().toUtf8().constData(), IN_CLOSE_WRITE | IN_MOVED_TO);
			if (inotify_wd == -1) {
				cerr << "inotify_add_watch: " << strerror(errno) << endl;
			}
		}
#endif

		// different file - clear jumplist
		// e.g. in inotify-caused reload it doesn't hurt to keep the old jumplist
		// search is always cleared, see reload()
		canvas->clear_jumps();

		setWindowTitle(QString::fromUtf8("%1 \u2014 katarakt").arg(info.fileName()));
		reload();
	}
}

void Viewer::update_info_widget() {
	if (!res->is_valid() || !search_bar->is_valid()) {
		QIcon icon;

		if (file == "") {
			icon = QIcon::fromTheme("dialog-information");

			info_label_text.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			info_label_text.setText("No file opened.");
			info_password.hide();
		} else if (!res->is_locked()) {
			icon = QIcon::fromTheme("dialog-error");

			info_label_text.setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			info_label_text.setText(
				QString("Failed to open file '") + file + QString("'."));

			info_password.hide();
		} else {
			icon = QIcon::fromTheme("dialog-password");

			info_label_text.setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
			info_label_text.setText("password:");

			info_password.show();
			info_password.setFocus(Qt::OtherFocusReason);
			info_password.clear();
		}
		info_label_icon.setPixmap(icon.pixmap(32, 32));
		info_widget.show();
	} else {
		info_widget.hide();
	}
}

ResourceManager *Viewer::get_res() const {
	return res;
}

Canvas *Viewer::get_canvas() const {
	return canvas;
}

SearchBar *Viewer::get_search_bar() const {
	return search_bar;
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
				reload(false); // don't clamp
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

