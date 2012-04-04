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

	virtual int get_page();
	virtual void rebuild();

	virtual void scroll_smooth(int dx, int dy);
	virtual void scroll_page(int new_page, bool relative = true);
	virtual void resize(float w, float h);
	virtual void render(QPainter *painter) = 0;

protected:
	ResourceManager *res;
	int page;
	int off_x, off_y;
	float width, height;

};

class PresentationLayout : public Layout {
public:
	PresentationLayout(ResourceManager *res, int page = 0);
	PresentationLayout(Layout& old_layout);
	~PresentationLayout() {};

	void scroll_smooth(int dx, int dy);
	void render(QPainter *painter);
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
	GridLayout(ResourceManager *res, int page = 0, int columns = 5);
	GridLayout(Layout& old_layout, int columns = 5);
	~GridLayout();

	void rebuild();

	void scroll_smooth(int dx, int dy);
	void scroll_page(int new_page, bool relative = true);

	void render(QPainter *painter);

private:
	void initialize(int columns);

	Grid *grid;

	int horizontal_page;
	float zoom;
	float total_width;
	float total_height;
};


#endif

