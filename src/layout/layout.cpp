#include <iostream>
#include <QImage>
#include "layout.h"
#include "../resourcemanager.h"
#include "../grid.h"
#include "../search.h"
#include "../config.h"

using namespace std;


//==[ Layout ]=================================================================
Layout::Layout(ResourceManager *_res, int _page) :
		res(_res), page(_page), width(0), height(0),
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
	page = old_layout->page;
	width = old_layout->width;
	height = old_layout->height;

	hits = old_layout->hits; // TODO unneccessary copy
	search_visible = old_layout->search_visible;
	hit_page = old_layout->hit_page;
	hit_it = old_layout->hit_it;
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

bool Layout::supports_smooth_scrolling() const {
	// normally a layout supports smooth scrolling
	return true;
}

bool Layout::scroll_smooth(int /*dx*/, int /*dy*/) {
	// implement in child classes where necessary
	return false;
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
	}
	if (page > res->get_page_count() - 1) {
		page = res->get_page_count() - 1;
	}
	return page != old_page;
}

void Layout::clear_hits() {
	for (map<int,QList<QRectF> *>::iterator it = hits.begin(); it != hits.end(); ++it) {
		delete it->second;
	}
	hits.clear();
}

void Layout::set_hits(int page, QList<QRectF> *l) {
	// no new search, just view hit on the current page or below. wraps.
	if (l == NULL) {
		map<int,QList<QRectF> *>::iterator it = hits.lower_bound(page);
		if (it != hits.end()) {
			hit_page = it->first;
			hit_it = hits[hit_page]->begin();
			view_hit();
		} else {
			if (hits.size() != 0) {
				hit_page = hits.lower_bound(0)->first;
				hit_it = hits[hit_page]->begin();
				view_hit();
			}
		}
		return;
	}

	// new search -> initialize highlight
	if (hits.size() == 0) {
		hit_page = page;
		hit_it = l->begin();
		view_hit();
	}

	// just to be safe - prevent memory leaks
	map<int,QList<QRectF> *>::iterator it = hits.find(page);
	if (it != hits.end()) {
		delete it->second;
	}
	hits[page] = l;
}

void Layout::set_search_visible(bool visible) {
	search_visible = visible;
}

bool Layout::advance_hit(bool forward) {
	if (hits.size() == 0) {
		return false;
	}
	// find next hit
	if (forward) {
		++hit_it;
		if (hit_it == hits[hit_page]->end()) { // this was the last hit on that page
			map<int,QList<QRectF> *>::const_iterator it = hits.upper_bound(hit_page);
			if (it == hits.end()) { // this was the last page with a hit -> wrap
				it = hits.begin();
			}
			hit_page = it->first;
			hit_it = it->second->begin();
		}
	// find previous hit
	} else {
		if (hit_it == hits[hit_page]->begin()) { // this was the first hit on that page
			map<int,QList<QRectF> *>::const_reverse_iterator it(hits.lower_bound(hit_page));
			if (it == hits.rend()) { // this was the first page with a hit -> wrap
				it = hits.rbegin();
			}
			hit_page = it->first;
			hit_it = --(it->second->end());
		} else {
			--hit_it;
		}
	}
	return true;
}

bool Layout::click_mouse(int /*mx*/, int /*my*/) {
	return false;
}

bool Layout::goto_page_at(int /*mx*/, int /*my*/) {
	return false;
}

bool Layout::get_search_visible() const {
	return search_visible;
}
