#include "canvas.h"
#include "viewer.h"
#include "layout.h"
#include "resourcemanager.h"
#include "search.h"

using namespace std;


Canvas::Canvas(Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v),
		draw_overlay(true),
		valid(true) {
	setFocusPolicy(Qt::StrongFocus);

	layout = new PresentationLayout(viewer->get_res());

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

	add_sequence("K", &Canvas::auto_smooth_up);
	add_sequence("J", &Canvas::auto_smooth_down);
	add_sequence("H", &Canvas::smooth_left);
	add_sequence("L", &Canvas::smooth_right);

	add_sequence("+", &Canvas::zoom_in);
	add_sequence("=", &Canvas::zoom_in);
	add_sequence("-", &Canvas::zoom_out);
	add_sequence("z,z", &Canvas::reset_zoom);

	add_sequence("]", &Canvas::columns_inc);
	add_sequence("[", &Canvas::columns_dec);

	add_sequence("T", &Canvas::toggle_overlay);

	add_sequence("Q", &Canvas::quit);
	add_sequence("n,0,0,b", &Canvas::quit); // just messing around :)

	add_sequence("/", &Canvas::search);
}

Canvas::~Canvas() {
	layout->clear_hits();
	delete layout;
}

bool Canvas::is_valid() const {
	return valid;
}

void Canvas::reload() {
	layout->rebuild();
	update();
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
		.arg(viewer->get_res()->get_page_count());

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
	layout->scroll_page(viewer->get_res()->get_page_count(), false);
	update();
}

void Canvas::auto_smooth_up() {
	if (layout->supports_smooth_scrolling()) {
		smooth_up();
	} else {
		page_up();
	}
}

void Canvas::auto_smooth_down() {
	if (layout->supports_smooth_scrolling()) {
		smooth_down();
	} else {
		page_down();
	}
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

void Canvas::reset_zoom() {
	layout->set_zoom(0, false);
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

void Canvas::quit() {
	QCoreApplication::exit(0);
}

void Canvas::search() {
	static_cast<Viewer*>(parentWidget())->focus_search();
}

void Canvas::search_clear() {
	layout->clear_hits();
	update();
}

void Canvas::search_done(int page, list<Result> *hits) {
	layout->set_hits(page, hits);
	update();
}

void Canvas::search_visible(bool visible) {
	layout->set_search_visible(visible);
	update();
}

void Canvas::page_rendered(int /*page*/) {
	// TODO use page, update selectively
	update();
}

