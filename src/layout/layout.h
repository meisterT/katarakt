#ifndef LAYOUT_H
#define LAYOUT_H

#include <QPainter>
#include <QList>
#include <map>


class Viewer;
class ResourceManager;
class Grid;


class Layout {
public:
	Layout(Viewer *v, int _page = 0);
	virtual ~Layout();

	virtual int get_page() const;
	void activate(const Layout *old_layout);
	virtual void rebuild(bool clamp = true);
	virtual void resize(int w, int h);
	virtual bool set_zoom(int new_zoom, bool relative = true);
	virtual void set_columns(int new_columns, bool relative = true);

	virtual bool supports_smooth_scrolling() const;
	virtual bool scroll_smooth(int dx, int dy);
	virtual bool scroll_page(int new_page, bool relative = true);
	virtual void render(QPainter *painter) = 0;

	virtual void update_search(int page);
	virtual void set_search_visible(bool visible);
	virtual bool advance_hit(bool forward = true);
	virtual bool advance_invisible_hit(bool forward = true) = 0;

	virtual bool click_mouse(int mx, int my);
	virtual bool goto_page_at(int mx, int my);

	virtual bool get_search_visible() const;
	virtual bool page_visible(int p) const = 0;

protected:
	virtual void view_hit() = 0;

	Viewer *viewer;
	ResourceManager *res;
	int page;
	int width, height;

	// search results
	bool search_visible;
	int hit_page;
	QList<QRectF>::const_iterator hit_it;

	// config options
	int useless_gap;
	int min_page_width;
	int min_zoom;
	int max_zoom;
	float zoom_factor;
	int prefetch_count;
	float search_padding;
};


#endif

