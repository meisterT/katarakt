#ifndef LAYOUT_H
#define LAYOUT_H

#include <QPainter>
#include <list>
#include <map>


class ResourceManager;
class Grid;
class Result;


class Layout {
public:
	Layout(ResourceManager *_res, int _page = 0);
	virtual ~Layout();

	virtual int get_page() const;
	virtual void rebuild();
	virtual void resize(int w, int h);
	virtual void set_zoom(int new_zoom, bool relative = true);
	virtual void set_columns(int new_columns, bool relative = true);

	virtual bool supports_smooth_scrolling() const;
	virtual bool scroll_smooth(int dx, int dy);
	virtual bool scroll_page(int new_page, bool relative = true);
	virtual void render(QPainter *painter) = 0;

	virtual void clear_hits();
	virtual void set_hits(int page, std::list<Result> *l);
	virtual void set_search_visible(bool visible);
	virtual bool advance_hit(bool forward = true);
	virtual bool advance_invisible_hit(bool forward = true) = 0;

	virtual bool click_mouse(int mx, int my);
	virtual bool goto_page_at(int mx, int my);

	virtual bool get_search_visible() const;
	virtual bool page_visible(int p) const = 0;

protected:
	virtual void view_hit() = 0;

	ResourceManager *res;
	int page;
	int off_x, off_y;
	int width, height;

	// search results
	std::map<int,std::list<Result> *> hits;
	bool search_visible;
	int hit_page;
	std::list<Result>::const_iterator hit_it;

	// config options
	int useless_gap;
	int min_page_width;
	int min_zoom;
	int max_zoom;
	float zoom_factor;
	int prefetch_count;
	float search_padding;
};

class PresentationLayout : public Layout {
public:
	PresentationLayout(ResourceManager *res, int page = 0);
	PresentationLayout(Layout& old_layout);
	~PresentationLayout() {};

	bool supports_smooth_scrolling() const;
	bool scroll_smooth(int dx, int dy);
	void render(QPainter *painter);

	bool advance_hit(bool forward = true);
	bool advance_invisible_hit(bool forward = true);

	bool click_mouse(int mx, int my);

	bool page_visible(int p) const;

private:
	int calculate_fit_width(int page);
	void view_hit();
};

class GridLayout : public Layout {
public:
	GridLayout(ResourceManager *res, int page = 0, int columns = 1);
	GridLayout(Layout& old_layout, int columns = 1);
	~GridLayout();

	void rebuild();
	void resize(int w, int h);
	void set_zoom(int new_zoom, bool relative = true);
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
	void initialize(int columns);
	void set_constants();
	void view_hit();
	void view_hit(const QRect &r);
	QRect get_hit_rect();
	std::pair<int,QPointF> get_page_at(int x, int y);

	Grid *grid;

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

