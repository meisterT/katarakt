#ifndef CANVAS_H
#define CANVAS_H

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
#include <iostream>
#include <map>
#include <list>
#include <sys/socket.h>


class Viewer;
class Layout;
class Result;
class GotoLine;


class Canvas : public QWidget {
	Q_OBJECT

	typedef void (Canvas::*func_t)();

public:
	Canvas(Viewer *v, QWidget *parent = 0);
	~Canvas();

	bool is_valid() const;
	void reload();

	const Layout *get_layout() const;

protected:
	// QT event handling
	bool event(QEvent *event);

	void paintEvent(QPaintEvent *event);
//	void keyPressEvent(QKeyEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void resizeEvent(QResizeEvent *event);

private slots:
	void search_clear();
	void search_done(int page, std::list<Result> *l);
	void search_visible(bool visible);
	void page_rendered(int page);
	void goto_page();

private:
	// primitive actions
	void set_presentation_layout();
	void set_grid_layout();
	void page_up();
	void page_down();
	void page_first();
	void page_last();
	void auto_smooth_up();
	void auto_smooth_down();
	void smooth_up();
	void smooth_down();
	void smooth_left();
	void smooth_right();
	void zoom_in();
	void zoom_out();
	void reset_zoom();
	void columns_inc();
	void columns_dec();
	void toggle_overlay();
	void quit();
	void search();
	void next_hit();
	void previous_hit();
	void focus_goto();

	void add_sequence(QString key, func_t action);

	Viewer *viewer;
	Layout *layout;

	GotoLine *goto_line;

	// key sequences
	std::map<QKeySequence,func_t> sequences;

	int mx, my;
	int mx_down, my_down;

	bool draw_overlay;

	bool valid;

};

#endif

