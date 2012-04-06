#include "layout.h"

using namespace std;


// TODO put in a config source file
#define USELESS_GAP 2

// rounds a float when afterwards cast to int
// seems to fix the mismatch between calculated page height and actual image height
#define ROUND(x) ((x) + 0.5f)


//==[ Layout ]=================================================================
Layout::Layout(ResourceManager *_res, int _page) :
		res(_res), page(_page), off_x(0), off_y(0), width(0), height(0) {
}

int Layout::get_page() {
	return page;
}

void Layout::rebuild() {
	// does nothing here
}

void Layout::resize(int w, int h) {
	width = w;
	height = h;
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
	int page_width;
	int center_x = 0, center_y = 0;

	// calculate perfect fit
	if ((float) width / height > res->get_page_aspect(page)) {
		page_width = res->get_page_aspect(page) * height;
		center_x = (width - page_width) / 2;
	} else {
		page_width = width;
		center_y = (height - page_width / res->get_page_aspect(page)) / 2;
	}
	QImage *img = res->get_page(page, page_width);
	if (img != NULL) {
		painter->drawImage(center_x, center_y, *img);
		res->unlock_page(page);
	}

	// prefetch - order should be important
	if (res->get_page(page + 1, page_width) != NULL) { // one after
		res->unlock_page(page + 1);
	}
	if (res->get_page(page - 1, page_width) != NULL) { // one before
		res->unlock_page(page - 1);
	}
	if (res->get_page(page + 2, page_width) != NULL) { // two after
		res->unlock_page(page + 2);
	}
	if (res->get_page(page - 2, page_width) != NULL) { // two before
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
	off_x = 0;
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
	int page_width = width;
	int cur_page = page, cur_offset = off_y;

	while (cur_offset < height && cur_page < res->get_page_count()) {
		int page_height = page_width / res->get_page_aspect(cur_page);
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
	initialize(columns);
}

GridLayout::GridLayout(Layout& old_layout, int columns) :
		Layout(old_layout) {
	initialize(columns);
}

GridLayout::~GridLayout() {
	delete grid;
}

void GridLayout::initialize(int columns) {
	grid = new Grid(res, columns);

	horizontal_page = page % grid->get_column_count();
	page -= horizontal_page;
//	page = page / grid->get_column_count() * grid->get_column_count();

//	zoom = 0.6;
	zoom = 250 / grid->get_width(0);
//	zoom = width / grid->get_width(0);

	set_constants();
}

void GridLayout::set_constants() {
//	zoom = width / grid->get_width(0); // makes it feel like SequentialLayout but better :)

	total_height = 0;
	for (int i = 0; i < grid->get_row_count(); i++) {
		total_height += ROUND(grid->get_height(i * grid->get_column_count()) * zoom);
	}
	total_height += USELESS_GAP * (grid->get_row_count() - 1);

	total_width = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		total_width += grid->get_width(i) * zoom;
	}
	total_width += USELESS_GAP * (grid->get_column_count() - 1);

	// calculate offset for blocking at the right border
	border_page_w = grid->get_column_count();
	int w = 0;
	for (int i = grid->get_column_count() - 1; i >= 0; i--) {
		w += grid->get_width(i) * zoom;
		if (w >= width) {
			border_page_w = i;
			border_off_w = width - w;
			break;
		}
		w += USELESS_GAP;
	}
	// bottom border
	border_page_h = grid->get_row_count() * grid->get_column_count();
	int h = 0;
	for (int i = grid->get_row_count() - 1; i >= 0; i--) {
		h += ROUND(grid->get_height(i * grid->get_column_count()) * zoom);
		if (h >= height) {
			border_page_h = i * grid->get_column_count();
			border_off_h = height - h;
			break;
		}
		h += USELESS_GAP;
	}

	// update view
	scroll_smooth(0, 0);
}

void GridLayout::rebuild() {
	// rebuild non-dynamic data
	int columns = grid->get_column_count();
	delete grid;
	initialize(columns);
}

void GridLayout::resize(int w, int h) {
	Layout::resize(w, h);
	set_constants();
}

void GridLayout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;

	// vertical scrolling
	if (total_height <= height) { // center view
		page = 0;
		off_y = (height - total_height) / 2;
	} else {
		int h; // implicit rounding
		// page up
		while (off_y > 0 &&
				(h = ROUND(grid->get_height(page - grid->get_column_count()) * zoom)) > 0) {
			off_y -= h + USELESS_GAP;
			page -= grid->get_column_count();
		}
		// page down
		while ((h = ROUND(grid->get_height(page) * zoom)) > 0 &&
				page < border_page_h &&
				off_y <= -h - USELESS_GAP) {
			off_y += h + USELESS_GAP;
			page += grid->get_column_count();
		}
		// top and bottom borders
		if (page == 0 && off_y > 0) {
			off_y = 0;
		} else if ((page == border_page_h && off_y < border_off_h) ||
				page > border_page_h) {
			page = border_page_h;
			off_y = border_off_h;
		}
	}

	// horizontal scrolling
	if (total_width <= width) { // center view
		horizontal_page = 0;
		off_x = (width - total_width) / 2;
	} else {
		int w; // implicit rounding
		// page left
		while (off_x > 0 &&
				(w = grid->get_width(horizontal_page - 1) * zoom) > 0) {
			off_x -= w + USELESS_GAP;
			horizontal_page--;
		}
		// page right
		while ((w = grid->get_width(horizontal_page) * zoom) > 0 &&
				horizontal_page < border_page_w &&
				horizontal_page < grid->get_column_count() - 1 && // only for horizontal
				off_x <= -w - USELESS_GAP) {
			off_x += w + USELESS_GAP;
			horizontal_page++;
		}
		// left and right borders
		if (horizontal_page == 0 && off_x > 0) {
			off_x = 0;
		} else if ((horizontal_page == border_page_w && off_x < border_off_w) ||
				horizontal_page > border_page_w) {
			horizontal_page = border_page_w;
			off_x = border_off_w;
		}
	}
}

void GridLayout::scroll_page(int new_page, bool relative) {
	if (total_height > height) {
		Layout::scroll_page(new_page * grid->get_column_count(), relative);
	}
	if ((page == border_page_h && off_y < border_off_h) || page > border_page_h) {
		page = border_page_h;
		off_y = border_off_h;
	}
}

void GridLayout::render(QPainter *painter) {
	// vertical
	int cur_page = page;
	int page_height; // implicit rounding
	int hpos = off_y;
	while ((page_height = ROUND(grid->get_height(cur_page) * zoom)) > 0 && hpos < height) {
		// horizontal
		int cur_col = horizontal_page;
		int page_width; // implicit rounding
		int wpos = off_x;
		while ((page_width = grid->get_width(cur_col) * zoom) > 0 &&
				cur_col < grid->get_column_count() &&
				wpos < width) {
			QImage *img = res->get_page(cur_page + cur_col, page_width);
			if (img != NULL) {
/*				// debugging
				int a = img->height(), b = ROUND(grid->get_height(cur_page + cur_col) * zoom);
				if (a != b) {
					// TODO fix this?
					cerr << "image is " << (a - b) << " pixels bigger than expected" << endl;
				} */
				painter->drawImage(wpos, hpos, *img);
				res->unlock_page(cur_page + cur_col);
			}
			wpos += page_width + USELESS_GAP;
			cur_col++;
		}
		hpos += page_height + USELESS_GAP;
		cur_page += grid->get_column_count();
	}
	res->collect_garbage(page - 6, cur_page + 6);
}

