#include <iostream>
#include <QImage>
#include <QApplication>
#include <set>
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

const QRect PresentationLayout::calculate_placement(int page) const {
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
	return QRect(center_x, center_y, page_width, page_height);
}

void PresentationLayout::render(QPainter *painter) {
	const QRect p = calculate_placement(page);
	const KPage *k_page = res->get_page(page, p.width());
	if (k_page != NULL) {
		const QImage *img = k_page->get_image();
		if (img != NULL) {
			int rot = (res->get_rotation() - k_page->get_rotation() + 4) % 4;
			QRect rect;
			painter->rotate(rot * 90);
			// calculate page position
			if (rot == 0) {
				rect = p;
//				rect = QRect(p.x(), p.y(), p.width(), p.height());
			} else if (rot == 1) {
				rect = QRect(p.y(), -p.x() - p.width(),
						p.height(), p.width());
			} else if (rot == 2) {
				rect = QRect(-p.x() - p.width(), -p.y() - p.height(),
						p.width(), p.height());
			} else if (rot == 3) {
				rect = QRect(-p.y() - p.height(), p.x(),
						p.height(), p.width());
			}
			if (p.width() != k_page->get_width() || rot != 0) { // draw scaled
				painter->drawImage(rect, *img);
			} else { // draw as-is
				painter->drawImage(rect.topLeft(), *img);
			}
			painter->rotate(-rot * 90);
		}
		res->unlock_page(page);
	}

	// draw search rects
	float factor = p.width() / res->get_page_width(page);
	if (search_visible) {
		render_search_rects(painter, page, p.topLeft(), factor);
	}

	// draw text selection
	render_selection(painter, page, p.topLeft(), factor);

	// draw goto link rects
/*	const list<Poppler::LinkGoto *> *l = res->get_links(page);
	if (l != NULL) {
		painter->setPen(QColor(0, 0, 255));
		painter->setBrush(QColor(0, 0, 255, 64));
		for (list<Poppler::LinkGoto *>::const_iterator it = l->begin();
				it != l->end(); ++it) {
			QRectF r = (*it)->linkArea();
			r.setLeft(r.left() * p.width());
			r.setTop(r.top() * p.height());
			r.setRight(r.right() * p.width());
			r.setBottom(r.bottom() * p.height());
			r.translate(p.x(), p.y());
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

bool PresentationLayout::view_hit() {
	return scroll_page(hit_page, false);
}

pair<int, QPointF> PresentationLayout::get_location_at(int px, int py) {
	const QRect p = calculate_placement(page);

	// transform mouse coordinates
	float x = (px - p.x()) / (float) p.width();
	float y = (py - p.y()) / (float) p.height();

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

	return make_pair(page, QPointF(x, y));
}

bool PresentationLayout::page_visible(int p) const {
	return p == page;
}

