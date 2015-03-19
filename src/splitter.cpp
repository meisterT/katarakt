#include "splitter.h"

SplitterHandle::SplitterHandle(Qt::Orientation orientation, QSplitter *parent) :
		QSplitterHandle(orientation, parent) {
}

void SplitterHandle::paintEvent(QPaintEvent *event) {
	QPainter painter(this);
	// draw background
	// couldn't find another way to make the contents of a splitter transparent, but not the splitter itself
	painter.fillRect(rect(), QApplication::palette().window());
	// draw handle
	QSplitterHandle::paintEvent(event);
}


Splitter::Splitter(QWidget *parent) :
		QSplitter(parent) {
}

Splitter::Splitter(Qt::Orientation orientation, QWidget *parent) :
		QSplitter(orientation, parent) {
}

QSplitterHandle *Splitter::createHandle() {
	return new SplitterHandle(orientation(), this);
}

