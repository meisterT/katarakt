#include "viewer.h"

using namespace std;


Viewer::Viewer(ResourceManager *_res, QWidget *parent) :
		QWidget(parent),
		res(_res),
		draw_overlay(true) {
	setFocusPolicy(Qt::StrongFocus);
	res->set_viewer(this);

	layout = new PresentationLayout(res);

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	// key -> function mapping
	add_sequence("1", &Viewer::set_presentation_layout);
	add_sequence("2", &Viewer::set_grid_layout);

	add_sequence("Space", &Viewer::page_down);
	add_sequence("PgDown", &Viewer::page_down);
	add_sequence("Down", &Viewer::page_down);

	add_sequence("Backspace", &Viewer::page_up);
	add_sequence("PgUp", &Viewer::page_up);
	add_sequence("Up", &Viewer::page_up);

	add_sequence("G", &Viewer::page_first);
	add_sequence("Shift+G", &Viewer::page_last);

	add_sequence("K", &Viewer::smooth_up);
	add_sequence("J", &Viewer::smooth_down);
	add_sequence("H", &Viewer::smooth_left);
	add_sequence("L", &Viewer::smooth_right);

	add_sequence("+", &Viewer::zoom_in);
	add_sequence("=", &Viewer::zoom_in);
	add_sequence("-", &Viewer::zoom_out);

	add_sequence("]", &Viewer::columns_inc);
	add_sequence("[", &Viewer::columns_dec);

	add_sequence("F", &Viewer::toggle_fullscreen);
	add_sequence("T", &Viewer::toggle_overlay);
	add_sequence("R", &Viewer::reload);

	add_sequence("Q", &Viewer::quit);
	add_sequence("Esc", &Viewer::quit);
	add_sequence("n,0,0,b", &Viewer::quit); // just messing around :)
}

Viewer::~Viewer() {
	delete layout;
}

void Viewer::add_sequence(QString key, func_t action) {
	QKeySequence s(key);
	sequences[s] = action;
	grabShortcut(s, Qt::WidgetShortcut);
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

void Viewer::paintEvent(QPaintEvent * /*event*/) {
//	cerr << "redraw" << endl;
	QPainter painter(this);
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

	setWindowTitle(title);
}

void Viewer::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		mx = event->x();
		my = event->y();
	}
}

void Viewer::mouseMoveEvent(QMouseEvent *event) {
	if (event->buttons() & Qt::LeftButton) {
		layout->scroll_smooth(event->x() - mx, event->y() - my);
		mx = event->x();
		my = event->y();
		update(); // TODO don't do this here
	}
}

void Viewer::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	if (event->orientation() == Qt::Vertical) {
		layout->scroll_smooth(0, d);
		update();
	} else { // TODO untested
		layout->scroll_smooth(d, 0);
		update();
	}
}

void Viewer::resizeEvent(QResizeEvent *event) {
	layout->resize(event->size().width(), event->size().height());
	update();
}

// primitive actions
// TODO find a more compact way?
void Viewer::set_presentation_layout() {
	Layout *old_layout = layout;
	layout = new PresentationLayout(*old_layout);
	delete old_layout;
	update();
}

void Viewer::set_grid_layout() {
	Layout *old_layout = layout;
	layout = new GridLayout(*old_layout);
	delete old_layout;
	update();
}

void Viewer::page_up() {
	layout->scroll_page(-1);
	update();
}

void Viewer::page_down() {
	layout->scroll_page(1);
	update();
}

void Viewer::page_first() {
	layout->scroll_page(-1, false);
	update();
}

void Viewer::page_last() {
	layout->scroll_page(res->get_page_count(), false);
	update();
}

void Viewer::smooth_up() {
	layout->scroll_smooth(0, 30);
	update();
}

void Viewer::smooth_down() {
	layout->scroll_smooth(0, -30);
	update();
}

void Viewer::smooth_left() {
	layout->scroll_smooth(30, 0);
	update();
}

void Viewer::smooth_right() {
	layout->scroll_smooth(-30, 0);
	update();
}

void Viewer::zoom_in() {
	layout->set_zoom(1);
	update();
}

void Viewer::zoom_out() {
	layout->set_zoom(-1);
	update();
}

void Viewer::columns_inc() {
	layout->set_columns(1);
	update();
}

void Viewer::columns_dec() {
	layout->set_columns(-1);
	update();
}

void Viewer::toggle_fullscreen() {
	setWindowState(windowState() ^ Qt::WindowFullScreen);
	update();
}

void Viewer::toggle_overlay() {
	draw_overlay = not draw_overlay;
	update();
}

void Viewer::reload() {
	res->reload_document();
	layout->rebuild();
	update();
}

void Viewer::quit() {
	QCoreApplication::exit(0);
}

void Viewer::page_rendered(int /*page*/) {
	// TODO use page, update selectively
	update();
}

