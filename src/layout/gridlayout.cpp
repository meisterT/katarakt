#include <iostream>
#include <QImage>
#include <QApplication>
#include "gridlayout.h"
#include "../util.h"
#include "layout.h"
#include "../viewer.h"
#include "../resourcemanager.h"
#include "../grid.h"
#include "../search.h"
#include "../config.h"
#include "../kpage.h"

using namespace std;


//==[ GridLayout ]=============================================================
GridLayout::GridLayout(Viewer *v, int page, int columns) :
		Layout(v, page),
		off_x(0), off_y(0),
		horizontal_page(0),
		last_visible_page(res->get_page_count() - 1),
		zoom(0) {
	initialize(columns, 0);
}

GridLayout::GridLayout(Layout& old_layout, int columns) :
		Layout(old_layout),
		horizontal_page(0),
		last_visible_page(res->get_page_count() - 1),
		zoom(0) {
	initialize(columns, 0);
}

GridLayout::~GridLayout() {
	delete grid;
}

int GridLayout::get_page() const {
	int tmp = page + horizontal_page - grid->get_offset();
	if (tmp < 0) {
		tmp = 0;
	} else if (tmp >= res->get_page_count()) {
		tmp = res->get_page_count() - 1;
	}
	return tmp;
}

void GridLayout::initialize(int columns, int offset, bool clamp) {
	grid = new Grid(res, columns, offset);

//	size = 0.6;
//	size = 250 / grid->get_width(0);
//	size = width / grid->get_width(0);

	set_constants(clamp);
}

void GridLayout::set_constants(bool clamp) {
	if (res->get_page_count() == 0) {
		return;
	}

	// calculate fit
	float used = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		used += grid->get_width(i);
	}
	int available = width - useless_gap * (grid->get_column_count() - 1);
	if (available < min_page_width * grid->get_column_count()) {
		available = min_page_width * grid->get_column_count();
	}
	size = (float) available / used;

	// apply zoom value
	size *= (1 + zoom * zoom_factor);

	horizontal_page = (page + horizontal_page) % grid->get_column_count();
	page = page / grid->get_column_count() * grid->get_column_count();

	total_height = 0;
	for (int i = 0; i < grid->get_row_count(); i++) {
		total_height += ROUND(grid->get_height(i) * size);
	}
	total_height += useless_gap * (grid->get_row_count() - 1);

	total_width = 0;
	for (int i = 0; i < grid->get_column_count(); i++) {
		total_width += grid->get_width(i) * size;
	}
	total_width += useless_gap * (grid->get_column_count() - 1);

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
		w += useless_gap;
	}
	// bottom border
	border_page_h = grid->get_row_count() * grid->get_column_count();
	int h = 0;
	for (int i = grid->get_row_count() - 1; i >= 0; i--) {
		h += ROUND(grid->get_height(i) * size);
		if (h >= height) {
			border_page_h = i * grid->get_column_count();
			border_off_h = height - h;
			break;
		}
		h += useless_gap;
	}

	// update view
	// TODO not updating when not clamping is not the best solution
	if (clamp) {
		scroll_smooth(0, 0);
	}
}

void GridLayout::activate(const Layout *old_layout) {
	Layout::activate(old_layout);
	page += grid->get_offset() - horizontal_page;
}

void GridLayout::rebuild(bool clamp) {
	Layout::rebuild(clamp);
	// rebuild non-dynamic data
	int columns = grid->get_column_count();
	int offset = grid->get_offset();
	delete grid;
	initialize(columns, offset, clamp);
}

void GridLayout::resize(int w, int h) {
	float old_size = size;
	Layout::resize(w, h);
	set_constants();

	off_x = off_x * size / old_size;
	off_y = off_y * size / old_size;
//	off_y = (off_y - height / 2) * size / old_size + height / 2;
	scroll_smooth(0, 0);
}

bool GridLayout::set_zoom(int new_zoom, bool relative) {
	float old_factor = 1 + zoom * zoom_factor;
	if (relative) {
		zoom += new_zoom;
	} else {
		zoom = new_zoom;
	}
	if (zoom < min_zoom) {
		zoom = min_zoom;
	} else if (zoom > max_zoom) {
		zoom = max_zoom;
	}
	float new_factor = 1 + zoom * zoom_factor;

	if (old_factor == new_factor) {
		return false;
	}

	off_x = (off_x - width / 2) * new_factor / old_factor + width / 2;
	off_y = (off_y - height / 2) * new_factor / old_factor + height / 2;

	set_constants();
	return true;
}

bool GridLayout::set_columns(int new_columns, bool relative) {
	if (relative) {
		new_columns += grid->get_column_count();
	}

	if (grid->set_columns(new_columns)) {
		set_constants();
		return true;
	}
	return false;
}

bool GridLayout::set_offset(int new_offset, bool relative) {
	if (relative) {
		new_offset += grid->get_offset();
	}

	if (grid->set_offset(new_offset)) {
		set_constants();
		return true;
	}
	return false;
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
				(h = ROUND(grid->get_height(page / grid->get_column_count() - 1) * size)) > 0) {
			off_y -= h + useless_gap;
			page -= grid->get_column_count();
		}
		// page down
		while ((h = ROUND(grid->get_height(page / grid->get_column_count()) * size)) > 0 &&
				page < border_page_h &&
				off_y <= -h - useless_gap) {
			off_y += h + useless_gap;
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
			off_x -= w + useless_gap;
			horizontal_page--;
		}
		// page right
		while ((w = grid->get_width(horizontal_page) * size) > 0 &&
				horizontal_page < border_page_w &&
				horizontal_page < grid->get_column_count() - 1 && // only for horizontal
				off_x <= -w - useless_gap) {
			off_x += w + useless_gap;
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
	int old_off_y = off_y;

	// negative has to stay negative, absolute movement has to account for grid offset
	if (!relative && new_page >= 0) {
		new_page += grid->get_offset();
	}

	if (total_height > height) {
		if (!relative) {
			if (new_page >= 0) { // else int rounding is bad, < 0 has to stay < 0
				new_page /= grid->get_column_count();
			}
			page = new_page * grid->get_column_count();
		} else {
			page += new_page * grid->get_column_count();
		}
		// clamp height
		if ((page == 0 && off_y > 0) || page < 0) { // top border
			page = 0;
			off_y = 0;
		} else if ((page == border_page_h && off_y < border_off_h) || page > border_page_h) { // bottom border
			page = border_page_h;
			off_y = border_off_h;
		}
	}
	return page != old_page || off_y != old_off_y;
}

void GridLayout::render(QPainter *painter) {
	// vertical
	int cur_page = page;
	int last_page = page + horizontal_page;
	int grid_height; // implicit rounding
	int hpos = off_y;
	while ((grid_height = ROUND(grid->get_height(cur_page / grid->get_column_count()) * size)) > 0 && hpos < height) {
		// horizontal
		int cur_col = horizontal_page;
		int grid_width; // implicit rounding
		int wpos = off_x;
		while ((grid_width = grid->get_width(cur_col) * size) > 0 &&
				cur_col < grid->get_column_count() &&
				wpos < width) {
			last_page = cur_page + cur_col - grid->get_offset();

			int page_width = res->get_page_width(last_page) * size;
			int page_height = ROUND(res->get_page_height(last_page) * size);

			int center_x = (grid_width - page_width) / 2;
			int center_y = (grid_height - page_height) / 2;

			const KPage *k_page = res->get_page(last_page, page_width);
			if (k_page != NULL) {
				const QImage *img = k_page->get_image();
				if (img != NULL) {
					int rot = (res->get_rotation() - k_page->get_rotation() + 4) % 4;
					QRect rect;
					painter->rotate(rot * 90);
					// calculate page position
					if (rot == 0) {
						rect = QRect(wpos + center_x, hpos + center_y,
								page_width, page_height);
					} else if (rot == 1) {
						rect = QRect(hpos + center_y, -wpos - center_x - page_width,
								page_height, page_width);
					} else if (rot == 2) {
						rect = QRect(-wpos - center_x - page_width,
								-hpos - center_y - page_height,
								page_width, page_height);
					} else if (rot == 3) {
						rect = QRect(-hpos - center_y - page_height, wpos + center_x,
								page_height, page_width);
					}
					// draw scaled
					if (page_width != k_page->get_width() || rot != 0) {
						painter->drawImage(rect, *img);
					} else { // draw as-is
						painter->drawImage(rect.topLeft(), *img);
					}
					painter->rotate(-rot * 90);
				}
				res->unlock_page(last_page);
			}

			// draw search rects
			QPoint offset(wpos + center_x, hpos + center_y);
			if (search_visible) {
				render_search_rects(painter, last_page, offset, size);
			}

			// draw text selection
			render_selection(painter, last_page, offset, size);

			wpos += grid_width + useless_gap;
			cur_col++;
		}
		hpos += grid_height + useless_gap;
		cur_page += grid->get_column_count();
	}

	last_visible_page = last_page;
	res->collect_garbage(page + horizontal_page - grid->get_offset() - prefetch_count * 3, last_page + prefetch_count * 3);

	// prefetch
	int prefetch_first = page + horizontal_page - grid->get_offset() - 1;
	int prefetch_last = last_visible_page + 1;
	for (int count = 0; count < prefetch_count; count++) {
		// after last visible page
		int page_width = res->get_page_width(prefetch_last + count) * size;
		if (res->get_page(prefetch_last + count, page_width) != NULL) {
			res->unlock_page(prefetch_last + count);
		}
		// before first visible page
		page_width = res->get_page_width(prefetch_first + count) * size;
		if (res->get_page(prefetch_first + count, page_width) != NULL) {
			res->unlock_page(prefetch_first + count);
		}
	}
}

bool GridLayout::advance_hit(bool forward) {
	if (Layout::advance_hit(forward)) {
		view_hit();
		return true;
	}
	return false;
}

bool GridLayout::advance_invisible_hit(bool forward) {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();

	if (hits->empty()) {
		return false;
	}

	QRect r;
	QList<QRectF>::const_iterator it = hit_it;
	do {
		Layout::advance_hit(forward);
		r = get_target_rect(hit_page, *hit_it);
		if (r.x() < 0 || r.y() < 0 ||
				r.x() + r.width() >= width ||
				r.y() + r.height() >= height) {
			break; // TODO always breaks for boxes larger than the viewport
		}
	} while (it != hit_it);
	view_rect(r);
	return true;
}

bool GridLayout::view_hit() {
	QRect r = get_target_rect(hit_page, *hit_it);
	return view_rect(r);
}

bool GridLayout::view_rect(const QRect &r) {
	bool change = false;

	// move view horizontally
	if (r.width() <= width * (1 - 2 * search_padding)) {
		if (r.x() < width * search_padding) {
			change |= scroll_smooth(width * search_padding - r.x(), 0);
		} else if (r.x() + r.width() > width * (1 - search_padding)) {
			change |= scroll_smooth(width * (1 - search_padding) - r.x() - r.width(), 0);
		}
	} else {
		int center = (width - r.width()) / 2;
		if (center < 0) {
			center = 0;
		}
		change |= scroll_smooth(center - r.x(), 0);
	}
	// vertically
	if (r.height() <= height * (1 - 2 * search_padding)) {
		if (r.y() < height * search_padding) {
			change |= scroll_smooth(0, height * search_padding - r.y());
		} else if (r.y() + r.height() > height * (1 - search_padding)) {
			change |= scroll_smooth(0, height * (1 - search_padding) - r.y() - r.height());
		}
	} else {
		int center = (height - r.height()) / 2;
		if (center < 0) {
			center = 0;
		}
		change |= scroll_smooth(0, center - r.y());
	}
	return change;
}

bool GridLayout::view_point(const QPoint &p) {
	return scroll_smooth(-p.x(), -p.y());
}

QRect GridLayout::get_target_rect(int target_page, QRectF target_rect) const {
	// get rect coordinates relative to the current view
	QRectF rot = rotate_rect(target_rect, res->get_page_width(target_page),
			res->get_page_height(target_page), res->get_rotation());
	QPoint p = get_target_page_distance(target_page);
	return transform_rect(rot, size, p.x(), p.y());
}

QPoint GridLayout::get_target_page_distance(int target_page) const {
	int target_page_offset = target_page + grid->get_offset();
	// calculate distances
	int page_width = res->get_page_width(target_page) * size;
	int page_height = ROUND(res->get_page_height(target_page) * size);

	int center_x = (grid->get_width(target_page_offset % grid->get_column_count()) * size - page_width) / 2;
	int center_y = (ROUND(grid->get_height(target_page_offset / grid->get_column_count()) * size) - page_height) / 2;

	int wpos = off_x;
	if (target_page_offset % grid->get_column_count() > horizontal_page) {
		for (int i = horizontal_page; i < target_page_offset % grid->get_column_count(); i++) {
			wpos += grid->get_width(i) * size + useless_gap;
		}
	} else {
		for (int i = horizontal_page; i > target_page_offset % grid->get_column_count(); i--) {
			wpos -= grid->get_width(i) * size + useless_gap;
		}
	}
	int hpos = off_y;
	if (target_page_offset > page) {
		for (int i = (page + grid->get_offset()) / grid->get_column_count();
				i < target_page_offset / grid->get_column_count(); i++) {
			hpos += ROUND(grid->get_height(i) * size) + useless_gap;
		}
	} else {
		for (int i = (page + grid->get_offset()) / grid->get_column_count();
				i > target_page_offset / grid->get_column_count(); i--) {
			hpos -= ROUND(grid->get_height(i) * size) + useless_gap;
		}
	}
	return QPoint(wpos + center_x, hpos + center_y);
}

pair<int, QPointF> GridLayout::get_location_at(int mx, int my) {
	// find vertical page
	int cur_page = page;
	int grid_height;
	int hpos = off_y;
	while ((grid_height = ROUND(grid->get_height(cur_page / grid->get_column_count()) * size)) > 0 &&
			hpos < height) {
		if (my < hpos + grid_height) {
			break;
		}
		hpos += grid_height + useless_gap;
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
		wpos += grid_width + useless_gap;
		cur_col++;
	}

	int page_width = res->get_page_width(cur_page + cur_col) * size;
	int page_height = ROUND(res->get_page_height(cur_page + cur_col) * size);

	int center_x = (grid_width - page_width) / 2;
	int center_y = (grid_height - page_height) / 2;

	// transform mouse coordinates
	float x = (mx - center_x - wpos) / (float) page_width;
	float y = (my - center_y - hpos) / (float) page_height;

	// apply rotation
	int rotation = res->get_rotation();
	if (rotation == 1) {
		float tmp = x;
		x = y;
		y = 1 - tmp;
	} else if (rotation == 2) {
		x = 1 - x;
		y = 1 - y;
	} else if (rotation == 3) {
		float tmp = y;
		y = x;
		x = 1 - tmp;
	}

	int page = cur_page + cur_col - grid->get_offset();

	return make_pair(page, QPointF(x, y));
}

bool GridLayout::goto_link_destination(const Poppler::LinkDestination &link) {
	// TODO variable margin?
	int link_page = link.pageNumber() - 1;
	float w = res->get_page_width(link_page);
	float h = res->get_page_height(link_page);

	const QPointF link_point = rotate_point(QPointF(link.left(), link.top()) * h, w, h, res->get_rotation());

	QPoint p = get_target_page_distance(link_page);
	if (link.isChangeLeft()) {
		p.rx() += link_point.x() * size - width * search_padding;
	}
	if (link.isChangeTop()) {
		p.ry() += link_point.y() * size - height * search_padding;
	}
	return view_point(p);
}

bool GridLayout::goto_page_at(int mx, int my) {
	pair<int,QPointF> page = get_location_at(mx, my);

	set_columns(1, false);
	scroll_page(page.first, false);

	int new_page_height = ROUND(res->get_page_height(page.first) * size);
	int new_y = page.second.y() * new_page_height;

	scroll_smooth(0, -new_y + height/2 - off_y);
	return true;
}

bool GridLayout::page_visible(int p) const {
	if (p < page + horizontal_page - grid->get_offset() || p > last_visible_page) {
		return false;
	}
	return true;
}

