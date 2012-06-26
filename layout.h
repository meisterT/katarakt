#ifndef LAYOUT_H
#define LAYOUT_H

#include <iostream>
#include <QPainter>
#include <QImage>
#include <list>
#include <map>


class ResourceManager;
class Grid;
class Result;


class Layout {
public:
	Layout(ResourceManager *_res, int _page = 0);
	virtual ~Layout() {};

	virtual int get_page() const;
	virtual void rebuild();
	virtual void resize(int w, int h);
	virtual void set_zoom(int new_zoom, bool relative = true);
	virtual void set_columns(int new_columns, bool relative = true);

	virtual bool supports_smooth_scrolling() const;
	virtual void scroll_smooth(int dx, int dy);
	virtual void scroll_page(int new_page, bool relative = true);
	virtual void render(QPainter *painter) = 0;

	virtual void clear_hits();
	virtual void set_hits(int page, std::list<Result> *l);
	virtual void set_search_visible(bool visible);
	virtual void advance_hit(bool forward = true);

	virtual bool click_mouse(int mx, int my);

	virtual bool get_search_visible() const;

protected:
	ResourceManager *res;
	int page;
	int off_x, off_y;
	int width, height;

	// search results
	std::map<int,std::list<Result> *> hits;
	bool search_visible;
	int hit_page;
	std::list<Result>::const_iterator hit_it;

};

class PresentationLayout : public Layout {
public:
	PresentationLayout(ResourceManager *res, int page = 0);
	PresentationLayout(Layout& old_layout);
	~PresentationLayout() {};

	bool supports_smooth_scrolling() const;
	void scroll_smooth(int dx, int dy);
	void render(QPainter *painter);

	void advance_hit(bool forward = true);

	bool click_mouse(int mx, int my);

private:
	int calculate_fit_width(int page);
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

	void scroll_smooth(int dx, int dy);
	void scroll_page(int new_page, bool relative = true);
	void render(QPainter *painter);

	void advance_hit(bool forward = true);

	bool click_mouse(int mx, int my);

private:
	void initialize(int columns);
	void set_constants();

	Grid *grid;

	int horizontal_page;
	float size;
	int zoom;
	int total_width;
	int total_height;

	int border_page_w, border_off_w;
	int border_page_h, border_off_h;
};


#endif

