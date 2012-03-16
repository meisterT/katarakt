#include "layout.h"

using namespace std;


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

