#include <QAction>
#include <QStringListIterator>
#include <QKeySequence>
#include <QString>
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QTimer>
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
#include "beamerwindow.h"
#include "util.h"

using namespace std;


Canvas::Canvas(Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v),
		triple_click_possible(false),
		draw_overlay(true),
		valid(true) {
	setFocusPolicy(Qt::StrongFocus);

	// load config options
	CFG *config = CFG::get_instance();

	{
		bool ok;
		unsigned int color = config->get_value("background_color").toString().toUInt(&ok, 16);
		if (ok) {
			background.setRgba(color);
		} else {
			cerr << "failed to parse background_color" << endl;
		}
	}
	{
		bool ok;
		unsigned int color = config->get_value("background_color_fullscreen").toString().toUInt(&ok, 16);
		if (ok) {
			background_fullscreen.setRgba(color);
		} else {
			cerr << "failed to parse background_color_fullscreen" << endl;
		}
	}
	mouse_wheel_factor = config->get_value("mouse_wheel_factor").toInt();

	switch (config->get_value("click_link_button").toInt()) {
		case 1: click_link_button = Qt::LeftButton; break;
		case 2: click_link_button = Qt::RightButton; break;
		case 3: click_link_button = Qt::MidButton; break;
		case 4: click_link_button = Qt::XButton1; break;
		case 5: click_link_button = Qt::XButton2; break;
		default: click_link_button = Qt::NoButton;
	}

	switch (config->get_value("drag_view_button").toInt()) {
		case 1: drag_view_button = Qt::LeftButton; break;
		case 2: drag_view_button = Qt::RightButton; break;
		case 3: drag_view_button = Qt::MidButton; break;
		case 4: drag_view_button = Qt::XButton1; break;
		case 5: drag_view_button = Qt::XButton2; break;
		default: drag_view_button = Qt::NoButton;
	}

	switch (config->get_value("select_text_button").toInt()) {
		case 1: select_text_button = Qt::LeftButton; break;
		case 2: select_text_button = Qt::RightButton; break;
		case 3: select_text_button = Qt::MidButton; break;
		case 4: select_text_button = Qt::XButton1; break;
		case 5: select_text_button = Qt::XButton2; break;
		default: select_text_button = Qt::NoButton;
	}

	presentation_layout = new PresentationLayout(viewer, 0);
	grid_layout = new GridLayout(viewer, 0);
	presenter_layout = new PresenterLayout(viewer, 1); // TODO add config string

	QString default_layout = config->get_value("default_layout").toString();
	if (default_layout == "grid") {
		cur_layout = grid_layout;
	} else { // "presentation" and everything else
		cur_layout = presentation_layout;
	}

	// apply start option
	cur_layout->scroll_page(config->get_tmp_value("start_page").toInt(), false);

	setup_keys(this);

	if (drag_view_button == Qt::LeftButton) {
		setCursor(Qt::OpenHandCursor);
	} else {
		setCursor(Qt::IBeamCursor);
	}

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	goto_line = new GotoLine(viewer->get_res()->get_page_count(), this);
	goto_line->move(0, height() - goto_line->height()); // TODO goto_line->height() reports the wrong size
	goto_line->hide();
	connect(goto_line, SIGNAL(returnPressed()), this, SLOT(goto_page()), Qt::UniqueConnection);

	// setup beamer
	BeamerWindow *beamer = viewer->get_beamer();
	setup_keys(beamer);
	if (cur_layout == presenter_layout) {
		beamer->show();
		viewer->show_progress(true);
	} else {
		beamer->hide();
		viewer->show_progress(false);
	}
}

Canvas::~Canvas() {
	delete goto_line;
	delete presentation_layout;
	delete grid_layout;
	delete presenter_layout;
}

bool Canvas::is_valid() const {
	return valid;
}

void Canvas::reload(bool clamp) {
	cur_layout->rebuild(clamp);
	goto_line->set_page_count(viewer->get_res()->get_page_count());
	update();
}

void Canvas::setup_keys(QWidget *base) {
	add_action(base, "set_presentation_layout", SLOT(set_presentation_layout()), this);
	add_action(base, "set_grid_layout", SLOT(set_grid_layout()), this);
	add_action(base, "set_presenter_layout", SLOT(set_presenter_layout()), this);
	add_action(base, "toggle_overlay", SLOT(toggle_overlay()), this);
	add_action(base, "focus_goto", SLOT(focus_goto()), this);
	add_action(base, "swap_selection_and_panning_buttons", SLOT(swap_selection_and_panning_buttons()), this);
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
		painter.fillRect(rect(), background_fullscreen);
	} else {
		painter.fillRect(rect(), background);
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
	if ((click_link_button != Qt::NoButton && event->button() == click_link_button)
			|| (drag_view_button != Qt::NoButton && event->button() == drag_view_button)) {
		mx = event->x();
		my = event->y();
		mx_down = mx;
		my_down = my;
	}
	if (drag_view_button != Qt::NoButton && event->button() == drag_view_button) {
		if (cursor().shape() != Qt::PointingHandCursor) { // TODO
			setCursor(Qt::ClosedHandCursor);
		}
	}
	if (select_text_button != Qt::NoButton && event->button() == select_text_button) {
		if (triple_click_possible) {
			cur_layout->select(event->x(), event->y(), Selection::StartLine);
			triple_click_possible = false;
		} else {
			cur_layout->select(event->x(), event->y(), Selection::Start);
		}

		if (cursor().shape() != Qt::PointingHandCursor) { // TODO
			setCursor(Qt::IBeamCursor);
		}
	}
}

void Canvas::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton && event->modifiers() == Qt::ControlModifier) {
		// emit synctex signal
		pair<int, QPointF> location = cur_layout->get_location_at(event->x(), event->y());
		// scale from [0,1] to points
		location.second.rx() *= viewer->get_res()->get_page_width(location.first, false);
		location.second.ry() *= viewer->get_res()->get_page_height(location.first, false);

		emit synchronize_editor(location.first, (int) ROUND(location.second.x()), (int) ROUND(location.second.y()));
	} else if (click_link_button != Qt::NoButton && event->button() == click_link_button) {
		if (mx_down == event->x() && my_down == event->y()) {
			pair<int, QPointF> location = cur_layout->get_location_at(mx_down, my_down);
			cur_layout->activate_link(location.first, location.second.x(), location.second.y());
		}
	}

	if (drag_view_button == Qt::LeftButton) {
		setCursor(Qt::OpenHandCursor);
	} else {
		setCursor(Qt::IBeamCursor);
	}

	if (select_text_button != Qt::NoButton && event->button() == select_text_button) {
		cur_layout->copy_selection_text();
	}
}

void Canvas::mouseMoveEvent(QMouseEvent *event) {
	if (drag_view_button != Qt::NoButton && event->buttons() & drag_view_button) {
		cur_layout->scroll_smooth(event->x() - mx, event->y() - my);
		mx = event->x();
		my = event->y();
	}
	if (select_text_button != Qt::NoButton && event->buttons() & select_text_button) {
		cur_layout->select(event->x(), event->y(), Selection::End);

		// scrolling by dragging the selection
		// TODO only scrolls when the mouse is moved
		int margin = min(10, min(width() / 10, height() / 10));
		if (event->x() < margin) {
			cur_layout->scroll_smooth(min(margin - event->x(), margin) * 2, 0);
		}
		if (event->x() > width() - margin) {
			cur_layout->scroll_smooth(max(width() - event->x() - margin, -margin) * 2, 0);
		}
		if (event->y() < margin) {
			cur_layout->scroll_smooth(0, min(margin - event->y(), margin) * 2);
		}
		if (event->y() > height() - margin) {
			cur_layout->scroll_smooth(0, max(height() - event->y() - margin, -margin) * 2);
		}
	}
}

void Canvas::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	switch (QApplication::keyboardModifiers()) {
		// scroll
		case Qt::NoModifier:
			if (event->orientation() == Qt::Vertical) {
				if (cur_layout->supports_smooth_scrolling()) {
					cur_layout->scroll_smooth(0, d);
				} else {
					cur_layout->scroll_page(-d / mouse_wheel_factor);
				}
			} else {
				cur_layout->scroll_smooth(d, 0);
			}
			break;

		// zoom
		case Qt::ControlModifier:
			cur_layout->set_zoom(d / mouse_wheel_factor);
			break;
	}
}

void Canvas::mouseDoubleClickEvent(QMouseEvent * event) {
	if (click_link_button != Qt::NoButton && event->button() == drag_view_button) {
		cur_layout->goto_page_at(event->x(), event->y());
	}
	if (select_text_button != Qt::NoButton && event->button() == select_text_button) {
		// enable triple click, disable after timeout
		triple_click_possible = true;
		QTimer::singleShot(QApplication::doubleClickInterval(), this, SLOT(disable_triple_click()));

		cur_layout->select(event->x(), event->y(), Selection::StartWord);
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
	viewer->get_beamer()->hide();
	viewer->show_progress(false);
	viewer->activateWindow();
}

void Canvas::set_grid_layout() {
	grid_layout->activate(cur_layout);
	grid_layout->rebuild();
	cur_layout = grid_layout;
	update();
	viewer->get_beamer()->hide();
	viewer->show_progress(false);
	viewer->activateWindow();
}

void Canvas::set_presenter_layout() {
	presenter_layout->activate(cur_layout);
	presenter_layout->rebuild();
	cur_layout = presenter_layout;
	update();

	// TODO move beamer to second screen
//	QDesktopWidget *desktop = QApplication::desktop();
//	if (desktop->screenCount() > 1) {
//		int primary_screen = desktop->primaryScreen();
//		int cur_screen = desktop->screenNumber(viewer);
//		cout << "primary: " << primary_screen << ", current: " << cur_screen << endl;
//	}
	viewer->get_beamer()->get_layout()->scroll_page(cur_layout->get_page(), false);
	viewer->get_beamer()->show();
	viewer->show_progress(true);
}

void Canvas::toggle_overlay() {
	draw_overlay = not draw_overlay;
	update();
}

void Canvas::focus_goto() {
	goto_line->activateWindow();
	goto_line->show();
	goto_line->setFocus();
	goto_line->setText(QString::number(cur_layout->get_page() + 1));
	goto_line->selectAll();
	goto_line->move(0, height() - goto_line->height()); // TODO this is only necessary because goto_line->height() is wrong in the beginning
}

void Canvas::disable_triple_click() {
	triple_click_possible = false;
}

void Canvas::swap_selection_and_panning_buttons() {
	Qt::MouseButton tmp = drag_view_button;
	drag_view_button = select_text_button;
	select_text_button = tmp;

	if (drag_view_button == Qt::LeftButton) {
		setCursor(Qt::OpenHandCursor);
	} else {
		setCursor(Qt::IBeamCursor);
	}
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
	int page = goto_line->text().toInt() - 1;
	goto_line->hide();
	cur_layout->scroll_page_jump(page, false);
}

