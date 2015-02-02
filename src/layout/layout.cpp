#include <iostream>
#include <QImage>
#include "layout.h"
#include "../viewer.h"
#include "../resourcemanager.h"
#include "../grid.h"
#include "../search.h"
#include "../config.h"
#include "../beamerwindow.h"

using namespace std;


//==[ Layout ]=================================================================
Layout::Layout(Viewer *v, int _page) :
		viewer(v), res(v->get_res()),
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

bool Layout::set_zoom(int /*new_zoom*/, bool /*relative*/) {
	// implement in child classes where necessary
	return false;
}

bool Layout::set_columns(int /*new_columns*/, bool /*relative*/) {
	// only useful for grid layout
	return false;
}

bool Layout::set_offset(int /*new_offset*/, bool /*relative*/) {
	// only useful for grid layout
	return false;
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

	if (page != old_page) {
		viewer->get_beamer()->set_page(page);
		return true;
	} else {
		return false;
	}
}

void Layout::update_search() {
	const map<int,QList<QRectF> *> *hits = viewer->get_search_bar()->get_hits();
	if (hits->empty()) {
		return;
	}

	// find the right page before/after the current one
	map<int,QList<QRectF> *>::const_iterator it = hits->lower_bound(page);
	bool forward = viewer->get_search_bar()->is_search_forward();
	if (forward) {
		if (it == hits->end()) {
			it = hits->lower_bound(0);
		}
	} else {
		if (it->first != page) {
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
	view_hit();
}

void Layout::set_search_visible(bool visible) {
	search_visible = visible;
}

bool Layout::advance_hit(bool forward) {
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
