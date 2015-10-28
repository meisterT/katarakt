#include "beamerwindow.h"
#include "viewer.h"
#include "canvas.h"
#include "config.h"
#include "resourcemanager.h"
#include "layout/singlelayout.h"
#include <QResizeEvent>
#include <QApplication>
#include <iostream>

using namespace std;


BeamerWindow::BeamerWindow(Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v),
		valid(false) {
	setFocusPolicy(Qt::StrongFocus);

	layout = new SingleLayout(viewer, 0);

	CFG *config = CFG::get_instance();
	mouse_wheel_factor = config->get_value("Settings/mouse_wheel_factor").toInt();

	switch (config->get_value("Settings/click_link_button").toInt()) {
		case 1: click_link_button = Qt::LeftButton; break;
		case 2: click_link_button = Qt::RightButton; break;
		case 3: click_link_button = Qt::MidButton; break;
		case 4: click_link_button = Qt::XButton1; break;
		case 5: click_link_button = Qt::XButton2; break;
		default: click_link_button = Qt::NoButton;
	}

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
	if (click_link_button != Qt::NoButton && event->button() == click_link_button) {
		mx_down = event->x();
		my_down = event->y();
	}
}

void BeamerWindow::mouseReleaseEvent(QMouseEvent *event) {
	if (click_link_button != Qt::NoButton && event->button() == click_link_button) {
		if (mx_down == event->x() && my_down == event->y()) {
			pair<int, QPointF> location = layout->get_location_at(mx_down, my_down);
			layout->activate_link(location.first, location.second.x(), location.second.y());
		}
	}
}

void BeamerWindow::wheelEvent(QWheelEvent *event) {
	int d = event->delta();
	if (QApplication::keyboardModifiers() == Qt::NoModifier) {
		if (event->orientation() == Qt::Vertical) {
			viewer->get_canvas()->get_layout()->scroll_page(-d / mouse_wheel_factor);
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

