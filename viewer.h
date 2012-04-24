#ifndef PDFVIEWER_H
#define PDFVIEWER_H

#include <QWidget>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QString>
//#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QCoreApplication>
#include <QResizeEvent>
#include <QKeySequence>
#include <QSocketNotifier>
#include <iostream>
#include <map>
#include <sys/socket.h>

#include "resourcemanager.h"
#include "layout.h"


class Viewer;
typedef void (Viewer::*func_t)();


class Viewer : public QWidget {
	Q_OBJECT

public:
	Viewer(ResourceManager *res, QWidget *parent = 0);
	~Viewer();

	// signal handling
	static void signal_handler(int unused);
public slots:
	void handle_signal();

protected:
	// QT event handling
	bool event(QEvent *event);

	void paintEvent(QPaintEvent *event);
//	void keyPressEvent(QKeyEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void resizeEvent(QResizeEvent *event);

private slots:
	void page_rendered(int page);

private:
	// primitive actions
	void set_presentation_layout();
	void set_grid_layout();
	void page_up();
	void page_down();
	void page_first();
	void page_last();
	void smooth_up();
	void smooth_down();
	void smooth_left();
	void smooth_right();
	void zoom_in();
	void zoom_out();
	void columns_inc();
	void columns_dec();
	void toggle_fullscreen();
	void toggle_overlay();
	void reload();
	void quit();

	void add_sequence(QString key, func_t action);

	ResourceManager *res;
	Layout *layout;

	// key sequences
	std::map<QKeySequence,func_t> sequences;

	int mx, my;

	bool draw_overlay;

	// signal handling
	static int sig_fd[2];
	QSocketNotifier *sig_notifier;

};

#endif

