#include "canvas.h"
#include <csignal>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include "viewer.h"

using namespace std;


int Canvas::sig_fd[2];

Canvas::Canvas(ResourceManager *_res, QWidget *parent) :
		QWidget(parent),
		res(_res),
		draw_overlay(true),
		valid(true) {
	setFocusPolicy(Qt::StrongFocus);
	res->set_canvas(this);

	layout = new PresentationLayout(res);

	// setup signal handling
	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sig_fd) == -1) {
		cerr << "socketpair: " << strerror(errno) << endl;
		valid = false;
		sig_notifier = NULL;
		return;
	}
	sig_notifier = new QSocketNotifier(sig_fd[1], QSocketNotifier::Read, this);
	connect(sig_notifier, SIGNAL(activated(int)), this, SLOT(handle_signal()));

	struct sigaction usr;
	usr.sa_handler = Canvas::signal_handler;
	sigemptyset(&usr.sa_mask);
	usr.sa_flags = SA_RESTART;

	if (sigaction(SIGUSR1, &usr, 0) > 0) {
		cerr << "sigaction: " << strerror(errno) << endl;
		valid = false;
		return;
	}

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	// key -> function mapping
	add_sequence("1", &Canvas::set_presentation_layout);
	add_sequence("2", &Canvas::set_grid_layout);

	add_sequence("Space", &Canvas::page_down);
	add_sequence("PgDown", &Canvas::page_down);
	add_sequence("Down", &Canvas::page_down);

	add_sequence("Backspace", &Canvas::page_up);
	add_sequence("PgUp", &Canvas::page_up);
	add_sequence("Up", &Canvas::page_up);

	add_sequence("G", &Canvas::page_first);
	add_sequence("Shift+G", &Canvas::page_last);
	add_sequence("C-x", &Canvas::page_last);

	add_sequence("K", &Canvas::smooth_up);
	add_sequence("J", &Canvas::smooth_down);
	add_sequence("H", &Canvas::smooth_left);
	add_sequence("L", &Canvas::smooth_right);

	add_sequence("+", &Canvas::zoom_in);
	add_sequence("=", &Canvas::zoom_in);
	add_sequence("-", &Canvas::zoom_out);

	add_sequence("]", &Canvas::columns_inc);
	add_sequence("[", &Canvas::columns_dec);

	add_sequence("T", &Canvas::toggle_overlay);
	add_sequence("R", &Canvas::reload);

	add_sequence("Q", &Canvas::quit);
	add_sequence("n,0,0,b", &Canvas::quit); // just messing around :)

	add_sequence("/", &Canvas::search);
}

Canvas::~Canvas() {
	::close(sig_fd[0]);
	::close(sig_fd[1]);
	delete sig_notifier;
	delete layout;
}

bool Canvas::is_valid() const {
	return valid;
}

void Canvas::signal_handler(int /*unused*/) {
	char tmp = '1';
	if (write(sig_fd[0], &tmp, sizeof(char)) < 0) {
		cerr << "write: " << strerror(errno) << endl;
	}
}

void Canvas::handle_signal() {
	sig_notifier->setEnabled(false);
	char tmp;
	if (read(sig_fd[1], &tmp, sizeof(char)) < 0) {
		cerr << "read: " << strerror(errno) << endl;
	}

	reload();

	sig_notifier->setEnabled(true);
}

void Canvas::add_sequence(QString key, func_t action) {
	QKeySequence s(key);
	sequences[s] = action;
	grabShortcut(s, Qt::WidgetShortcut);
}

bool Canvas::event(QEvent *event) {
	if (event->type() == QEvent::Shortcut) {
		QShortcutEvent *s = static_cast<QShortcutEvent*>(event);
		if (sequences.find(s->key()) != sequences.end()) {
			(this->*sequences[s->key()])();
		}
		return true;
	}
	return QWidget::event(event);
}

void Canvas::paintEvent(QPaintEvent * /*event*/) {
//	cerr << "redraw" << endl;
	QPainter painter(this);
	painter.fillRect(rect(), Qt::black);
	layout->render(&painter);

	QString title = QString("page %1/%2")
		.arg(layout->get_page() + 1)
		.arg(res->get_page_count());

	if (draw_overlay) {
		QRect size = QRect(0, 0, width(), height());
		int flags = Qt::AlignBottom | Qt::AlignRight;
		QRect bounding = painter.boundingRect(size, flags, title);

		painter.setPen(Qt::NoPen);
		painter.setBrush(QColor("#eeeeec"));
		painter.drawRect(bounding);
		painter.setPen(QColor("#2e3436"));
		painter.drawText(size, flags, title);
	}
}

void Canvas::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		mx = event->x();
		my = event->y();
	}
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
	if (event->buttons() & Qt::LeftButton) {
		layout->scroll_smooth(event->x() - mx, event->y() - my);
		mx = event->x();
		my = event->y();
		update(); // TODO don't do this here
	}
}

void Canvas::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	if (event->orientation() == Qt::Vertical) {
		layout->scroll_smooth(0, d);
		update();
	} else { // TODO untested
		layout->scroll_smooth(d, 0);
		update();
	}
}

void Canvas::resizeEvent(QResizeEvent *event) {
	layout->resize(event->size().width(), event->size().height());
	update();
}

// primitive actions
// TODO find a more compact way?
void Canvas::set_presentation_layout() {
	Layout *old_layout = layout;
	layout = new PresentationLayout(*old_layout);
	delete old_layout;
	update();
}

void Canvas::set_grid_layout() {
	Layout *old_layout = layout;
	layout = new GridLayout(*old_layout);
	delete old_layout;
	update();
}

void Canvas::page_up() {
	layout->scroll_page(-1);
	update();
}

void Canvas::page_down() {
	layout->scroll_page(1);
	update();
}

void Canvas::page_first() {
	layout->scroll_page(-1, false);
	update();
}

void Canvas::page_last() {
	layout->scroll_page(res->get_page_count(), false);
	update();
}

void Canvas::smooth_up() {
	layout->scroll_smooth(0, 30);
	update();
}

void Canvas::smooth_down() {
	layout->scroll_smooth(0, -30);
	update();
}

void Canvas::smooth_left() {
	layout->scroll_smooth(30, 0);
	update();
}

void Canvas::smooth_right() {
	layout->scroll_smooth(-30, 0);
	update();
}

void Canvas::zoom_in() {
	layout->set_zoom(1);
	update();
}

void Canvas::zoom_out() {
	layout->set_zoom(-1);
	update();
}

void Canvas::columns_inc() {
	layout->set_columns(1);
	update();
}

void Canvas::columns_dec() {
	layout->set_columns(-1);
	update();
}

void Canvas::toggle_overlay() {
	draw_overlay = not draw_overlay;
	update();
}

void Canvas::reload() {
	res->reload_document();
	layout->rebuild();
	update();
}

void Canvas::quit() {
	QCoreApplication::exit(0);
}

void Canvas::search() {
	static_cast<Viewer*>(parentWidget())->focus_search();
}

void Canvas::page_rendered(int /*page*/) {
	// TODO use page, update selectively
	update();
}

