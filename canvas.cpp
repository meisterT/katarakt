#include <QAction>
#include <QStringListIterator>
#include <QKeySequence>
#include <QCoreApplication>
#include <QString>
#include <QPainter>
#include <iostream>
#include "canvas.h"
#include "viewer.h"
#include "layout.h"
#include "resourcemanager.h"
#include "search.h"
#include "gotoline.h"
#include "config.h"

using namespace std;


Canvas::Canvas(Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v),
		draw_overlay(true),
		valid(true) {
	setFocusPolicy(Qt::StrongFocus);

	layout = new PresentationLayout(viewer->get_res());

	// load config options
	CFG *config = CFG::get_instance();
	// apply start option
	layout->scroll_page(config->get_value("start_page").toInt(), false);

	mouse_wheel_factor = config->get_value("mouse_wheel_factor").toInt();
	smooth_scroll_delta = config->get_value("smooth_scroll_delta").toInt();
	screen_scroll_factor = config->get_value("screen_scroll_factor").toFloat();
	// setup keys
	add_action("set_presentation_layout", SLOT(set_presentation_layout()));
	add_action("set_grid_layout", SLOT(set_grid_layout()));
	add_action("page_up", SLOT(page_up()));
	add_action("page_down", SLOT(page_down()));
	add_action("page_first", SLOT(page_first()));
	add_action("page_last", SLOT(page_last()));
	add_action("half_screen_up", SLOT(half_screen_up()));
	add_action("half_screen_down", SLOT(half_screen_down()));
	add_action("screen_up", SLOT(screen_up()));
	add_action("screen_down", SLOT(screen_down()));
	add_action("smooth_up", SLOT(smooth_up()));
	add_action("smooth_down", SLOT(smooth_down()));
	add_action("smooth_left", SLOT(smooth_left()));
	add_action("smooth_right", SLOT(smooth_right()));
	add_action("zoom_in", SLOT(zoom_in()));
	add_action("zoom_out", SLOT(zoom_out()));
	add_action("reset_zoom", SLOT(reset_zoom()));
	add_action("columns_inc", SLOT(columns_inc()));
	add_action("columns_dec", SLOT(columns_dec()));
	add_action("toggle_overlay", SLOT(toggle_overlay()));
	add_action("quit", SLOT(quit()));
	add_action("search", SLOT(search()));
	add_action("next_hit", SLOT(next_hit()));
	add_action("previous_hit", SLOT(previous_hit()));
	add_action("next_invisible_hit", SLOT(next_invisible_hit()));
	add_action("previous_invisible_hit", SLOT(previous_invisible_hit()));
	add_action("focus_goto", SLOT(focus_goto()));
	add_action("rotate_left", SLOT(rotate_left()));
	add_action("rotate_right", SLOT(rotate_right()));

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	goto_line = new GotoLine(viewer->get_res()->get_page_count(), this);
	goto_line->hide(); // TODO why is it shown by default?
	connect(goto_line, SIGNAL(returnPressed()), this, SLOT(goto_page()), Qt::UniqueConnection);
	goto_line->move(0, height() - goto_line->height()); // TODO why is the initial position wrong?
}

Canvas::~Canvas() {
	delete goto_line;
	layout->clear_hits();
	delete layout;
}

bool Canvas::is_valid() const {
	return valid;
}

void Canvas::reload() {
	layout->rebuild();
	goto_line->set_page_count(viewer->get_res()->get_page_count());
	update();
}

void Canvas::add_action(const char *action, const char *slot) {
	QStringListIterator i(CFG::get_instance()->get_keys(action));
	while (i.hasNext()) {
		QAction *a = new QAction(this);
		a->setShortcut(QKeySequence(i.next()));
		addAction(a);
		connect(a, SIGNAL(triggered()), this, slot);
	}
}

const Layout *Canvas::get_layout() const {
	return layout;
}

void Canvas::paintEvent(QPaintEvent * /*event*/) {
#ifdef DEBUG
	cerr << "redraw" << endl;
#endif
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
		mx_down = mx;
		my_down = my;
	}
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		if (mx_down == event->x() && my_down == event->y()) {
			if (layout->click_mouse(mx_down, my_down)) {
				update();
			}
		}
	}
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
	if (event->buttons() & Qt::LeftButton) {
		if (layout->scroll_smooth(event->x() - mx, event->y() - my)) {
			update();
		}
		mx = event->x();
		my = event->y();
	}
}

void Canvas::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	if (event->orientation() == Qt::Vertical) {
		if (layout->supports_smooth_scrolling()) {
			if (layout->scroll_smooth(0, d)) {
				update();
			}
		} else {
			if (layout->scroll_page(-d / mouse_wheel_factor)) {
				update();
			}
		}
	} else {
		if (layout->scroll_smooth(d, 0)) {
			update();
		}
	}
}

void Canvas::mouseDoubleClickEvent(QMouseEvent * event) {
	if (event->button() == Qt::LeftButton) {
		layout->goto_page_at(event->x(), event->y());
		update();
	}
}

void Canvas::resizeEvent(QResizeEvent *event) {
	layout->resize(event->size().width(), event->size().height());
	goto_line->move(0, height() - goto_line->height());
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

// general movement
void Canvas::page_up() {
	if (layout->scroll_page(-1)) {
		update();
	}
}

void Canvas::page_down() {
	if (layout->scroll_page(1)) {
		update();
	}
}

void Canvas::page_first() {
	if (layout->scroll_page(-1, false)) {
		update();
	}
}

void Canvas::page_last() {
	if (layout->scroll_page(viewer->get_res()->get_page_count(), false)) {
		update();
	}
}

void Canvas::half_screen_up() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, height() * 0.5f)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::half_screen_down() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, -height() * 0.5f)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}

void Canvas::screen_up() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, height() * screen_scroll_factor)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::screen_down() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, -height() * screen_scroll_factor)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}

void Canvas::smooth_up() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, smooth_scroll_delta)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::smooth_down() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(0, -smooth_scroll_delta)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}


void Canvas::smooth_left() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(smooth_scroll_delta, 0)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::smooth_right() {
	if (layout->supports_smooth_scrolling()) {
		if (layout->scroll_smooth(-smooth_scroll_delta, 0)) {
			update();
		}
	} else { // fallback
		page_down();
	}
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
	viewer->focus_search();
}

void Canvas::next_hit() {
	if (layout->get_search_visible()) {
		if (layout->advance_hit()) {
			update();
		}
	}
}

void Canvas::previous_hit() {
	if (layout->get_search_visible()) {
		if (layout->advance_hit(false)) {
			update();
		}
	}
}

void Canvas::next_invisible_hit() {
	if (layout->get_search_visible()) {
		if (layout->advance_invisible_hit()) {
			update();
		}
	}
}

void Canvas::previous_invisible_hit() {
	if (layout->get_search_visible()) {
		if (layout->advance_invisible_hit(false)) {
			update();
		}
	}
}

void Canvas::focus_goto() {
	goto_line->setFocus();
	goto_line->setText(QString::number(layout->get_page() + 1));
	goto_line->selectAll();
	goto_line->show();
}

void Canvas::search_clear() {
	layout->clear_hits();
	update();
}

void Canvas::search_done(int page, list<Result> *l) {
	layout->set_hits(page, l);
	update();
}

void Canvas::search_visible(bool visible) {
	layout->set_search_visible(visible);
	update();
}

void Canvas::page_rendered(int page) {
	if (layout->page_visible(page)) {
		update();
	}
}

void Canvas::goto_page() {
	int page = goto_line->text().toInt() - 1;
	goto_line->hide();
	if (layout->scroll_page(page, false)) {
		update();
	}
}
void Canvas::rotate_left() {
	viewer->get_res()->rotate(-1);
	layout->rebuild();
	update();
}

void Canvas::rotate_right() {
	viewer->get_res()->rotate(1);
	layout->rebuild();
	update();
}

