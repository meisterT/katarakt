#include "viewer.h"

using namespace std;


Viewer::Viewer(ResourceManager *_res, QWidget *parent) :
		QWidget(parent),
		res(_res) {
	setFocusPolicy(Qt::StrongFocus);
	res->setFinishedCallback(&scheduleUpdate, this);

	layout = new PresentationLayout(res);
//	layout = new SequentialLayout(res);
}

Viewer::~Viewer() {
	delete layout;
}

void Viewer::paintEvent(QPaintEvent * /*event*/) {
//	cerr << "redraw" << endl;
	QPainter painter(this);
	layout->render(&painter);

	QString title = QString("page %1/%2")
		.arg(layout->get_page() + 1)
		.arg(res->get_page_count());
	QRect size = QRect(0, 0, width(), height());
	int flags = Qt::AlignBottom | Qt::AlignRight;
	QRect bounding = painter.boundingRect(size, flags, title);

	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor("#eeeeec"));
	painter.drawRect(bounding);
	painter.setPen(QColor("#2e3436"));
	painter.drawText(size, flags, title);

	setWindowTitle(title);
}

void Viewer::keyPressEvent(QKeyEvent *event) {
	switch (event->key()) {

		// quit
		case Qt::Key_Q:
		case Qt::Key_Escape:
			QCoreApplication::exit(0);
			break;

		// scroll whole page
		case Qt::Key_Space:
		case Qt::Key_Down:
			layout->scroll_page(1);
			update();
			break;
		case Qt::Key_Backspace:
		case Qt::Key_Up:
			layout->scroll_page(-1);
			update();
			break;

		// scroll smoothly
		case Qt::Key_J:
			layout->scroll_smooth(0, -30);
			update();
			break;
		case Qt::Key_K:
			layout->scroll_smooth(0, 30);
			update();
			break;

		// scroll to absolute position
		case Qt::Key_G:
			// TODO off_y = 0
			layout->scroll_page(0, false);
			update();
			break;
		case Qt::Key_B:
			layout->scroll_page(res->get_page_count() - 1, false);
			update();
			break;

		// change layout
		case Qt::Key_1:
			{
				Layout *old_layout = layout;
				layout = new PresentationLayout(*old_layout);
				delete old_layout;
				update();
				break;
			}
		case Qt::Key_2:
			{
				Layout *old_layout = layout;
				layout = new SequentialLayout(*old_layout);
				delete old_layout;
				update();
				break;
			}
		case Qt::Key_3:
			{
				Layout *old_layout = layout;
				layout = new GridLayout(*old_layout);
				delete old_layout;
				update();
				break;
			}

		// set columns
		case Qt::Key_BracketLeft:
			layout->set_columns(-1);
			update();
			break;
		case Qt::Key_BracketRight:
			layout->set_columns(1);
			update();
			break;

		// toggle fullscreen
		case Qt::Key_F5:
		case Qt::Key_F:
			 setWindowState(windowState() ^ Qt::WindowFullScreen);
			 update();
			 break;

		// reload document
		case Qt::Key_R:
			res->reload_document();
			layout->rebuild();
			update();
			break;

		default:
			QWidget::keyPressEvent(event);
			break;
	}
}

void Viewer::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		mx = event->x();
		my = event->y();
	}
}

void Viewer::mouseMoveEvent(QMouseEvent *event) {
	layout->scroll_smooth(event->x() - mx, event->y() - my);
	mx = event->x();
	my = event->y();
	update(); // TODO don't do this here
}

void Viewer::resizeEvent(QResizeEvent *event) {
	layout->resize(event->size().width(), event->size().height());
	update();
}

void Viewer::scheduleUpdate(Viewer *_this) {
	_this->update();
}

