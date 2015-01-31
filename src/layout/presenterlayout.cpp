#include "presenterlayout.h"
#include "../resourcemanager.h"
#include "../util.h"
#include "../kpage.h"
#include "../viewer.h"
#include "../search.h"

using namespace std;

PresenterLayout::PresenterLayout(Viewer *v, int page) :
		Layout(v, page),
		main_ratio(0.67) {
	rebuild();
}

PresenterLayout::PresenterLayout(Layout &old_layout) :
		Layout(old_layout),
		main_ratio(0.67) {
	rebuild();
}

PresenterLayout::~PresenterLayout() {
}

void PresenterLayout::rebuild(bool clamp) {
	Layout::rebuild(clamp);
	resize(width, height);
}

void PresenterLayout::resize(int w, int h) {
	Layout::resize(w, h);

	int small_width = width * main_ratio;
	int small_height = height * main_ratio;
	// horizontal: main left, secondary right
	// vertical: main top
	float horiz_aspect = (float) small_width / height;
	float vert_aspect = (float) width / small_height;

	float aspect = res->get_page_aspect(0); // use only the first page consistently
	// calculate size for horizontal split
	int horiz_width, horiz_height;
	if (horiz_aspect > aspect) {
		horiz_width = height * aspect;
		horiz_height = height;
	} else {
		horiz_width = small_width;
		horiz_height = small_width / aspect;
	}

	// calculate size for vertical split
	int vert_width, vert_height;
	if (vert_aspect > aspect) {
		vert_width = small_height * aspect;
		vert_height = small_height;
	} else {
		vert_width = width;
		vert_height = width / aspect;
	}

	if (horiz_width * horiz_height > vert_width * vert_height) {
		horizontal_split = true;
	} else {
		horizontal_split = false;
	}

	optimized_ratio = main_ratio;
	if (horizontal_split) {
		if (horiz_aspect > res->get_max_aspect()) {
			optimized_ratio = res->get_max_aspect() * height / width;
		}
	} else {
		if (vert_aspect < res->get_min_aspect()) {
			optimized_ratio = width / res->get_min_aspect() / height;
		}
	}
}

bool PresenterLayout::supports_smooth_scrolling() const {
	return false;
}

int PresenterLayout::calculate_fit_width(int page) const {
	float aspect = res->get_page_aspect(page);
	int w, h;
	if (horizontal_split) {
		w = optimized_ratio * width;
		h = height;
	} else {
		w = width;
		h = optimized_ratio * height;
	}

	if ((float) w / h > aspect) {
		return h * aspect;
	} else {
		return w;
	}
}

void PresenterLayout::render(QPainter *painter) {
	int page_width[2], page_height[2];
	int center_x[2] = {0, 0};
	int center_y[2] = {0, 0};

	int w[2], h[2];
	if (horizontal_split) {
		w[0] = optimized_ratio * width;
		h[0] = height;
		w[1] = width - w[0] - useless_gap;
		h[1] = height;
	} else {
		w[0] = width;
		h[0] = optimized_ratio * height;
		w[1] = width;
		h[1] = height - h[0] - useless_gap;
	}

	for (int i = 0; i < 2; i++) {
		float aspect = res->get_page_aspect(page + i);
		if ((float) w[i] / h[i] > aspect) {
			page_width[i] = h[i] * aspect;
			page_height[i] = h[i];
			center_x[i] = (w[i] - page_width[i]) / 2;
		} else {
			page_width[i] = w[i];
			page_height[i] = ROUND(w[i] / aspect);
			center_y[i] = (h[i] - page_height[i]) / 2;
		}
	}

	center_x[1] = width - page_width[1];
	if (horizontal_split) {
		center_y[1] = 0;
	} else {
		center_y[1] = height - page_height[1];
	}

	for (int i = 0; i < 2; i++) {
		int index = i + 1;
		const KPage *k_page = res->get_page(page + i, page_width[i], index);
		if (k_page != NULL) {
			const QImage *img = k_page->get_image(index);
			if (img != NULL) {
				int rot = (res->get_rotation() - k_page->get_rotation(index) + 4) % 4;
				QRect rect;
				painter->rotate(rot * 90);
				// calculate page position
				if (rot == 0) {
					rect = QRect(center_x[i], center_y[i], page_width[i], page_height[i]);
				} else if (rot == 1) {
					rect = QRect(center_y[i], -center_x[i] - page_width[i],
							page_height[i], page_width[i]);
				} else if (rot == 2) {
					rect = QRect(-center_x[i] - page_width[i], -center_y[i] - page_height[i],
							page_width[i], page_height[i]);
				} else if (rot == 3) {
					rect = QRect(-center_y[i] - page_height[i], center_x[i],
							page_height[i], page_width[i]);
				}
				if (page_width[i] != k_page->get_width(i) || rot != 0) { // draw scaled
					painter->drawImage(rect, *img);
				} else { // draw as-is
					painter->drawImage(rect.topLeft(), *img);
				}
				painter->rotate(-rot * 90);
			}
			res->unlock_page(page + i);
		}
	}

	// draw search rects
	for (int i = 0; i < 2; i++) {
		if (search_visible) {
			painter->setPen(QColor(0, 0, 0));
			painter->setBrush(QColor(255, 0, 0, 64));
			float w_ = res->get_page_width(page + i);
			float h_ = res->get_page_height(page + i);
			float factor = page_width[i] / w_;

			const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();
			map<int,QList<QRectF> *>::const_iterator it = hits->find(page + i);
			if (it != hits->end()) {
				for (QList<QRectF>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
					if (i2 == hit_it) {
						painter->setBrush(QColor(0, 255, 0, 64));
					}
					QRectF rot = rotate_rect(*i2, w_, h_, res->get_rotation());
					painter->drawRect(transform_rect(rot, factor, center_x[i], center_y[i]));
					if (i2 == hit_it) {
						painter->setBrush(QColor(255, 0, 0, 64));
					}
				}
			}
		}
	}

	// prefetch
	for (int count = 1; count <= prefetch_count; count++) {
		// after current page
		if (res->get_page(page + count, calculate_fit_width(page + count), 1) != NULL) {
			res->unlock_page(page + count);
		}
		// before current page
		if (res->get_page(page - count, calculate_fit_width(page - count), 1) != NULL) {
			res->unlock_page(page - count);
		}
	}
	res->collect_garbage(page - prefetch_count * 3, page + 1 + prefetch_count * 3);
}

bool PresenterLayout::advance_hit(bool forward) {
	if (Layout::advance_hit(forward)) {
		view_hit();
		return true;
	}
	return false;
}

bool PresenterLayout::advance_invisible_hit(bool forward) {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();

	if (hits->empty()) {
		return false;
	}

	if (forward ^ !viewer->get_search_bar()->is_search_forward()) {
		hit_it = hits->find(hit_page)->second->end();
		--hit_it;
	} else {
		hit_it = hits->find(hit_page)->second->begin();
	}
	Layout::advance_hit(forward);
	view_hit();
	return true;
}

void PresenterLayout::view_hit() {
	scroll_page(hit_page, false);
}

bool PresenterLayout::click_mouse(int mx, int my) {
	// TODO duplicate code
	int page_width[2], page_height[2];
	int center_x[2] = {0, 0};
	int center_y[2] = {0, 0};

	int w[2], h[2];
	if (horizontal_split) {
		w[0] = optimized_ratio * width;
		h[0] = height;
		w[1] = width - w[0] - useless_gap;
		h[1] = height;
	} else {
		w[0] = width;
		h[0] = optimized_ratio * height;
		w[1] = width;
		h[1] = height - h[0] - useless_gap;
	}

	for (int i = 0; i < 2; i++) {
		float aspect = res->get_page_aspect(page + i);
		if ((float) w[i] / h[i] > aspect) {
			page_width[i] = h[i] * aspect;
			page_height[i] = h[i];
			center_x[i] = (w[i] - page_width[i]) / 2;
		} else {
			page_width[i] = w[i];
			page_height[i] = ROUND(w[i] / aspect);
			center_y[i] = (h[i] - page_height[i]) / 2;
		}
	}

	center_x[1] = width - page_width[1];
	if (horizontal_split) {
		center_y[1] = 0;
	} else {
		center_y[1] = height - page_height[1];
	}

	for (int i = 0; i < 2; i++) {
		// transform mouse coordinates
		float x = (mx - center_x[i]) / (float) page_width[i];
		float y = (my - center_y[i]) / (float) page_height[i];

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

		// find matching box
		const list<Poppler::LinkGoto *> *l = res->get_links(page + i);
		if (l == NULL) {
			return false;
		}
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
	}
	return false;
}

bool PresenterLayout::page_visible(int p) const {
	return p == page || p == page + 1;
}

