#ifndef LAYOUT_H
#define LAYOUT_H


#include <iostream>
#include <QPainter>
#include <QImage>

#include "resourcemanager.h"
#include "grid.h"


class Layout {
public:
	Layout(ResourceManager *_res, int _page = 0);
	virtual ~Layout() {};

	virtual int get_page() const;
	virtual void rebuild();
	virtual void resize(int w, int h);
	virtual void set_zoom(int new_zoom, bool relative = true);
	virtual void set_columns(int new_columns, bool relative = true);

	virtual bool supports_smooth_scrolling();
	virtual void scroll_smooth(int dx, int dy);
	virtual void scroll_page(int new_page, bool relative = true);
	virtual void render(QPainter *painter) = 0;

protected:
	ResourceManager *res;
	int page;
	int off_x, off_y;
	int width, height;

};

class PresentationLayout : public Layout {
public:
	PresentationLayout(ResourceManager *res, int page = 0);
	PresentationLayout(Layout& old_layout);
	~PresentationLayout() {};

	bool supports_smooth_scrolling();
	void scroll_smooth(int dx, int dy);
	void render(QPainter *painter);

private:
	int calculate_fit_width(int page);
};

class SequentialLayout : public Layout {
public:
	SequentialLayout(ResourceManager *res, int page = 0);
	SequentialLayout(Layout& old_layout);
	~SequentialLayout() {};

	void scroll_smooth(int dx, int dy);
	void render(QPainter *painter);
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

