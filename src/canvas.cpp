#include <QAction>
#include <QStringListIterator>
#include <QKeySequence>
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
	// setup keys
	add_action("set_presentation_layout", SLOT(set_presentation_layout()));
	add_action("set_grid_layout", SLOT(set_grid_layout()));
	add_action("set_presenter_layout", SLOT(set_presenter_layout()));
	add_action("toggle_overlay", SLOT(toggle_overlay()));
	add_action("focus_goto", SLOT(focus_goto()));

	// prints the string representation of a key
//	cerr << QKeySequence(Qt::Key_Equal).toString().toUtf8().constData() << endl;

	goto_line = new GotoLine(viewer->get_res()->get_page_count(), this);
	goto_line->move(0, height() - goto_line->height()); // TODO goto_line->height() reports the wrong size
	goto_line->hide();
	connect(goto_line, SIGNAL(returnPressed()), this, SLOT(goto_page()), Qt::UniqueConnection);
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

void Canvas::add_action(const char *action, const char *slot) {
	QStringListIterator i(CFG::get_instance()->get_keys(action));
	while (i.hasNext()) {
		QAction *a = new QAction(this);
		a->setShortcut(QKeySequence(i.next()));
		addAction(a);
		connect(a, SIGNAL(triggered()), this, slot);
	}
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
				viewer->get_res()->store_jump(page); // store old position if a clicked link moved the view
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


void Canvas::toggle_overlay() {
	draw_overlay = not draw_overlay;
	update();
}

void Canvas::focus_goto() {
	goto_line->setFocus();
	goto_line->setText(QString::number(cur_layout->get_page() + 1));
	goto_line->selectAll();
	goto_line->move(0, height() - goto_line->height()); // TODO this is only necessary because goto_line->height() is wrong in the beginning
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
		viewer->get_res()->store_jump(old_page);
		update();
	}
}

