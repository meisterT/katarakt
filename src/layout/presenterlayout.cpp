#include "presenterlayout.h"
#include "../resourcemanager.h"
#include "../util.h"
#include <iostream>

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
		const KPage *k_page = res->get_page(page + i, page_width[i], i);
		if (k_page != NULL) {
			const QImage *img = k_page->get_image(i);
			if (img != NULL) {
				int rot = (res->get_rotation() - k_page->get_rotation(i) + 4) % 4;
				QRect rect;
				painter->rotate(rot* 90);
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

	// prefetch
	for (int count = 1; count <= prefetch_count; count++) {
		// after current page
		if (res->get_page(page + count, calculate_fit_width(page + count), 0) != NULL) {
			res->unlock_page(page + count);
		}
		// before current page
		if (res->get_page(page - count, calculate_fit_width(page - count), 0) != NULL) {
			res->unlock_page(page - count);
		}
	}
	res->collect_garbage(page - prefetch_count * 3, page + 1 + prefetch_count * 3);
}

bool PresenterLayout::advance_invisible_hit(bool /*forward*/) {
	return false;
}

bool PresenterLayout::page_visible(int /*p*/) const {
	return true;
}

void PresenterLayout::view_hit() {
}
