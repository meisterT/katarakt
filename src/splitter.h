#ifndef SPLITTER_H
#define SPLITTER_H

#include <QSplitter>
#include <QSplitterHandle>
#include <QPaintEvent>
#include <QApplication>
#include <QPainter>


class SplitterHandle : public QSplitterHandle {
	Q_OBJECT

public:
	SplitterHandle(Qt::Orientation orientation, QSplitter *parent);

protected:
	void paintEvent(QPaintEvent *event);
};


class Splitter : public QSplitter {
	Q_OBJECT

public:
	Splitter(QWidget *parent = 0);
	Splitter(Qt::Orientation orientation, QWidget *parent = 0);

protected:
	QSplitterHandle *createHandle();
};

#endif

