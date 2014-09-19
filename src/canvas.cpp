#include <QAction>
#include <QStringListIterator>
#include <QKeySequence>
#include <QCoreApplication>
#include <QString>
#include <QPainter>
#include <QApplication>
#include <iostream>
#include "canvas.h"
#include "viewer.h"
#include "layout/layout.h"
#include "layout/presentationlayout.h"
#include "layout/gridlayout.h"
#include "layout/presenterlayout.h"
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

	clear_jumps();

	// load config options
	CFG *config = CFG::get_instance();

	background_opacity = config->get_value("background_opacity").toInt();

	presentation_layout = new PresentationLayout(viewer);
	grid_layout = new GridLayout(viewer);
	presenter_layout = new PresenterLayout(viewer);

	QString default_layout = config->get_value("default_layout").toString();
	if (default_layout == "grid") {
		cur_layout = grid_layout;
	} else { // "presentation" and everything else
		cur_layout = presentation_layout;
	}

	// apply start option
	cur_layout->scroll_page(config->get_tmp_value("start_page").toInt(), false);

	mouse_wheel_factor = config->get_value("mouse_wheel_factor").toInt();
	smooth_scroll_delta = config->get_value("smooth_scroll_delta").toInt();
	screen_scroll_factor = config->get_value("screen_scroll_factor").toFloat();
	// setup keys
	add_action("set_presentation_layout", SLOT(set_presentation_layout()));
	add_action("set_grid_layout", SLOT(set_grid_layout()));
	add_action("set_presenter_layout", SLOT(set_presenter_layout()));
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
	add_action("jump_back", SLOT(jump_back()));
	add_action("jump_forward", SLOT(jump_forward()));
	add_action("toggle_invert_colors", SLOT(invert_colors()));

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	goto_line = new GotoLine(viewer->get_res()->get_page_count(), this);
	goto_line->hide(); // TODO why is it shown by default?
	connect(goto_line, SIGNAL(returnPressed()), this, SLOT(goto_page()), Qt::UniqueConnection);
	goto_line->move(0, height() - goto_line->height()); // TODO why is the initial position wrong?
}

Canvas::~Canvas() {
	delete goto_line;
	delete presentation_layout;
	delete grid_layout;
}

bool Canvas::is_valid() const {
	return valid;
}

void Canvas::reload(bool clamp) {
	cur_layout->rebuild(clamp);
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

void Canvas::store_jump(int page) {
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

void Canvas::clear_jumps() {
	jumplist.clear();
	jump_map.clear();
	cur_jump_pos = jumplist.end();
}

void Canvas::jump_back() {
	if (cur_jump_pos == jumplist.begin()) {
		return;
	}
	if (cur_jump_pos == jumplist.end()) {
		store_jump(cur_layout->get_page());
		--cur_jump_pos;
	}
	--cur_jump_pos;
	if (cur_layout->scroll_page(*cur_jump_pos, false)) {
		update();
	}
}

void Canvas::jump_forward() {
	if (cur_jump_pos == jumplist.end() || cur_jump_pos == --jumplist.end()) {
		return;
	}
	++cur_jump_pos;
	if (cur_layout->scroll_page(*cur_jump_pos, false)) {
		update();
	}
}

void Canvas::invert_colors() {
	viewer->get_res()->invert_colors();
	update();
}

Layout *Canvas::get_layout() const {
	return cur_layout;
}

void Canvas::paintEvent(QPaintEvent * /*event*/) {
#ifdef DEBUG
	cerr << "redraw" << endl;
#endif
	QPainter painter(this);
	if (viewer->isFullScreen()) {
		painter.fillRect(rect(), QColor(0, 0, 0));
	} else {
		painter.fillRect(rect(), QColor(0, 0, 0, background_opacity));
	}
	cur_layout->render(&painter);

	QString title = QString("page %1/%2")
		.arg(cur_layout->get_page() + 1)
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
			int page = cur_layout->get_page();
			if (cur_layout->click_mouse(mx_down, my_down)) {
				store_jump(page); // store old position if a clicked link moved the view
				update();
			}
		}
	}
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
	if (event->buttons() & Qt::LeftButton) {
		if (cur_layout->scroll_smooth(event->x() - mx, event->y() - my)) {
			update();
		}
		mx = event->x();
		my = event->y();
	}
}

void Canvas::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	switch (QApplication::keyboardModifiers()) {
		// scroll
		case Qt::NoModifier:
			if (event->orientation() == Qt::Vertical) {
				if (cur_layout->supports_smooth_scrolling()) {
					if (cur_layout->scroll_smooth(0, d)) {
						update();
					}
				} else {
					if (cur_layout->scroll_page(-d / mouse_wheel_factor)) {
						update();
					}
				}
			} else {
				if (cur_layout->scroll_smooth(d, 0)) {
					update();
				}
			}
			break;

		// zoom
		case Qt::ControlModifier:
			if (cur_layout->set_zoom(d / mouse_wheel_factor)) {
				update();
			}
			break;
	}
}

void Canvas::mouseDoubleClickEvent(QMouseEvent * event) {
	if (event->button() == Qt::LeftButton) {
		cur_layout->goto_page_at(event->x(), event->y());
		update();
	}
}

void Canvas::resizeEvent(QResizeEvent *event) {
	cur_layout->resize(event->size().width(), event->size().height());
	goto_line->move(0, height() - goto_line->height());
	update();
}

// primitive actions
void Canvas::set_presentation_layout() {
	presentation_layout->activate(cur_layout);
	cur_layout = presentation_layout;
	update();
}

void Canvas::set_grid_layout() {
	grid_layout->activate(cur_layout);
	grid_layout->rebuild();
	cur_layout = grid_layout;
	update();
}

void Canvas::set_presenter_layout() {
	presenter_layout->activate(cur_layout);
	presenter_layout->rebuild();
	cur_layout = presenter_layout;
	update();
}

// general movement
void Canvas::page_up() {
	if (cur_layout->scroll_page(-1)) {
		update();
	}
}

void Canvas::page_down() {
	if (cur_layout->scroll_page(1)) {
		update();
	}
}

void Canvas::page_first() {
	int page = cur_layout->get_page();
	if (cur_layout->scroll_page(-1, false)) {
		store_jump(page);
		update();
	}
}

void Canvas::page_last() {
	int page = cur_layout->get_page();
	if (cur_layout->scroll_page(viewer->get_res()->get_page_count(), false)) {
		store_jump(page);
		update();
	}
}

void Canvas::half_screen_up() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, height() * 0.5f)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::half_screen_down() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, -height() * 0.5f)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}

void Canvas::screen_up() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, height() * screen_scroll_factor)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::screen_down() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, -height() * screen_scroll_factor)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}

void Canvas::smooth_up() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, smooth_scroll_delta)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::smooth_down() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(0, -smooth_scroll_delta)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}


void Canvas::smooth_left() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(smooth_scroll_delta, 0)) {
			update();
		}
	} else { // fallback
		page_up();
	}
}

void Canvas::smooth_right() {
	if (cur_layout->supports_smooth_scrolling()) {
		if (cur_layout->scroll_smooth(-smooth_scroll_delta, 0)) {
			update();
		}
	} else { // fallback
		page_down();
	}
}

void Canvas::zoom_in() {
	if (cur_layout->set_zoom(1)) {
		update();
	}
}

void Canvas::zoom_out() {
	if (cur_layout->set_zoom(-1)) {
		update();
	}
}

void Canvas::reset_zoom() {
	if (cur_layout->set_zoom(0, false)) {
		update();
	}
}

void Canvas::columns_inc() {
	cur_layout->set_columns(1);
	update();
}

void Canvas::columns_dec() {
	cur_layout->set_columns(-1);
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
	if (cur_layout->get_search_visible()) {
		int page = cur_layout->get_page();
		if (cur_layout->advance_hit()) {
			store_jump(page);
			update();
		}
	}
}

void Canvas::previous_hit() {
	if (cur_layout->get_search_visible()) {
		int page = cur_layout->get_page();
		if (cur_layout->advance_hit(false)) {
			store_jump(page);
			update();
		}
	}
}

void Canvas::next_invisible_hit() {
	if (cur_layout->get_search_visible()) {
		int page = cur_layout->get_page();
		if (cur_layout->advance_invisible_hit()) {
			store_jump(page);
			update();
		}
	}
}

void Canvas::previous_invisible_hit() {
	if (cur_layout->get_search_visible()) {
		int page = cur_layout->get_page();
		if (cur_layout->advance_invisible_hit(false)) {
			store_jump(page);
			update();
		}
	}
}

void Canvas::focus_goto() {
	goto_line->setFocus();
	goto_line->setText(QString::number(cur_layout->get_page() + 1));
	goto_line->selectAll();
	goto_line->show();
}

void Canvas::set_search_visible(bool visible) {
	cur_layout->set_search_visible(visible);
	update();
}

void Canvas::page_rendered(int page) {
	if (cur_layout->page_visible(page)) {
		update();
	}
}

void Canvas::goto_page() {
	int old_page = cur_layout->get_page();
	int page = goto_line->text().toInt() - 1;
	goto_line->hide();
	if (cur_layout->scroll_page(page, false)) {
		store_jump(old_page);
		update();
	}
}
void Canvas::rotate_left() {
	viewer->get_res()->rotate(-1);
	cur_layout->rebuild();
	update();
}

void Canvas::rotate_right() {
	viewer->get_res()->rotate(1);
	cur_layout->rebuild();
	update();
}

