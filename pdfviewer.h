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

class PdfViewer : public QWidget {
	Q_OBJECT

public:
	PdfViewer(ResourceManager *res, QWidget *parent = 0);
	~PdfViewer();

protected:
	void paintEvent(QPaintEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void resizeEvent(QResizeEvent *event);

	void setPage(int newPage, bool relative = false);
	void toggleFit();
	void scrollPage(int dx, int dy);

private:
	ResourceManager *res;
	int page;
	int pageHeight;
	float scroll;
	bool fit;

	int mx, my;

};

#endif

