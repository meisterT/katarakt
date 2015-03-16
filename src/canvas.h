#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QList>
#include <sys/socket.h>


class Viewer;
class Layout;
class PresentationLayout;
class GridLayout;
class PresenterLayout;
class GotoLine;


class Canvas : public QWidget {
	Q_OBJECT

public:
	Canvas(Viewer *v, QWidget *parent = 0);
	~Canvas();

	bool is_valid() const;
	void reload(bool clamp);

	void set_search_visible(bool visible);

	Layout *get_layout() const;

protected:
	// QT event handling
	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void resizeEvent(QResizeEvent *event);

private slots:
	void page_rendered(int page);
	void goto_page();

	// primitive actions
	void set_presentation_layout();
	void set_grid_layout();
	void set_presenter_layout();

	void toggle_overlay();
	void focus_goto();

private:
	void setup_keys(QWidget *base);

	Viewer *viewer;
	Layout *cur_layout;
	PresentationLayout *presentation_layout;
	GridLayout *grid_layout;
	PresenterLayout *presenter_layout;

	GotoLine *goto_line;

	int mx, my;
	int mx_down, my_down;

	bool draw_overlay;

	bool valid;

	// config options
	int background_opacity;
	int mouse_wheel_factor;
};

#endif

