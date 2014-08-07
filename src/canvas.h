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
class GotoLine;


class Canvas : public QWidget {
	Q_OBJECT

public:
	Canvas(Viewer *v, QWidget *parent = 0);
	~Canvas();

	bool is_valid() const;
	void reload(bool clamp);

	const Layout *get_layout() const;

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
	void search_clear();
	void search_done(int page, QList<QRectF> *l);
	void search_visible(bool visible);
	void page_rendered(int page);
	void goto_page();

	// primitive actions
	void set_presentation_layout();
	void set_grid_layout();
	void page_up();
	void page_down();
	void page_first();
	void page_last();
	void half_screen_up();
	void half_screen_down();
	void screen_up();
	void screen_down();
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
	void next_invisible_hit();
	void previous_invisible_hit();
	void focus_goto();
	void rotate_left();
	void rotate_right();

private:
	void add_action(const char *action, const char *slot);

	Viewer *viewer;
	Layout *cur_layout;
	PresentationLayout *presentation_layout;
	GridLayout *grid_layout;

	GotoLine *goto_line;

	int mx, my;
	int mx_down, my_down;

	bool draw_overlay;

	bool valid;

	// config options
	int background_opacity;
	int mouse_wheel_factor;
	int smooth_scroll_delta;
	float screen_scroll_factor;
};

#endif

