#include <iostream>
#include <QImage>
#include <QDesktopServices>
#include <QUrl>
#include <QApplication>
#include "layout.h"
#include "../viewer.h"
#include "../resourcemanager.h"
#include "../grid.h"
#include "../search.h"
#include "../config.h"
#include "../beamerwindow.h"
#include "../util.h"

using namespace std;


//==[ Layout ]=================================================================
Layout::Layout(Viewer *v, int render_index, int _page) :
		viewer(v), res(v->get_res()),
		render_index(render_index),
		page(_page), width(0), height(0),
		search_visible(false) {
	// load config options
	CFG *config = CFG::get_instance();
	useless_gap = config->get_value("useless_gap").toInt();
	min_page_width = config->get_value("min_page_width").toInt();
	min_zoom = config->get_value("min_zoom").toInt();
	max_zoom = config->get_value("max_zoom").toInt();
	zoom_factor = config->get_value("zoom_factor").toFloat();
	prefetch_count = config->get_value("prefetch_count").toInt();
	search_padding = config->get_value("search_padding").toFloat();
}

Layout::~Layout() {
	// nothing here
	// destructor call could be layout change -> data still needed
}

int Layout::get_page() const {
	return page;
}

void Layout::activate(const Layout *old_layout) {
	page = old_layout->get_page();
	width = old_layout->width;
	height = old_layout->height;

	search_visible = old_layout->search_visible;
	hit_page = old_layout->hit_page;
	hit_it = old_layout->hit_it;

	selection = old_layout->selection;
}

void Layout::rebuild(bool clamp) {
	// clamp to available pages
	// not clamping is useful when inotify reloads a broken TeX document;
	// you don't have to scroll back to the previous position after fixing it
	if (page < 0) {
		page = 0;
	}
	if (clamp && page >= res->get_page_count()) {
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

void Layout::set_offset(int /*new_offset*/, bool /*relative*/) {
	// only useful for grid layout
}

bool Layout::supports_smooth_scrolling() const {
	// override if layout supports smooth scrolling
	return false;
}

void Layout::scroll_smooth(int /*dx*/, int /*dy*/) {
	// implement in child classes where necessary
}

void Layout::scroll_page(int new_page, bool relative) {
	if (scroll_page_noupdate(new_page, relative)) {
		viewer->layout_updated(page, true);
	}
}

void Layout::scroll_page_jump(int new_page, bool relative) {
	res->store_jump(get_page());
	scroll_page(new_page, relative);
}

void Layout::scroll_page_top_jump(int new_page, bool relative) {
	// override if layout supports smooth scrolling
	scroll_page_jump(new_page, relative);
}

void Layout::update_search() {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();
	if (hits->empty()) {
		return;
	}

	// find the right page before/after the current one
	map<int,QList<QRectF> *>::const_iterator it = hits->lower_bound(get_page());
	bool forward = viewer->get_search_bar()->is_search_forward();
	if (forward) {
		if (it == hits->end()) {
			it = hits->lower_bound(0);
		}
	} else {
		if (it->first != get_page()) {
			if (it == hits->begin()) {
				it = hits->end();
			}
			--it;
		}
	}

	hit_page = it->first;
	if (forward) {
		hit_it = it->second->begin();
	} else {
		hit_it = it->second->end();
		--hit_it;
	}
	res->store_jump(get_page());
	view_hit();
}

void Layout::set_search_visible(bool visible) {
	search_visible = visible;
}

bool Layout::scroll_page_noupdate(int new_page, bool relative) {
	int old_page = page;
	if (relative) {
		page += new_page;
	} else {
		page = new_page;
	}
	if (page < 0) {
		page = 0;
	}
	if (page > res->get_page_count() - 1) {
		page = res->get_page_count() - 1;
	}
	return page != old_page;
}

bool Layout::advance_hit_noupdate(bool forward) {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();

	if (hits->empty()) {
		return false;
	}
	// find next hit
	if (forward ^ !viewer->get_search_bar()->is_search_forward()) {
		++hit_it;
		if (hit_it == hits->find(hit_page)->second->end()) {
			// this was the last hit on hit_page
			map<int,QList<QRectF> *>::const_iterator it = hits->upper_bound(hit_page);
			if (it == hits->end()) { // this was the last page with a hit -> wrap
				it = hits->begin();
			}
			hit_page = it->first;
			hit_it = it->second->begin();
		}
	// find previous hit
	} else {
		if (hit_it == hits->find(hit_page)->second->begin()) {
			// this was the first hit on hit_page
			map<int,QList<QRectF> *>::const_reverse_iterator it(hits->lower_bound(hit_page));
			if (it == hits->rend()) { // this was the first page with a hit -> wrap
				it = hits->rbegin();
			}
			hit_page = it->first;
			hit_it = --(it->second->end());
		} else {
			--hit_it;
		}
	}
	res->store_jump(get_page());
	return true;
}

void Layout::advance_hit(bool forward) {
	if (advance_hit_noupdate(forward)) {
		view_hit();
	}
}

void Layout::goto_link_destination(const Poppler::LinkDestination &link) {
	res->store_jump(get_page());
	scroll_page(link.pageNumber() - 1, false);
}

void Layout::goto_position(int page, QPointF /*pos*/) {
	res->store_jump(get_page());
	scroll_page(page, false);
}

void Layout::goto_page_at(int /*mx*/, int /*my*/) {
	// implement in child classes where necessary
}

bool Layout::get_search_visible() const {
	return search_visible;
}

void Layout::select(int px, int py, enum Selection::Mode mode) {
	pair<int, QPointF> loc = get_location_at(px, py);
	loc.second.rx() *= res->get_page_width(loc.first, false);
	loc.second.ry() *= res->get_page_height(loc.first, false);

	const QList<SelectionLine *> *text = res->get_text(loc.first);
	selection.set_cursor(text, loc, mode);
	viewer->layout_updated(page, false); // TODO visible? change?
}

void Layout::copy_selection_text(QClipboard::Mode mode) const {
	QString text;
	if (selection.is_active()) {
		Cursor from = selection.get_cursor(true);
		Cursor to = selection.get_cursor(false);
		for (int i = from.page; i <= to.page; i++) {
			text += selection.get_selection_text(i, res->get_text(i));
		}
	}
	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText(text, mode);
}

void Layout::clear_selection() {
	selection.deactivate();

	QClipboard *clipboard = QApplication::clipboard();
	clipboard->setText("", QClipboard::Selection);
}

void Layout::render_search_rects(QPainter *painter, int cur_page, QPoint offset, float size) {
	painter->setPen(QColor(0, 0, 0));
	painter->setBrush(QColor(255, 0, 0, 64));
	float w = res->get_page_width(cur_page);
	float h = res->get_page_height(cur_page);

	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();
	map<int,QList<QRectF> *>::const_iterator it = hits->find(cur_page);
	if (it != hits->end()) {
		for (QList<QRectF>::iterator i2 = it->second->begin(); i2 != it->second->end(); ++i2) {
			if (i2 == hit_it) {
				painter->setBrush(QColor(0, 255, 0, 64));
			}
			QRectF rot = rotate_rect(*i2, w, h, res->get_rotation());
			painter->drawRect(transform_rect_expand(rot, size, offset.x(), offset.y()));
			if (i2 == hit_it) {
				painter->setBrush(QColor(255, 0, 0, 64));
			}
		}
	}
}

void Layout::render_selection(QPainter *painter, int cur_page, QPoint offset, float size) {
	float w = res->get_page_width(cur_page);
	float h = res->get_page_height(cur_page);
	// what's going on?! If I use Qt::NoPen, I can't draw the overlay rect anymore (Canvas:paintEvent)
//	painter->setPen(Qt::NoPen);
	QColor color = QApplication::palette().highlight().color();
	color.setAlpha(0); // TODO this is a workaround
	painter->setPen(color);
	color.setAlpha(96);
	painter->setBrush(color);

	const QList<SelectionLine *> *text = res->get_text(cur_page);
	if (text != NULL && text->size() != 0 && selection.is_active()) {
		Cursor from = selection.get_cursor(true);
		Cursor to = selection.get_cursor(false);
		if (from.page <= cur_page && to.page >= cur_page) {
			if (from.page < cur_page) {
				from.line = 0;
			}
			if (to.page > cur_page) {
				to.line = text->size() - 1;
			}
			for (int i = from.line; i <= to.line; i++) {
				QRectF rect = text->at(i)->get_bbox();
				if (from.page == cur_page && from.line == i) {
					rect.setLeft(from.x);
				}
				if (to.page == cur_page && to.line == i) {
					rect.setRight(to.x);
				}
				QRectF bb = rotate_rect(rect, w, h, res->get_rotation());;
				painter->drawRect(transform_rect(bb, size, offset.x(), offset.y()));
			}
		}
	}
}

void Layout::view_hit() {
	bool page_changed = scroll_page_noupdate(hit_page, false);
	viewer->layout_updated(hit_page, page_changed);
}

void Layout::activate_link(int page, float x, float y) {
	// find matching box
	const QList<Poppler::Link *> *links = res->get_links(page);
	if (links == NULL) {
		return;
	}
	Q_FOREACH(Poppler::Link *l, *links) {
		QRectF r = l->linkArea();
		if (x >= r.left() && x < r.right()) {
			if (y < r.top() && y >= r.bottom()) {
				switch (l->linkType()) {
					case Poppler::Link::Goto: {
						Poppler::LinkGoto *link = static_cast<Poppler::LinkGoto *>(l);
						// TODO support links to other files
						goto_link_destination(link->destination());
						return;
					}
					case Poppler::Link::Browse: {
						Poppler::LinkBrowse *link = static_cast<Poppler::LinkBrowse *>(l);
						QDesktopServices::openUrl(QUrl(link->url()));
						break;
					}
					case Poppler::Link::Execute:
					case Poppler::Link::Action:
					case Poppler::Link::Sound:
					case Poppler::Link::Movie:
					case Poppler::Link::Rendition:
					case Poppler::Link::JavaScript:
					case Poppler::Link::None:
						cerr << "link type not implemented (yet?)" << endl;
				}
			}
		}
	}
}

