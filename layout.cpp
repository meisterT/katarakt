#include "layout.h"
#include "resourcemanager.h"
#include "grid.h"
#include "search.h"

using namespace std;


// TODO put in a config source file
#define USELESS_GAP 2
#define MIN_PAGE_WIDTH 150
#define MIN_ZOOM -14
#define MAX_ZOOM 30
#define ZOOM_FACTOR 0.05f
#define PREFETCH_COUNT 3
#define SEARCH_PADDING 0.2 // must be <= 0.5

// rounds a float when afterwards cast to int
// seems to fix the mismatch between calculated page height and actual image height
#define ROUND(x) ((x) + 0.5f)


//==[ Layout ]=================================================================
Layout::Layout(ResourceManager *_res, int _page) :
		res(_res), page(_page), off_x(0), off_y(0), width(0), height(0),
		search_visible(false) {
}

int Layout::get_page() const {
	return page;
}

void Layout::rebuild() {
	// clamp to available pages
	if (page < 0) {
		page = 0;
	}
	if (page >= res->get_page_count()) {
		page = res->get_page_count() - 1;
	}
}

void Layout::resize(int w, int h) {
	width = w;
	height = h;
}

void Layout::set_zoom(int /*new_zoom*/, bool /*relative*/) {
	// implement in child classes where necessary
}

void Layout::set_columns(int /*new_columns*/, bool /*relative*/) {
	// only useful for grid layout
}

bool Layout::supports_smooth_scrolling() const {
	// normally a layout supports smooth scrolling
	return true;
}

bool Layout::scroll_smooth(int dx, int dy) {
	off_x += dx;
	off_y += dy;
	return true;
}

bool Layout::scroll_page(int new_page, bool relative) {
	int old_page = page;
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
	return page != old_page;
}

void Layout::clear_hits() {
	for (map<int,list<Result> *>::iterator it = hits.begin(); it != hits.end(); ++it) {
		delete it->second;
	}
	hits.clear();
}

void Layout::set_hits(int page, list<Result> *l) {
	// new search -> initialize highlight
	if (hits.size() == 0) {
		hit_page = page;
		hit_it = l->begin();
		view_hit();
	}

	// just to be safe - prevent memory leaks
	map<int,list<Result> *>::iterator it = hits.find(page);
	if (it != hits.end()) {
		delete it->second;
	}
	hits[page] = l;
}

void Layout::set_search_visible(bool visible) {
	search_visible = visible;
}

void Layout::advance_hit(bool forward) {
	if (hits.size() == 0) {
		return;
	}
	// find next hit
	if (forward) {
		++hit_it;
		if (hit_it == hits[hit_page]->end()) { // this was the last hit on that page
			map<int,list<Result> *>::const_iterator it = hits.upper_bound(hit_page);
			if (it == hits.end()) { // this was the last page with a hit -> wrap
				it = hits.begin();
			}
			hit_page = it->first;
			hit_it = it->second->begin();
		}
	// find previous hit
	} else {
		if (hit_it == hits[hit_page]->begin()) { // this was the first hit on that page
			map<int,list<Result> *>::const_reverse_iterator it(hits.lower_bound(hit_page));
			if (it == hits.rend()) { // this was the first page with a hit -> wrap
				it = hits.rbegin();
			}
			hit_page = it->first;
			hit_it = --(it->second->end());
		} else {
			--hit_it;
		}
	}
}

bool Layout::click_mouse(int /*mx*/, int /*my*/) {
	return false;
}

bool Layout::get_search_visible() const {
	return search_visible;
}


//==[ PresentationLayout ]===========================================================
PresentationLayout::PresentationLayout(ResourceManager *_res, int page) :
		Layout(_res, page) {
}

PresentationLayout::PresentationLayout(Layout& old_layout) :
		Layout(old_layout) {
}

bool PresentationLayout::supports_smooth_scrolling() const {
	return false;
}

bool PresentationLayout::scroll_smooth(int /*dx*/, int /*dy*/) {
	// ignore smooth scrolling
	return false;
}

int PresentationLayout::calculate_fit_width(int page) {
	if ((float) width / height > res->get_page_aspect(page)) {
		return res->get_page_aspect(page) * height;
	}
	return width;
}

void PresentationLayout::render(QPainter *painter) {
	int page_width = width, page_height = height;
	int center_x = 0, center_y = 0;

	// calculate perfect fit
	if ((float) width / height > res->get_page_aspect(page)) {
		page_width = res->get_page_aspect(page) * page_height;
		center_x = (width - page_width) / 2;
	} else {
		page_height = ROUND(page_width / res->get_page_aspect(page));
		center_y = (height - page_height) / 2;
	}
	QImage *img = res->get_page(page, page_width);
	if (img != NULL) {
		if (page_width != img->width()) { // draw scaled
			QRect rect(center_x, center_y, page_width, page_height);
			painter->drawImage(rect, *img);
		} else { // draw as-is
			painter->drawImage(center_x, center_y, *img);
		}
		res->unlock_page(page);
	}

	// draw search rects
	if (search_visible) {
		painter->setPen(QColor(0, 0, 0));
		painter->setBrush(QColor(255, 0, 0, 64));
		double factor = page_width / res->get_page_width(page);
		map<int,list<Result> *>::iterator it = hits.find(page);
		if (it != hits.end()) {
			for (list<Result>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
				if (i2 == hit_it) {
					painter->setBrush(QColor(0, 255, 0, 64));
					painter->drawRect(i2->scale_translate(factor, center_x, center_y));
					painter->setBrush(QColor(255, 0, 0, 64));
				} else {
					painter->drawRect(i2->scale_translate(factor, center_x, center_y));
				}
			}
		}
	}

	// draw goto link rects
/*	const list<Poppler::LinkGoto *> *l = res->get_links(page);
	if (l != NULL) {
		painter->setPen(QColor(0, 0, 255));
		painter->setBrush(QColor(0, 0, 255, 64));
		for (list<Poppler::LinkGoto *>::const_iterator it = l->begin();
				it != l->end(); ++it) {
			QRectF r = (*it)->linkArea();
			r.setLeft(r.left() * page_width);
			r.setTop(r.top() * page_height);
			r.setRight(r.right() * page_width);
			r.setBottom(r.bottom() * page_height);
			r.translate(center_x, center_y);
			painter->drawRect(r);
		}
	} */

	// prefetch
	for (int count = 1; count <= PREFETCH_COUNT; count++) {
		// after current page
		if (res->get_page(page + count, calculate_fit_width(page + count)) != NULL) {
			res->unlock_page(page + count);
		}
		// before current page
		if (res->get_page(page - count, calculate_fit_width(page - count)) != NULL) {
			res->unlock_page(page - count);
		}
	}
	res->collect_garbage(page - PREFETCH_COUNT * 3, page + PREFETCH_COUNT * 3);
}

void PresentationLayout::advance_hit(bool forward) {
	Layout::advance_hit(forward);
	view_hit();
}

void PresentationLayout::view_hit() {
	scroll_page(hit_page, false);
}

bool PresentationLayout::click_mouse(int mx, int my) {
	// TODO duplicate code
	int page_width = width, page_height = height;
	int center_x = 0, center_y = 0;

	// calculate perfect fit
	if ((float) width / height > res->get_page_aspect(page)) {
		page_width = res->get_page_aspect(page) * page_height;
		center_x = (width - page_width) / 2;
	} else {
		page_height = ROUND(page_width / res->get_page_aspect(page));
		center_y = (height - page_height) / 2;
	}

	// transform mouse coordinates
	float x = (mx - center_x) / (float) page_width;
	float y = (my - center_y) / (float) page_height;

	// find matching box
	const list<Poppler::LinkGoto *> *l = res->get_links(page);
	for (list<Poppler::LinkGoto *>::const_iterator it = l->begin();
			it != l->end(); ++it) {
		QRectF r = (*it)->linkArea();
		if (x >= r.left() && x < r.right()) {
			if (y < r.top() && y >= r.bottom()) {
				int new_page = (*it)->destination().pageNumber();
				scroll_page(new_page - 1, false);
				return true;
			}
		}
	}
	return false;
}

bool PresentationLayout::page_visible(int p) const {
	return p == page;
}


//==[ GridLayout ]=============================================================
GridLayout::GridLayout(ResourceManager *_res, int page, int columns) :
		Layout(_res, page),
		horizontal_page(0),
		zoom(0) {
	initialize(columns);
}

GridLayout::GridLayout(Layout& old_layout, int columns) :
		Layout(old_layout),
		horizontal_page(0),
		zoom(0) {
	initialize(columns);
}

GridLayout::~GridLayout() {
	delete grid;
}

void GridLayout::initialize(int columns) {
	grid = new Grid(res, columns);

//	size = 0.6;
//	size = 250 / grid->get_width(0);
//	size = width / grid->get_width(0);

	set_constants();
}

void GridLayout::set_constants() {
	// calculate fit
	int used = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		used += grid->get_width(i);
	}
	int available = width - USELESS_GAP * (grid->get_column_count() - 1);
	if (available < MIN_PAGE_WIDTH * grid->get_column_count()) {
		available = MIN_PAGE_WIDTH * grid->get_column_count();
	}
	size = (float) available / used;

	// apply zoom value
	size *= (1 + zoom * ZOOM_FACTOR);

	horizontal_page = (page + horizontal_page) % grid->get_column_count();
	page = page / grid->get_column_count() * grid->get_column_count();

	total_height = 0;
	for (int i = 0; i < grid->get_row_count(); i++) {
		total_height += ROUND(grid->get_height(i * grid->get_column_count()) * size);
	}
	total_height += USELESS_GAP * (grid->get_row_count() - 1);

	total_width = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		total_width += grid->get_width(i) * size;
	}
	total_width += USELESS_GAP * (grid->get_column_count() - 1);

	// calculate offset for blocking at the right border
	border_page_w = grid->get_column_count();
	int w = 0;
	for (int i = grid->get_column_count() - 1; i >= 0; i--) {
		w += grid->get_width(i) * size;
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
		h += ROUND(grid->get_height(i * grid->get_column_count()) * size);
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
	Layout::rebuild();
	// rebuild non-dynamic data
	int columns = grid->get_column_count();
	delete grid;
	initialize(columns);
}

void GridLayout::resize(int w, int h) {
	Layout::resize(w, h);
	set_constants();
}

void GridLayout::set_zoom(int new_zoom, bool relative) {
	float old_factor = 1 + zoom * ZOOM_FACTOR;
	if (relative) {
		zoom += new_zoom;
	} else {
		zoom = new_zoom;
	}
	if (zoom < MIN_ZOOM) {
		zoom = MIN_ZOOM;
	} else if (zoom > MAX_ZOOM) {
		zoom = MAX_ZOOM;
	}
	float new_factor = 1 + zoom * ZOOM_FACTOR;

	off_x = (off_x - width / 2) * new_factor / old_factor + width / 2;
	off_y = (off_y - height / 2) * new_factor / old_factor + height / 2;

	set_constants();
}

void GridLayout::set_columns(int new_columns, bool relative) {
	if (relative) {
		grid->set_columns(grid->get_column_count() + new_columns);
	} else {
		grid->set_columns(new_columns);
	}

	set_constants();
}

bool GridLayout::scroll_smooth(int dx, int dy) {
	int old_off_x = off_x;
	int old_off_y = off_y;
	int old_page = page;
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
				(h = ROUND(grid->get_height(page - grid->get_column_count()) * size)) > 0) {
			off_y -= h + USELESS_GAP;
			page -= grid->get_column_count();
		}
		// page down
		while ((h = ROUND(grid->get_height(page) * size)) > 0 &&
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
				(w = grid->get_width(horizontal_page - 1) * size) > 0) {
			off_x -= w + USELESS_GAP;
			horizontal_page--;
		}
		// page right
		while ((w = grid->get_width(horizontal_page) * size) > 0 &&
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
	return off_x != old_off_x || off_y != old_off_y || page != old_page;
}

bool GridLayout::scroll_page(int new_page, bool relative) {
	int old_page = page;
	if (total_height > height) {
		if (!relative) {
			new_page /= grid->get_column_count();
		}
		Layout::scroll_page(new_page * grid->get_column_count(), relative);
	}
	if ((page == border_page_h && off_y < border_off_h) || page > border_page_h) {
		page = border_page_h;
		off_y = border_off_h;
	}
	return page != old_page;
}

void GridLayout::render(QPainter *painter) {
	// vertical
	int cur_page = page;
	int last_page = page + horizontal_page;
	int grid_height; // implicit rounding
	int hpos = off_y;
	while ((grid_height = ROUND(grid->get_height(cur_page) * size)) > 0 && hpos < height) {
		// horizontal
		int cur_col = horizontal_page;
		int grid_width; // implicit rounding
		int wpos = off_x;
		while ((grid_width = grid->get_width(cur_col) * size) > 0 &&
				cur_col < grid->get_column_count() &&
				wpos < width) {
			last_page = cur_page + cur_col;

			int page_width = res->get_page_width(last_page) * size;
			int page_height = ROUND(res->get_page_height(last_page) * size);

			int center_x = (grid_width - page_width) / 2;
			int center_y = (grid_height - page_height) / 2;

			QImage *img = res->get_page(last_page, page_width);
			if (img != NULL) {
/*				// debugging
				int a = img->height(), b = ROUND(grid->get_height(last_page) * size);
				if (a != b) {
					// TODO fix this?
					cerr << "image is " << (a - b) << " pixels bigger than expected" << endl;
				} */
				if (page_width != img->width()) { // draw scaled
					QRect rect(wpos + center_x, hpos + center_y, page_width, page_height);
					painter->drawImage(rect, *img);
				} else { // draw as-is
					painter->drawImage(wpos + center_x, hpos + center_y, *img);
				}
				res->unlock_page(last_page);
			}

			// draw search rects
			if (search_visible) {
				painter->setPen(QColor(0, 0, 0));
				painter->setBrush(QColor(255, 0, 0, 64));
				map<int,list<Result> *>::iterator it = hits.find(last_page);
				if (it != hits.end()) {
					for (list<Result>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
						if (i2 == hit_it) {
							painter->setBrush(QColor(0, 255, 0, 64));
							painter->drawRect(i2->scale_translate(size, wpos + center_x, hpos + center_y));
							painter->setBrush(QColor(255, 0, 0, 64));
						} else {
							painter->drawRect(i2->scale_translate(size, wpos + center_x, hpos + center_y));
						}
					}
				}
			}

			wpos += grid_width + USELESS_GAP;
			cur_col++;
		}
		hpos += grid_height + USELESS_GAP;
		cur_page += grid->get_column_count();
	}

	last_visible_page = last_page;
	res->collect_garbage(page - PREFETCH_COUNT * 3, last_page + PREFETCH_COUNT * 3);

	// prefetch
	int cur_col = last_page % grid->get_column_count();
	cur_page = last_page - cur_col;
	int cur_col_first = horizontal_page;
	int cur_page_first = page;
	for (int count = 0; count < PREFETCH_COUNT; count++) {
		// after last visible page
		cur_col++;
		if (cur_col >= grid->get_column_count()) {
			cur_col = 0;
			cur_page += grid->get_column_count();
		}
		int page_width = res->get_page_width(cur_page + cur_col) * size;
		if (res->get_page(cur_page + cur_col, page_width) != NULL) {
			res->unlock_page(cur_page + cur_col);
		}
		// before first visible page
		cur_col_first--;
		if (cur_col_first < 0) {
			cur_col_first = grid->get_column_count() - 1;
			cur_page_first -= grid->get_column_count();
		}
		page_width = res->get_page_width(cur_page_first + cur_col_first) * size;
		if (res->get_page(cur_page_first + cur_col_first, page_width) != NULL) {
			res->unlock_page(cur_page_first + cur_col_first);
		}
	}
}

void GridLayout::advance_hit(bool forward) {
	Layout::advance_hit(forward);
	view_hit();
}

void GridLayout::view_hit() {
	// calculate offsets
	int page_width = res->get_page_width(hit_page) * size;
	int page_height = ROUND(res->get_page_height(hit_page) * size);

	int center_x = (grid->get_width(hit_page) * size - page_width) / 2;
	int center_y = (grid->get_height(hit_page) * size - page_height) / 2;

	int wpos = off_x;
	for (int i = 0; i < hit_page % grid->get_column_count(); i++) {
		wpos += grid->get_width(i) * size + USELESS_GAP;
	}
	int hpos = off_y;
	if (hit_page > page) {
		for (int i = page / grid->get_column_count();
				i < hit_page / grid->get_column_count(); i++) {
			hpos += grid->get_height(i) * size + USELESS_GAP;
		}
	} else {
		for (int i = page / grid->get_column_count();
				i > hit_page / grid->get_column_count(); i--) {
			hpos -= grid->get_height(i) * size + USELESS_GAP;
		}
	}
	// get search rect coordinates relative to the current view
	QRect r = hit_it->scale_translate(size, wpos + center_x, hpos + center_y);
	// move view horizontally
	if (r.width() <= width * (1 - 2 * SEARCH_PADDING)) {
		if (r.x() < width * SEARCH_PADDING) {
			scroll_smooth(width * SEARCH_PADDING - r.x(), 0);
		} else if (r.x() + r.width() > width * (1 - SEARCH_PADDING)) {
			scroll_smooth(width * (1 - SEARCH_PADDING) - r.x() - r.width(), 0);
		}
	} else {
		int center = (width - r.width()) / 2;
		if (center < 0) {
			center = 0;
		}
		scroll_smooth(center - r.x(), 0);
	}
	// vertically
	if (r.height() <= height * (1 - 2 * SEARCH_PADDING)) {
		if (r.y() < height * SEARCH_PADDING) {
			scroll_smooth(0, height * SEARCH_PADDING - r.y());
		} else if (r.y() + r.height() > height * (1 - SEARCH_PADDING)) {
			scroll_smooth(0, height * (1 - SEARCH_PADDING) - r.y() - r.height());
		}
	} else {
		int center = (height - r.height()) / 2;
		if (center < 0) {
			center = 0;
		}
		scroll_smooth(0, center - r.y());
	}
}

bool GridLayout::click_mouse(int mx, int my) {
	// TODO ignore gaps?
	// find vertical page
	int cur_page = page;
	int grid_height;
	int hpos = off_y;
	while ((grid_height = ROUND(grid->get_height(cur_page) * size)) > 0 &&
			hpos < height) {
		if (my < hpos + grid_height) {
			break;
		}
		hpos += grid_height + USELESS_GAP;
		cur_page += grid->get_column_count();
	}
	// find horizontal page
	int cur_col = horizontal_page;
	int grid_width;
	int wpos = off_x;
	while ((grid_width = grid->get_width(cur_col) * size) > 0 &&
			cur_col < grid->get_column_count() &&
			wpos < width) {
		if (mx < wpos + grid_width) {
			break;
		}
		wpos += grid_width + USELESS_GAP;
		cur_col++;
	}

	int page_width = res->get_page_width(cur_page + cur_col) * size;
	int page_height = ROUND(res->get_page_height(cur_page + cur_col) * size);

	int center_x = (grid_width - page_width) / 2;
	int center_y = (grid_height - page_height) / 2;

	// transform mouse coordinates
	float x = (mx - center_x - wpos) / (float) page_width;
	float y = (my - center_y - hpos) / (float) page_height;

	// find matching box
	const list<Poppler::LinkGoto *> *l = res->get_links(cur_page + cur_col);
	int count = 0;
	for (list<Poppler::LinkGoto *>::const_iterator it = l->begin();
			it != l->end(); ++it) {
		QRectF r = (*it)->linkArea();
		if (x >= r.left() && x < r.right()) {
			if (y < r.top() && y >= r.bottom()) {
				int new_page = (*it)->destination().pageNumber();
				scroll_page((new_page - 1) / grid->get_column_count(), false);
				return true;
			}
		}
		count++;
	}
	return false;
}

bool GridLayout::page_visible(int p) const {
	if (p < page || p > last_visible_page) {
		return false;
	}
	return true;
}

