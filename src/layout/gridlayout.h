#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include "layout.h"


class Grid;


class GridLayout : public Layout {
public:
	GridLayout(Viewer *v, int page = 0, int columns = 1);
	GridLayout(Layout& old_layout, int columns = 1);
	~GridLayout();

	int get_page() const;

	void activate(const Layout *old_layout);
	void rebuild(bool clamp = true);
	void resize(int w, int h);
	bool set_zoom(int new_zoom, bool relative = true);
	bool set_columns(int new_columns, bool relative = true);
	bool set_offset(int new_offset, bool relative = true);

	bool scroll_smooth(int dx, int dy);
	bool scroll_page(int new_page, bool relative = true);
	void render(QPainter *painter);

	bool advance_hit(bool forward = true);
	bool advance_invisible_hit(bool forward = true);

	bool click_mouse(int mx, int my);
	bool goto_link_destination(Poppler::LinkDestination *link);
	bool goto_page_at(int mx, int my);

	bool page_visible(int p) const;

private:
	void initialize(int columns, int offset, bool clamp = true);
	void set_constants(bool clamp = true);
	bool view_hit();
	bool view_rect(const QRect &r);
	bool view_point(const QPoint &p);
	QRect get_target_rect(int target_page, QRectF target_rect) const;
	QPoint get_target_page_distance(int target_page) const;
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

