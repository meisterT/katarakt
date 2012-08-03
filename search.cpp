#include "search.h"
#include "canvas.h"
#include "viewer.h"
#include "layout.h"
#include <iostream>

using namespace std;


// TODO put in a config source file
#define RECT_EXPANSION 2


//==[ Result ]=================================================================
Result::Result(double _x1, double _y1, double _x2, double _y2) :
		x1(_x1), y1(_y1),
		x2(_x2), y2(_y2) {
}

QRect Result::scale_translate(double factor, double off_x, double off_y) {
	return QRect(x1 * factor + off_x - RECT_EXPANSION, y1 * factor + off_y - RECT_EXPANSION,
			(x2 - x1) * factor + RECT_EXPANSION * 2, (y2 - y1) * factor + RECT_EXPANSION * 2);
}

QRect Result::scale_translate(double factor, double off_x, double off_y) const {
	return QRect(x1 * factor + off_x - RECT_EXPANSION, y1 * factor + off_y - RECT_EXPANSION,
			(x2 - x1) * factor + RECT_EXPANSION * 2, (y2 - y1) * factor + RECT_EXPANSION * 2);
}


//==[ SearchWorker ]===========================================================
SearchWorker::SearchWorker(SearchBar *_bar) :
		stop(false),
		die(false),
		bar(_bar) {
}

void SearchWorker::run() {
	while (1) {
		bar->search_mutex.lock();
		stop = false;
		if (die) {
			break;
		}
		// always clear results -> empty search == stop search
		emit bar->search_clear();

		// get search string
		bar->term_mutex.lock();
		if (bar->term.isEmpty()) {
			bar->term_mutex.unlock();
			continue;
		}
		int start = bar->start_page;
		QString search_term = bar->term;
		bar->term_mutex.unlock();

#ifdef DEBUG
		cerr << "'" << search_term.toUtf8().constData() << "'" << endl;
#endif
		// search all pages
		int page = start;
		do {
			Poppler::Page *p = bar->doc->page(page);
			if (p == NULL) {
				cerr << "failed to load page " << page << endl;
				continue;
			}

			// collect all occurrences
			list<Result> *hits = new list<Result>;
			Result rect;
			// TODO option for case sensitive
			while (!stop && !die &&
					p->search(search_term, rect.x1, rect.y1, rect.x2, rect.y2,
						Poppler::Page::NextResult, Poppler::Page::CaseInsensitive)) {
				hits->push_back(rect);
			}
#ifdef DEBUG
			if (hits->size() > 0) {
				cerr << hits->size() << " hits on page " << page << endl;
			}
#endif
			delete p;

			// clean up when interrupted
			if (stop || die) {
				delete hits;
				break;
			}

			if (hits->size() > 0) {
				emit bar->search_done(page, hits);
			}

			if (++page == bar->doc->numPages()) {
				page = 0;
			}
		} while (page != start);
#ifdef DEBUG
		cerr << "done!" << endl;
#endif
	}
}


//==[ SearchBar ]==============================================================
SearchBar::SearchBar(QString file, Viewer *v, QWidget *parent) :
		QLineEdit(parent),
		viewer(v) {
	initialize(file);
}

void SearchBar::initialize(QString file) {
	worker = NULL;
	doc = Poppler::Document::load(file);
	if (doc == NULL) {
		// poppler already prints a debug message
		return;
	}
	if (doc->isLocked()) {
		cerr << "missing password" << endl;
		delete doc;
		doc = NULL;
		return;
	}
	worker = new SearchWorker(this);
	worker->start();

	connect(this, SIGNAL(returnPressed()), this, SLOT(set_text()),
			Qt::UniqueConnection);
}

SearchBar::~SearchBar() {
	shutdown();
}

void SearchBar::shutdown() {
	if (worker != NULL) {
		join_threads();
	}
	if (doc == NULL) {
		return;
	}
	delete doc;
	delete worker;
}

void SearchBar::load(QString file) {
	shutdown();
	initialize(file);

	// clear old search results if initialisation failed
	if (!is_valid()) {
		term = "";
		emit search_clear();
	}
}

bool SearchBar::is_valid() const {
	return doc != NULL;
}

void SearchBar::connect_canvas(Canvas *c) const {
	connect(this, SIGNAL(search_clear()), c, SLOT(search_clear()),
			Qt::UniqueConnection);
	connect(this, SIGNAL(search_done(int, std::list<Result> *)),
			c, SLOT(search_done(int, std::list<Result> *)),
			Qt::UniqueConnection);
	connect(this, SIGNAL(search_visible(bool)),
			c, SLOT(search_visible(bool)), Qt::UniqueConnection);
}

bool SearchBar::event(QEvent *event) {
	if (event->type() == QEvent::Hide) {
		emit search_visible(false);
		return true;
	} else if (event->type() == QEvent::Show) {
		emit search_visible(true);
		return true;
	}
	return QLineEdit::event(event);
}

void SearchBar::set_text() {
	// prevent searching a non-existing document
	if (!is_valid()) {
		return;
	}
	// do not search for the same term twice
	if (term == text()) {
		return;
	}

	term_mutex.lock();
	start_page = viewer->get_canvas()->get_layout()->get_page();
	term = text();
	term_mutex.unlock();

	worker->stop = true;
	search_mutex.unlock();
	viewer->get_canvas()->setFocus(Qt::OtherFocusReason);
}

void SearchBar::join_threads() {
	worker->die = true;
	search_mutex.unlock();
	worker->wait();
}

