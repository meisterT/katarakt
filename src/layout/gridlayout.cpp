#include <iostream>
#include <QImage>
#include "gridlayout.h"
#include "../util.h"
#include "layout.h"
#include "../resourcemanager.h"
#include "../grid.h"
#include "../search.h"
#include "../config.h"

using namespace std;


//==[ GridLayout ]=============================================================
GridLayout::GridLayout(ResourceManager *_res, int page, int columns) :
		Layout(_res, page),
		off_x(0), off_y(0),
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

void GridLayout::initialize(int columns, bool clamp) {
	grid = new Grid(res, columns);

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
	int used = 0;
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
		total_height += ROUND(grid->get_height(i * grid->get_column_count()) * size);
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
		h += ROUND(grid->get_height(i * grid->get_column_count()) * size);
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

void GridLayout::rebuild(bool clamp) {
	Layout::rebuild(clamp);
	// rebuild non-dynamic data
	int columns = grid->get_column_count();
	delete grid;
	initialize(columns, clamp);
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

void GridLayout::set_zoom(int new_zoom, bool relative) {
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
			off_y -= h + useless_gap;
			page -= grid->get_column_count();
		}
		// page down
		while ((h = ROUND(grid->get_height(page) * size)) > 0 &&
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
	if (total_height > height) {
		if (!relative) {
			if (new_page >= 0) { // else int rounding is bad, < 0 has to stay < 0
				new_page /= grid->get_column_count();
			}
			page = new_page * grid->get_column_count();
		} else {
			page += new_page * grid->get_column_count();
		}
	}
	if ((page == 0 && off_y > 0) || page < 0) {
		page = 0;
		off_y = 0;
	} else if ((page == border_page_h && off_y < border_off_h) || page > border_page_h) {
		page = border_page_h;
		off_y = border_off_h;
	}
	return page != old_page || off_y != old_off_y;
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
			// TODO rotate correctly
			if (search_visible) {
				painter->setPen(QColor(0, 0, 0));
				painter->setBrush(QColor(255, 0, 0, 64));
				float w = res->get_page_width(last_page);
				float h = res->get_page_height(last_page);
				map<int,QList<QRectF> *>::iterator it = hits.find(last_page);
				if (it != hits.end()) {
					for (QList<QRectF>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
						if (i2 == hit_it) {
							painter->setBrush(QColor(0, 255, 0, 64));
						}
						QRectF rot = rotate_rect(*i2, w, h, res->get_rotation());
						painter->drawRect(transform_rect(rot, size, wpos + center_x, hpos + center_y));
						if (i2 == hit_it) {
							painter->setBrush(QColor(255, 0, 0, 64));
						}
					}
				}
			}

			wpos += grid_width + useless_gap;
			cur_col++;
		}
		hpos += grid_height + useless_gap;
		cur_page += grid->get_column_count();
	}

	last_visible_page = last_page;
	res->collect_garbage(page - prefetch_count * 3, last_page + prefetch_count * 3);

	// prefetch
	int cur_col = last_page % grid->get_column_count();
	cur_page = last_page - cur_col;
	int cur_col_first = horizontal_page;
	int cur_page_first = page;
	for (int count = 0; count < prefetch_count; count++) {
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

bool GridLayout::advance_hit(bool forward) {
	if (Layout::advance_hit(forward)) {
		view_hit();
		return true;
	}
	return false;
}

bool GridLayout::advance_invisible_hit(bool forward) {
	if (hits.size() == 0) {
		return false;
	}

	QRect r;
	QList<QRectF>::const_iterator it = hit_it;
	do {
		Layout::advance_hit(forward);
		r = get_hit_rect();
		if (r.x() < 0 || r.y() < 0 ||
				r.x() + r.width() >= width ||
				r.y() + r.height() >= height) {
			break; // TODO always breaks for boxes larger than the viewport
		}
	} while (it != hit_it);
	view_hit(r);
	return true;
}

void GridLayout::view_hit() {
	QRect r = get_hit_rect();
	view_hit(r);
}

void GridLayout::view_hit(const QRect &r) {
	// move view horizontally
	if (r.width() <= width * (1 - 2 * search_padding)) {
		if (r.x() < width * search_padding) {
			scroll_smooth(width * search_padding - r.x(), 0);
		} else if (r.x() + r.width() > width * (1 - search_padding)) {
			scroll_smooth(width * (1 - search_padding) - r.x() - r.width(), 0);
		}
	} else {
		int center = (width - r.width()) / 2;
		if (center < 0) {
			center = 0;
		}
		scroll_smooth(center - r.x(), 0);
	}
	// vertically
	if (r.height() <= height * (1 - 2 * search_padding)) {
		if (r.y() < height * search_padding) {
			scroll_smooth(0, height * search_padding - r.y());
		} else if (r.y() + r.height() > height * (1 - search_padding)) {
			scroll_smooth(0, height * (1 - search_padding) - r.y() - r.height());
		}
	} else {
		int center = (height - r.height()) / 2;
		if (center < 0) {
			center = 0;
		}
		scroll_smooth(0, center - r.y());
	}
}


QRect GridLayout::get_hit_rect() {
	// calculate offsets
	int page_width = res->get_page_width(hit_page) * size;
	int page_height = ROUND(res->get_page_height(hit_page) * size);

	int center_x = (grid->get_width(hit_page) * size - page_width) / 2;
	int center_y = (grid->get_height(hit_page) * size - page_height) / 2;

	int wpos = off_x;
	if (hit_page % grid->get_column_count() > horizontal_page) {
		for (int i = horizontal_page; i < hit_page % grid->get_column_count(); i++) {
			wpos += grid->get_width(i) * size + useless_gap;
		}
	} else {
		for (int i = horizontal_page; i > hit_page % grid->get_column_count(); i--) {
			wpos -= grid->get_width(i) * size + useless_gap;
		}
	}
	int hpos = off_y;
	if (hit_page > page) {
		for (int i = page / grid->get_column_count();
				i < hit_page / grid->get_column_count(); i++) {
			hpos += grid->get_height(i) * size + useless_gap;
		}
	} else {
		for (int i = page / grid->get_column_count();
				i > hit_page / grid->get_column_count(); i--) {
			hpos -= grid->get_height(i) * size + useless_gap;
		}
	}
	// get search rect coordinates relative to the current view
	QRectF rot = rotate_rect(*hit_it, res->get_page_width(hit_page),
			res->get_page_height(hit_page), res->get_rotation());
	return transform_rect(rot, size, wpos + center_x, hpos + center_y);
}

pair<int,QPointF> GridLayout::get_page_at(int mx, int my) {
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

	int page = cur_page + cur_col;

	return make_pair(page, QPointF(x,y));
}

bool GridLayout::click_mouse(int mx, int my) {
	pair<int,QPointF> page = get_page_at(mx, my);

	// find matching box
	const list<Poppler::LinkGoto *> *l = res->get_links(page.first);
	if (l == NULL) {
		return false;
	}
	for (list<Poppler::LinkGoto *>::const_iterator it = l->begin();
			it != l->end(); ++it) {
		QRectF r = (*it)->linkArea();
		if (page.second.x() >= r.left() && page.second.x() < r.right()) {
			if (page.second.y() < r.top() && page.second.y() >= r.bottom()) {
				int new_page = (*it)->destination().pageNumber();
				// TODO scroll_smooth() to the link position
				scroll_page(new_page - 1, false);
				return true;
			}
		}
	}
	return false;
}

bool GridLayout::goto_page_at(int mx, int my) {
	pair<int,QPointF> page = get_page_at(mx, my);

	set_columns(1, false);
	scroll_page(page.first, false);

	int new_page_height = ROUND(res->get_page_height(page.first) * size);
	int new_y = page.second.y() * new_page_height;

	scroll_smooth(0, -new_y + height/2 - off_y);
	return true;
}

bool GridLayout::page_visible(int p) const {
	if (p < page || p > last_visible_page) {
		return false;
	}
	return true;
}

