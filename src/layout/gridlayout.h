#ifndef GRIDLAYOUT_H
#define GRIDLAYOUT_H

#include "layout.h"


class Grid;


class GridLayout : public Layout {
public:
	GridLayout(Viewer *v, int render_index, int page = 0, int columns = 1);
	~GridLayout();

	int get_page() const;

	void activate(const Layout *old_layout);
	void rebuild(bool clamp = true);
	void resize(int w, int h);
	void set_zoom(int new_zoom, bool relative = true);
	void set_columns(int new_columns, bool relative = true);
	void set_offset(int new_offset, bool relative = true);

	void scroll_smooth(int dx, int dy);
	void scroll_page(int new_page, bool relative = true);
	void render(QPainter *painter);

	void advance_invisible_hit(bool forward = true);

	std::pair<int, QPointF> get_location_at(int pixel_x, int pixel_y) const;
	void goto_link_destination(const Poppler::LinkDestination &link);
	void goto_position(int page, QPointF pos);
	void goto_page_at(int mx, int my);

	bool page_visible(int p) const;

	bool supports_smooth_scrolling() const;

protected:
	// internal functions for nested use
	// they don't call the viewer that stuff needs updating
	bool set_columns_noupdate(int new_columns, bool relative = true);
	bool scroll_smooth_noupdate(int dx, int dy);
	bool scroll_page_noupdate(int new_page, bool relative = true);

private:
	void initialize(int columns, int offset, bool clamp = true);
	void set_constants(bool clamp = true);
	void view_hit();
	void view_rect(const QRect &r);
	void view_point(const QPoint &p);
	QRect get_target_rect(int target_page, QRectF target_rect) const;
	QPoint get_target_page_distance(int target_page) const;

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

