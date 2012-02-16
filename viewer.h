#ifndef PDFVIEWER_H
#define PDFVIEWER_H

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QString>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QCoreApplication>
#include <QResizeEvent>
#include <iostream>

#include "resourcemanager.h"
#include "layout.h"

class Viewer : public QWidget {
	Q_OBJECT

public:
	Viewer(ResourceManager *res, QWidget *parent = 0);
	~Viewer();

protected:
	// QT event handling
	void paintEvent(QPaintEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);

	// I don't like member function pointers
	// gets called when a page is ready -> need redraw
	static void scheduleUpdate(Viewer *_this);

private:
	ResourceManager *res;
	Layout *layout;

	int mx, my;

};

#endif

