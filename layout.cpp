#include "layout.h"

using namespace std;


// TODO put in a config source file
#define USELESS_GAP 1


//==[ Layout ]=================================================================
Layout::Layout(ResourceManager *_res, int _page) :
		res(_res), page(_page), off_x(0), off_y(0), width(0), height(0) {
}

int Layout::get_page() {
	return page;
}

void Layout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;
}

void Layout::scroll_page(int new_page, bool relative) {
	if (relative) {
		page += new_page;
	} else {
		page = new_page;
	}
	if (page < 0) {
		page = 0;
		off_y = 0;
	}
	if (page > res->get_page_count() - 1) {
		page = res->get_page_count() - 1;
	}
}

void Layout::resize(float w, float h) {
	width = w;
	height = h;
}

//==[ PresentationLayout ]===========================================================
PresentationLayout::PresentationLayout(ResourceManager *_res, int page) :
		Layout(_res, page) {
}

PresentationLayout::PresentationLayout(Layout& old_layout) :
		Layout(old_layout) {
}

void PresentationLayout::scroll_smooth(int /*dx*/, int /*dy*/) {
	// ignore smooth scrolling
}

void PresentationLayout::render(QPainter *painter) {
	float render_width;
	int center_x = 0, center_y = 0;

	if (width / height > res->get_page_aspect(page)) {
		render_width = res->get_page_aspect(page) * height;
		center_x = (width - render_width) / 2;
	} else {
		render_width = width;
		center_y = (height - render_width / res->get_page_aspect(page)) / 2;
	}
	QImage *img = res->get_page(page, render_width);
	if (img != NULL) {
		painter->drawImage(center_x, center_y, *img);
		res->unlock_page(page);
	}

	// prefetch - order should be important
	if (res->get_page(page + 1, render_width) != NULL) { // one after
		res->unlock_page(page + 1);
	}
	if (res->get_page(page - 1, render_width) != NULL) { // one before
		res->unlock_page(page - 1);
	}
	if (res->get_page(page + 2, render_width) != NULL) { // two after
		res->unlock_page(page + 2);
	}
	if (res->get_page(page - 2, render_width) != NULL) { // two before
		res->unlock_page(page - 2);
	}
	res->collect_garbage(page - 4, page + 4);
}


//==[ SequentialLayout ]=======================================================
SequentialLayout::SequentialLayout(ResourceManager *_res, int page) :
		Layout(_res, page) {
	off_x = 0;
}

SequentialLayout::SequentialLayout(Layout& old_layout) :
		Layout(old_layout) {
}

void SequentialLayout::scroll_smooth(int dx, int dy) {
	Layout::scroll_smooth(dx, dy);
	while (off_y <= -width / res->get_page_aspect(page) && page < res->get_page_count() - 1) {
		off_y += width / res->get_page_aspect(page);
		page++;
	}
	while (off_y > 0 && page > 0) {
		page--;
		off_y -= width / res->get_page_aspect(page);
	}
	if (page == 0 && off_y > 0) {
		off_y = 0;
	}
	off_x = 0;
}

void SequentialLayout::render(QPainter *painter) {
	float page_width = width;
	float page_height; // = page_width / res->get_page_aspect(page);
	int cur_page = page, cur_offset = off_y;

	while (cur_offset < height && cur_page < res->get_page_count()) {
		page_height = page_width / res->get_page_aspect(cur_page);
		QImage *img = res->get_page(cur_page, page_width);
		if (img != NULL) {
			painter->drawImage(off_x, cur_offset, *img);
			res->unlock_page(cur_page);
		}
		cur_offset += page_height;
		cur_page++;
	}

	// prefetch - order should be important
	if (res->get_page(cur_page, page_width) != NULL) { // one after
		res->unlock_page(cur_page);
	}
	if (res->get_page(page - 1, page_width) != NULL) { // one before
		res->unlock_page(page - 1);
	}
	if (res->get_page(cur_page + 1, page_width) != NULL) { // two after
		res->unlock_page(cur_page + 1);
	}
	if (res->get_page(page - 2, page_width) != NULL) { // two before
		res->unlock_page(page - 2);
	}

	res->collect_garbage(page - 4, cur_page + 4);
}

//==[ GridLayout ]=============================================================
GridLayout::GridLayout(ResourceManager *_res, int page, int columns) :
		Layout(_res, page) {
	grid = new Grid(_res, columns);

	horizontal_page = page % grid->get_column_count();
}

GridLayout::GridLayout(Layout& old_layout, int columns) :
		Layout(old_layout) {
	grid = new Grid(res, columns);

	horizontal_page = page % grid->get_column_count();
}

GridLayout::~GridLayout() {
	delete grid;
}

void GridLayout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;

	// page is always in the first column
	page = page / grid->get_column_count() * grid->get_column_count();

	// vertical scrolling
	while (off_y > 0 && grid->get_height(page - grid->get_column_count()) > 0) {
		page -= grid->get_column_count();
		off_y -= grid->get_height(page) + USELESS_GAP;
	}
	float h;
	while ((h = grid->get_height(page)) > 0 &&
			off_y <= -h - USELESS_GAP) {
		off_y += grid->get_height(page) + USELESS_GAP;
		page += grid->get_column_count();
	}

	// horizontal scrolling
	while (off_x > 0 && grid->get_width(horizontal_page - 1) > 0) {
		horizontal_page--;
		off_x -= grid->get_width(horizontal_page) + USELESS_GAP;
	}
	float w;
	while ((w = grid->get_width(horizontal_page)) > 0 &&
			horizontal_page < grid->get_column_count() - 1 && // TODO not nice
			off_x <= -w - USELESS_GAP) {
		off_x += grid->get_width(horizontal_page) + USELESS_GAP;
		horizontal_page++;
	}

	// TODO horizontal borders
}

void GridLayout::scroll_page(int new_page, bool relative) {
	page = page / grid->get_column_count() * grid->get_column_count();
	Layout::scroll_page(new_page * grid->get_column_count(), relative);
	page = page / grid->get_column_count() * grid->get_column_count();
}

void GridLayout::render(QPainter *painter) {
	// vertical
	int cur_page = page;
	float h;
	int hpos = off_y;
	while ((h = grid->get_height(cur_page)) > 0 && hpos < height) {
		// horizontal
		int cur_col = horizontal_page;
		float w;
		int wpos = off_x;
		while ((w = grid->get_width(cur_col)) > 0 &&
				cur_col < grid->get_column_count() && // TODO not nice
				wpos < width) {
			int page_width = res->get_page_width(cur_page + cur_col);
			QImage *img = res->get_page(cur_page + cur_col, page_width);
			if (img != NULL) {
				painter->drawImage(wpos, hpos, *img);
				res->unlock_page(cur_page + cur_col);
			}
			wpos += grid->get_width(cur_page + cur_col) + USELESS_GAP;
			cur_col++;
		}
		hpos += grid->get_height(cur_page) + USELESS_GAP;
		cur_page += grid->get_column_count();
	}
	res->collect_garbage(page - 6, cur_page + 6);
}

