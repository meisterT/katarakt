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
	GridLayout(ResourceManager *res, int page = 0, int columns = 2);
	GridLayout(Layout& old_layout, int columns = 2);
	~GridLayout();

	void scroll_smooth(int dx, int dy);
	void scroll_page(int new_page, bool relative = true);

	void render(QPainter *painter);

private:
	Grid *grid;

	int horizontal_page;
};


#endif

