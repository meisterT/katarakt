#include "beamerwindow.h"
#include "viewer.h"
#include "canvas.h"
#include "config.h"
#include "resourcemanager.h"
#include "layout/presentationlayout.h"
#include <QResizeEvent>
#include <QApplication>
#include <iostream>

using namespace std;


BeamerWindow::BeamerWindow(Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v),
		valid(false) {
	setFocusPolicy(Qt::StrongFocus);

	layout = new PresentationLayout(viewer);

	CFG *config = CFG::get_instance();
	mouse_wheel_factor = config->get_value("mouse_wheel_factor").toInt();

	valid = true;
}

BeamerWindow::~BeamerWindow() {
	delete layout;
}

bool BeamerWindow::is_valid() const {
	return valid;
}

Layout *BeamerWindow::get_layout() const {
	return layout;
}

void BeamerWindow::set_page(int page) {
	// TODO calls itself if page != layout->get_page(), so once too often
	if (layout->scroll_page(page, false)) {
		update();
	}
}

void BeamerWindow::toggle_fullscreen() {
	setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void BeamerWindow::paintEvent(QPaintEvent * /*event*/) {
#ifdef DEBUG
	cerr << "redraw beamer" << endl;
#endif
	QPainter painter(this);
	painter.fillRect(rect(), QColor(0, 0, 0));
	layout->render(&painter);
}

void BeamerWindow::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		mx_down = event->x();
		my_down = event->y();
	}
}

void BeamerWindow::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		if (mx_down == event->x() && my_down == event->y()) {
			int page = layout->get_page();
			pair<int, QPointF> location = layout->get_location_at(mx_down, my_down);
			if (layout->activate_link(location.first, location.second.x(), location.second.y())) {
				if (viewer->get_canvas()->get_layout()->scroll_page(layout->get_page(), false)) {
					viewer->get_canvas()->update();
				}
				viewer->get_res()->store_jump(page); // store old position if a clicked link moved the view
				update();
			}
		}
	}
}

void BeamerWindow::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	if (QApplication::keyboardModifiers() == Qt::NoModifier) {
		if (event->orientation() == Qt::Vertical) {
			if (viewer->get_canvas()->get_layout()->scroll_page(-d / mouse_wheel_factor)) {
				viewer->get_canvas()->update();
			}
		}
	}
}

void BeamerWindow::resizeEvent(QResizeEvent *event) {
	layout->resize(event->size().width(), event->size().height());
	update();
}

void BeamerWindow::page_rendered(int page) {
	if (layout->page_visible(page)) {
		update();
	}
}

