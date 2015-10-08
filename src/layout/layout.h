#ifndef LAYOUT_H
#define LAYOUT_H

#include <QPainter>
#include <QList>
#include <QClipboard>
#include <poppler/qt4/poppler-qt4.h>
#include <map>
#include "../selection.h"


class Viewer;
class ResourceManager;
class Grid;
namespace Poppler {
	class LinkDestination;
}


class Layout {
public:
	Layout(Viewer *v, int render_index, int _page = 0);
	virtual ~Layout();

	virtual void activate(const Layout *old_layout);
	virtual void rebuild(bool clamp = true);
	virtual void resize(int w, int h);

	// normal movement
	virtual void scroll_smooth(int dx, int dy);
	virtual void scroll_page(int new_page, bool relative = true);

	// jump movement
	virtual void scroll_page_jump(int new_page, bool relative = true);

	virtual void update_search();
	virtual void advance_hit(bool forward = true);
	virtual void advance_invisible_hit(bool forward = true) = 0;

	virtual void activate_link(int page, float x, float y);
	virtual void goto_link_destination(const Poppler::LinkDestination &link);
	virtual void goto_position(int page, QPointF pos);
	virtual void goto_page_at(int mx, int my);

	// misc actions
	virtual void render(QPainter *painter) = 0;

	virtual void set_zoom(int new_zoom, bool relative = true);
	virtual void set_columns(int new_columns, bool relative = true);
	virtual void set_offset(int new_offset, bool relative = true);
	virtual void set_search_visible(bool visible);

	void select(int px, int py, enum Selection::Mode mode);
	void clear_selection();

	// misc getters
	virtual int get_page() const;
	virtual bool supports_smooth_scrolling() const;
	virtual bool get_search_visible() const;
	virtual bool page_visible(int p) const = 0;
	virtual std::pair<int, QPointF> get_location_at(int px, int py) const = 0;
	void copy_selection_text(QClipboard::Mode mode = QClipboard::Selection) const;

protected:
	// internal functions for nested use
	// they don't call the viewer that stuff needs updating
	bool scroll_page_noupdate(int new_page, bool relative = true);
	bool advance_hit_noupdate(bool forward = true);

	void render_search_rects(QPainter *painter, int cur_page, QPoint offset, float size);
	void render_selection(QPainter *painter, int cur_page, QPoint offset, float size);
	virtual void view_hit();

	Viewer *viewer;
	ResourceManager *res;
	int render_index;
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

	MouseSelection selection;
};


#endif

