#include <iostream>
#include <QCoreApplication>
#include <QFileInfo>
#include <QAction>
#include <QFileDialog>
#include <QPrinter>
#include <QProcess>
#include <QPrintDialog>
#include <csignal>
#include <cerrno>
#include <unistd.h>
#include "viewer.h"
#include "resourcemanager.h"
#include "canvas.h"
#include "search.h"
#include "config.h"
#include "layout/layout.h"
#include "beamerwindow.h"
#include "toc.h"
#include "splitter.h"
#include "util.h"

using namespace std;


int Viewer::sig_fd[2];

Viewer::Viewer(const QString &file, QWidget *parent) :
		QWidget(parent),
		res(NULL),
		splitter(NULL),
		toc(NULL),
		canvas(NULL),
		search_bar(NULL),
		layout(NULL),
		sig_notifier(NULL),
		beamer(NULL),
		valid(true) {
	res = new ResourceManager(file, this);
	if (!res->is_valid()) {
		if (CFG::get_instance()->get_most_current_value("Settings/quit_on_init_fail").toBool()) {
			valid = false;
			return;
		}
	}

	search_bar = new SearchBar(file, this, this);
	if (!search_bar->is_valid()) {
		if (CFG::get_instance()->get_most_current_value("Settings/quit_on_init_fail").toBool()) {
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

	setup_keys(this);
	beamer = new BeamerWindow(this);
	setup_keys(beamer);

	splitter = new Splitter(this);
	toc = new Toc(this, splitter);

	canvas = new Canvas(this, splitter); // beamer must already exist
	if (!canvas->is_valid()) {
		valid = false;
		return;
	}
	res->connect_canvas();
	canvas->update_page_overlay(); // set initial position

	splitter->addWidget(toc);
	splitter->addWidget(canvas);
	splitter->setSizes(QList<int>() << (width() / 4) << (width() * 3 / 4)); // TODO config option

	toc->hide();

	// load config options
	CFG *config = CFG::get_instance();
	smooth_scroll_delta = config->get_value("Settings/smooth_scroll_delta").toInt();
	screen_scroll_factor = config->get_value("Settings/screen_scroll_factor").toFloat();

	layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	// initialize info bar
	QIcon::setThemeName(CFG::get_instance()->get_value("Settings/icon_theme").toString());

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

	presenter_progress.setMinimum(1);
 	presenter_progress.setMaximum(res->get_page_count());
 	presenter_progress.setValue(canvas->get_layout()->get_page() + 1);
//	presenter_progress.setFormat("%v/%m");
 	presenter_progress.setTextVisible(true);

	layout->addWidget(splitter);
	layout->addWidget(&presenter_progress);
	layout->addWidget(search_bar);
	setLayout(layout);

	QFileInfo info(res->get_file());
	setWindowTitle(QString::fromUtf8("%1 \u2014 katarakt").arg(info.fileName()));
	setMinimumSize(50, 50);
	resize(500, 500);

	search_bar->hide();
	info_password.setFocus(Qt::OtherFocusReason); // only works if shown

	// apply start options
	if (res->is_valid()) {
		canvas->get_layout()->scroll_page(config->get_tmp_value("start_page").toInt(), false);
	}
	if (CFG::get_instance()->get_tmp_value("fullscreen").toBool()) {
		toggle_fullscreen();
	}

	// enable transparency, but only in the right places
	setAttribute(Qt::WA_TranslucentBackground);
	presenter_progress.setAutoFillBackground(true);
	search_bar->setAutoFillBackground(true);
	info_widget.setAutoFillBackground(true);
	toc->setAutoFillBackground(true);
	// the splitter handle has to be opaque, canvas has to be transparent
	// problem: either both are transparent or none is
	// these functions don't help, the splitter handle is still transparent (depending on theme)
//	splitter->handle(0)->setAttribute(Qt::WA_TranslucentBackground, false);
//	splitter->handle(0)->setBackgroundRole(QPalette::Window);
//	splitter->handle(0)->setAutoFillBackground(true);
}

Viewer::~Viewer() {
	::close(sig_fd[0]);
	::close(sig_fd[1]);
	delete beamer;
	delete sig_notifier;
	delete layout;
	delete search_bar;
	delete canvas;
	delete toc;
	delete splitter;
	delete res;
}

bool Viewer::is_valid() const {
	return valid;
}

void Viewer::reload(bool clamp) {
#ifdef DEBUG
	cerr << "reloading file " << res->get_file().toUtf8().constData() << endl;
#endif

	res->load(res->get_file(), info_password.text().toLatin1());

	search_bar->reset_search(); // TODO restart search if loading the same document?
	search_bar->load(res->get_file(), info_password.text().toLatin1());

	update_info_widget();

	toc->init();
	canvas->get_layout()->clear_selection();
	canvas->reload(clamp);

	canvas->update_page_overlay();
 	presenter_progress.setMaximum(res->get_page_count());
}

void Viewer::open(QString new_file) {
	res->set_file(new_file);
	QFileInfo info(new_file);

	// different file - clear jumplist
	// e.g. in inotify-caused reload it doesn't hurt to keep the old jumplist
	// search is always cleared, see reload()
	res->clear_jumps();
	// TODO reset rotation?
	setWindowTitle(QString::fromUtf8("%1 \u2014 katarakt").arg(info.fileName()));
	reload();
}

void Viewer::open() {
	QString new_file = QFileDialog::getOpenFileName(this, "Open File", "", "PDF Files (*.pdf)");
	if (!new_file.isNull()) {
		open(new_file);
	}
}

void Viewer::print() {
	QPrinter printer(QPrinter::PrinterResolution);
	QPrintDialog *printDialog = new QPrintDialog(&printer, this);
	printDialog->setWindowTitle(QString::fromUtf8("katarakt/print %1").arg(get_res()->get_file()));
	printDialog->setOptions(QAbstractPrintDialog::PrintPageRange | QAbstractPrintDialog::PrintCurrentPage);
	int fromPage = 1, toPage = get_res()->get_page_count();
	printDialog->setMinMax(fromPage, toPage);
	printDialog->setFromTo(fromPage, toPage);

	if (printDialog->exec() != QDialog::Accepted) {
		delete printDialog;
		return;
	}

	if (printer.printRange() == QPrinter::PageRange) {
		fromPage = printer.fromPage();
		toPage = printer.toPage();
	} else if (printer.printRange() == QPrinter::CurrentPage) {
		fromPage = toPage = canvas->get_layout()->get_page() + 1; // 0-indexed
	}

	QStringList list;
	list << QString("-d") << QString("%1").arg(printer.printerName());
	list << QString("-n") << QString("%1").arg(printer.copyCount());
	list << QString("-P") << QString("%1-%2").arg(fromPage).arg(toPage);
	if (printer.duplex() == QPrinter::DuplexLongSide) {
		list << QString("-o") << QString("sides=two-sided-long-edge");
	} else if (printer.duplex() == QPrinter::DuplexShortSide) {
		list << QString("-o") << QString("sides=two-sided-short-edge");
	}
	// FIXME: how to set color / grayscale with lp? Or: how to disable it in print dialog?
	list << QString("--");
	list << QString("%1").arg(get_res()->get_file());
	QProcess::execute("lp", list);
	delete printDialog;
}

void Viewer::save() {
	QString new_file = QFileDialog::getSaveFileName(this, "Save File", "", "PDF Files (*.pdf)");
	if (!new_file.isNull()) {
		// file exists? Remove it (qt doesn't overwrite)
		QFile f(new_file);
		if (f.exists()) {
			QFileInfo source_file(res->get_file());
			QFileInfo dest_file(f);
			// source and destination the same?
			if (source_file == dest_file) {
#ifdef DEBUG
				cerr << "copy: src and dest file are the same" << endl;
#endif
				return;
			}

			if (!f.remove()) {
#ifdef DEBUG
				cerr << "failed to remove the file" << endl;
#endif
			}
		}
		if (!QFile::copy(res->get_file(), new_file)) {
			// TODO error dialog?
#ifdef DEBUG
			cerr << "failed to copy the file" << endl;
#endif
		}
	}
}

void Viewer::jump_back() {
	int new_page = res->jump_back();
	if (new_page == -1) {
		return;
	}
	canvas->get_layout()->scroll_page(new_page, false);
}

void Viewer::jump_forward() {
	int new_page = res->jump_forward();
	if (new_page == -1) {
		return;
	}
	canvas->get_layout()->scroll_page(new_page, false);
}

void Viewer::mark_jump() {
	int page = canvas->get_layout()->get_page();
	res->store_jump(page);
}

// general movement
void Viewer::page_up() {
	canvas->get_layout()->scroll_page(-1);
}

void Viewer::page_down() {
	canvas->get_layout()->scroll_page(1);
}

void Viewer::top() {
	canvas->get_layout()->scroll_page_jump(-1, false);
}

void Viewer::bottom() {
	canvas->get_layout()->scroll_page_jump(res->get_page_count(), false);
}

void Viewer::half_screen_up() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, height() * 0.5f);
	} else { // fallback
		page_up();
	}
}

void Viewer::half_screen_down() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, -height() * 0.5f);
	} else { // fallback
		page_down();
	}
}

void Viewer::screen_up() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, height() * screen_scroll_factor);
	} else { // fallback
		page_up();
	}
}

void Viewer::screen_down() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, -height() * screen_scroll_factor);
	} else { // fallback
		page_down();
	}
}

void Viewer::smooth_up() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, smooth_scroll_delta);
	} else { // fallback
		page_up();
	}
}

void Viewer::smooth_down() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(0, -smooth_scroll_delta);
	} else { // fallback
		page_down();
	}
}

void Viewer::smooth_left() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(smooth_scroll_delta, 0);
	} else { // fallback
		page_up();
	}
}

void Viewer::smooth_right() {
	if (canvas->get_layout()->supports_smooth_scrolling()) {
		canvas->get_layout()->scroll_smooth(-smooth_scroll_delta, 0);
	} else { // fallback
		page_down();
	}
}

void Viewer::zoom_in() {
	canvas->get_layout()->set_zoom(1);
}

void Viewer::zoom_out() {
	canvas->get_layout()->set_zoom(-1);
}

void Viewer::reset_zoom() {
	canvas->get_layout()->set_zoom(0, false);
}

void Viewer::increase_columns() {
	canvas->get_layout()->set_columns(1);
}

void Viewer::decrease_columns() {
	canvas->get_layout()->set_columns(-1);
}

void Viewer::increase_offset() {
	canvas->get_layout()->set_offset(1);
}

void Viewer::decrease_offset() {
	canvas->get_layout()->set_offset(-1);
}

void Viewer::quit() {
	QCoreApplication::exit(0);
}

void Viewer::search() {
	search_bar->focus();
}

void Viewer::search_backward() {
	search_bar->focus(false);
}

void Viewer::next_hit() {
	if (canvas->get_layout()->get_search_visible()) {
		canvas->get_layout()->advance_hit();
	}
}

void Viewer::previous_hit() {
	if (canvas->get_layout()->get_search_visible()) {
		canvas->get_layout()->advance_hit(false);
	}
}

void Viewer::next_invisible_hit() {
	if (canvas->get_layout()->get_search_visible()) {
		canvas->get_layout()->advance_invisible_hit();
	}
}

void Viewer::previous_invisible_hit() {
	if (canvas->get_layout()->get_search_visible()) {
		canvas->get_layout()->advance_invisible_hit(false);
	}
}

void Viewer::rotate_left() {
	res->rotate(-1);
	canvas->get_layout()->rebuild();
	canvas->update();
}

void Viewer::rotate_right() {
	res->rotate(1);
	canvas->get_layout()->rebuild();
	canvas->update();
}

void Viewer::invert_colors() {
	res->invert_colors();
	canvas->update();
	beamer->update();
}

void Viewer::copy_to_clipboard() {
	canvas->get_layout()->copy_selection_text(QClipboard::Clipboard);
}

void Viewer::toggle_toc() {
	toc->setVisible(!toc->isVisible());
	toc->setFocus(Qt::OtherFocusReason); // only works if shown
}

void Viewer::update_info_widget() {
	if (!res->is_valid() || !search_bar->is_valid()) {
		QIcon icon;
		const QString file = res->get_file();

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

BeamerWindow *Viewer::get_beamer() const {
	return beamer;
}

void Viewer::layout_updated(int new_page, bool page_changed) {
	if (page_changed) {
		canvas->get_layout()->scroll_page(new_page, false);
		if (beamer->isVisible()) {
			beamer->get_layout()->scroll_page(new_page, false);
		}
		// TODO unfold toc tree to show current entry?
		canvas->update_page_overlay();
		presenter_progress.setValue(new_page + 1);
	}
	canvas->update();
	beamer->update();
}

void Viewer::show_progress(bool show) {
	presenter_progress.setVisible(show);
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

void Viewer::setup_keys(QWidget *base) {
	add_action(base, "Keys/page_up", SLOT(page_up()), this);
	add_action(base, "Keys/page_down", SLOT(page_down()), this);
	add_action(base, "Keys/top", SLOT(top()), this);
	add_action(base, "Keys/bottom", SLOT(bottom()), this);
	add_action(base, "Keys/half_screen_up", SLOT(half_screen_up()), this);
	add_action(base, "Keys/half_screen_down", SLOT(half_screen_down()), this);
	add_action(base, "Keys/screen_up", SLOT(screen_up()), this);
	add_action(base, "Keys/screen_down", SLOT(screen_down()), this);
	add_action(base, "Keys/smooth_up", SLOT(smooth_up()), this);
	add_action(base, "Keys/smooth_down", SLOT(smooth_down()), this);
	add_action(base, "Keys/smooth_left", SLOT(smooth_left()), this);
	add_action(base, "Keys/smooth_right", SLOT(smooth_right()), this);
	add_action(base, "Keys/next_hit", SLOT(next_hit()), this);
	add_action(base, "Keys/previous_hit", SLOT(previous_hit()), this);
	add_action(base, "Keys/next_invisible_hit", SLOT(next_invisible_hit()), this);
	add_action(base, "Keys/previous_invisible_hit", SLOT(previous_invisible_hit()), this);
	add_action(base, "Keys/jump_back", SLOT(jump_back()), this);
	add_action(base, "Keys/jump_forward", SLOT(jump_forward()), this);

	add_action(base, "Keys/zoom_in", SLOT(zoom_in()), this);
	add_action(base, "Keys/zoom_out", SLOT(zoom_out()), this);
	add_action(base, "Keys/reset_zoom", SLOT(reset_zoom()), this);
	add_action(base, "Keys/increase_columns", SLOT(increase_columns()), this);
	add_action(base, "Keys/decrease_columns", SLOT(decrease_columns()), this);
	add_action(base, "Keys/increase_offset", SLOT(increase_offset()), this);
	add_action(base, "Keys/decrease_offset", SLOT(decrease_offset()), this);
	add_action(base, "Keys/rotate_left", SLOT(rotate_left()), this);
	add_action(base, "Keys/rotate_right", SLOT(rotate_right()), this);

	add_action(base, "Keys/quit", SLOT(quit()), this);
	add_action(base, "Keys/search", SLOT(search()), this);
	add_action(base, "Keys/search_backward", SLOT(search_backward()), this);
	add_action(base, "Keys/close_search", SLOT(close_search()), this);
	add_action(base, "Keys/mark_jump", SLOT(mark_jump()), this);
	add_action(base, "Keys/invert_colors", SLOT(invert_colors()), this);
	add_action(base, "Keys/copy_to_clipboard", SLOT(copy_to_clipboard()), this);
	add_action(base, "Keys/toggle_fullscreen", SLOT(toggle_fullscreen()), base);
	add_action(base, "Keys/reload", SLOT(reload()), this);
	add_action(base, "Keys/open", SLOT(open()), this);
	add_action(base, "Keys/print", SLOT(print()), this);
	add_action(base, "Keys/save", SLOT(save()), this);
	add_action(base, "Keys/toggle_toc", SLOT(toggle_toc()), this);
}

