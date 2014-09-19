#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include "layout.h"


class Grid;


class GridLayout : public Layout {
public:
	GridLayout(Viewer *v, int page = 0, int columns = 1);
	GridLayout(Layout& old_layout, int columns = 1);
	~GridLayout();

	void rebuild(bool clamp = true);
	void resize(int w, int h);
	bool set_zoom(int new_zoom, bool relative = true);
	void set_columns(int new_columns, bool relative = true);

	bool scroll_smooth(int dx, int dy);
	bool scroll_page(int new_page, bool relative = true);
	void render(QPainter *painter);

	bool advance_hit(bool forward = true);
	bool advance_invisible_hit(bool forward = true);

	bool click_mouse(int mx, int my);
	bool goto_page_at(int mx, int my);

	bool page_visible(int p) const;

private:
	void initialize(int columns, bool clamp = true);
	void set_constants(bool clamp = true);
	void view_hit();
	void view_hit(const QRect &r);
	QRect get_hit_rect();
	std::pair<int,QPointF> get_page_at(int x, int y);

	Grid *grid;

	int off_x, off_y;
	int horizontal_page;
	int last_visible_page;
	float size;
	int zoom;
	int total_width;
	int total_height;

	int border_page_w, border_off_w;
	int border_page_h, border_off_h;
};


#endif

