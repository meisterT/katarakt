#include <iostream>
#include <QImage>
#include "presentationlayout.h"
#include "layout.h"
#include "../util.h"
#include "../viewer.h"
#include "../resourcemanager.h"
#include "../search.h"
#include "../config.h"
#include "../kpage.h"

using namespace std;


PresentationLayout::PresentationLayout(Viewer *v, int page) :
		Layout(v, page) {
}

PresentationLayout::PresentationLayout(Layout& old_layout) :
		Layout(old_layout) {
}

bool PresentationLayout::supports_smooth_scrolling() const {
	return false;
}

int PresentationLayout::calculate_fit_width(int page) const {
	if ((float) width / height > res->get_page_aspect(page)) {
		return res->get_page_aspect(page) * height;
	}
	return width;
}

void PresentationLayout::render(QPainter *painter) {
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
	const KPage *k_page = res->get_page(page, page_width);
	if (k_page != NULL) {
		const QImage *img = k_page->get_image();
		if (img != NULL) {
			int rot = (res->get_rotation() - k_page->get_rotation() + 4) % 4;
			QRect rect;
			painter->rotate(rot* 90);
			// calculate page position
			if (rot == 0) {
				rect = QRect(center_x, center_y, page_width, page_height);
			} else if (rot == 1) {
				rect = QRect(center_y, -center_x - page_width,
						page_height, page_width);
			} else if (rot == 2) {
				rect = QRect(-center_x - page_width, -center_y - page_height,
						page_width, page_height);
			} else if (rot == 3) {
				rect = QRect(-center_y - page_height, center_x,
						page_height, page_width);
			}
			if (page_width != k_page->get_width() || rot != 0) { // draw scaled
				painter->drawImage(rect, *img);
			} else { // draw as-is
				painter->drawImage(rect.topLeft(), *img);
			}
			painter->rotate(-rot * 90);
		}
		res->unlock_page(page);
	}

	// draw search rects
	if (search_visible) {
		painter->setPen(QColor(0, 0, 0));
		painter->setBrush(QColor(255, 0, 0, 64));
		float w = res->get_page_width(page);
		float h = res->get_page_height(page);
		float factor = page_width / w;

		const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();
		map<int,QList<QRectF> *>::const_iterator it = hits->find(page);
		if (it != hits->end()) {
			for (QList<QRectF>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
				if (i2 == hit_it) {
					painter->setBrush(QColor(0, 255, 0, 64));
				}
				QRectF rot = rotate_rect(*i2, w, h, res->get_rotation());
				painter->drawRect(transform_rect(rot, factor, center_x, center_y));
				if (i2 == hit_it) {
					painter->setBrush(QColor(255, 0, 0, 64));
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
	for (int count = 1; count <= prefetch_count; count++) {
		// after current page
		if (res->get_page(page + count, calculate_fit_width(page + count)) != NULL) {
			res->unlock_page(page + count);
		}
		// before current page
		if (res->get_page(page - count, calculate_fit_width(page - count)) != NULL) {
			res->unlock_page(page - count);
		}
	}
	res->collect_garbage(page - prefetch_count * 3, page + prefetch_count * 3);
}

bool PresentationLayout::advance_hit(bool forward) {
	if (Layout::advance_hit(forward)) {
		view_hit();
		return true;
	}
	return false;
}

bool PresentationLayout::advance_invisible_hit(bool forward) {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();

	if (hits->empty()) {
		return false;
	}

	if (forward) {
		hit_it = hits->find(hit_page)->second->end();
		--hit_it;
	} else {
		hit_it = hits->find(hit_page)->second->begin();
	}
	Layout::advance_hit(forward);
	view_hit();
	return true;
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
	const list<Poppler::LinkGoto *> *l = res->get_links(page);
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
	return false;
}

bool PresentationLayout::page_visible(int p) const {
	return p == page;
}

