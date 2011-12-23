#include "pdfviewer.h"

PdfViewer::PdfViewer(ResourceManager *_res, QWidget *parent) :
		QWidget(parent),
		res(_res),
		page(0),
		scroll(0.0),
		fit(false) {
	setFocusPolicy(Qt::StrongFocus);
	res->setFinishedCallback(&scheduleUpdate, this);
}

PdfViewer::~PdfViewer() {
}

void PdfViewer::paintEvent(QPaintEvent * /*event*/) {
	using namespace std;
	cout << "redraw" << endl;
	QPainter painter(this);

	for (int i = -1; i < 3; ++i) {
		float pageWidth, pageHeight;
		if (!fit) {
			pageWidth = width();
			pageHeight =  pageWidth / res->getPageAspect(page + i);
		} else {
			pageHeight = height();
			pageWidth = res->getPageAspect(page + i) * pageHeight;
		}

		QImage *img = res->getPage(page + i, pageWidth);
		if (img != NULL) {
			QRect target(0, 0, width(), height());
			QRect source(0, scroll * pageHeight - (pageHeight + 2) * i, width(), height());
			painter.drawImage(target, *img, source);
		} else {
		//	cout << "null: " << page << " " << i << endl;
		}
	}
}

void PdfViewer::keyPressEvent(QKeyEvent *event) {
	switch (event->key()) {
		case Qt::Key_Q:
		case Qt::Key_Escape:
			res->wait();
			QCoreApplication::exit(0);
			break;
		case Qt::Key_Space:
		case Qt::Key_Down:
			scroll = 0;
			setPage(1, true);
			break;
		case Qt::Key_Backspace:
		case Qt::Key_Up:
			scroll = 0;
			setPage(-1, true);
			break;
		case Qt::Key_J:
			scrollPage(0, -50);
			break;
		case Qt::Key_K:
			scrollPage(0, 50);
			break;
		case Qt::Key_G:
			scroll = 0;
			setPage(0);
			break;
		case Qt::Key_B:
			scroll = 0;
			setPage(res->numPages() - 1);
			break;
		case Qt::Key_1:
			toggleFit();
			break;
		default:
			QWidget::keyPressEvent(event);
			break;
	}
}

void PdfViewer::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
		mx = event->x();
		my = event->y();
	}
}

void PdfViewer::mouseMoveEvent(QMouseEvent *event) {
	scrollPage(event->x() - mx, event->y() - my);
	mx = event->x();
	my = event->y();
}

void PdfViewer::resizeEvent(QResizeEvent * /*event*/) {
	update();
}

void PdfViewer::scheduleUpdate(PdfViewer *_this) {
	_this->update();
}

void PdfViewer::setPage(int newPage, bool relative) {
	if (relative) {
		newPage += page;
	}
	if (newPage < 0) {
		newPage = 0;
		scroll = 0;
	} else if (newPage >= res->numPages()) {
		newPage = res->numPages() - 1;
	}

	if (page != newPage) {
		QString str = "page ";
		str.append(QString::number(newPage + 1));
		str.append("/");
		str.append(QString::number(res->numPages()));
		setWindowTitle(str);
		page = newPage;
		update();
	}
}

void PdfViewer::toggleFit() {
	fit = not fit;
	update();
}

void PdfViewer::scrollPage(int dx, int dy) {
	if (dy == 0)
		return;

	if (!fit) {
		scroll -= dy / (width() / res->getPageAspect(page));
	} else {
		scroll -= (float) dy / height();
	}

	if (scroll < 0) {
		scroll++;
		setPage(-1, true);
	} else if (scroll >= 1) {
		scroll--;
		setPage(1, true);
	} else {
		update();
	}
}
