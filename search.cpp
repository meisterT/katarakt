#include <iostream>
#include "search.h"
#include "canvas.h"
#include "viewer.h"
#include "layout.h"
#include "config.h"
#include "util.h"

using namespace std;


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
			emit bar->update_label_text("done.");
			continue;
		}
		int start = bar->start_page;
		QString search_term = bar->term;
		bar->term_mutex.unlock();

#ifdef DEBUG
		cerr << "'" << search_term.toUtf8().constData() << "'" << endl;
#endif
		emit bar->update_label_text("0\% searched, 0 hits");

		// search all pages
		int hit_count = 0;
		int page = start;
		do {
			Poppler::Page *p = bar->doc->page(page);
			if (p == NULL) {
				cerr << "failed to load page " << page << endl;
				continue;
			}

			// collect all occurrences
			QList<QRectF> *hits = new QList<QRectF>;
			// TODO option for case sensitive
#if POPPLER_VERSION < POPPLER_VERSION_CHECK(0, 22, 0)
			// old search interface, slow for many hits per page
			double x = 0, y = 0, x2 = 0, y2 = 0;
			while (!stop && !die &&
					p->search(search_term, x, y, x2, y2, Poppler::Page::NextResult,
						Poppler::Page::CaseInsensitive)) {
				hits->push_back(QRectF(x, y, x2 - x, y2 - y));
			}
#else
			// new search interface
			QList<QRectF> tmp = p->search(search_term, Poppler::Page::CaseInsensitive);
			hits->swap(tmp);
#endif
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
				hit_count += hits->size();
				emit bar->search_done(page, hits);
			} else {
				delete hits;
			}

			// update progress label next to the search bar
			int percent = ((page + bar->doc->numPages() - start)
					% bar->doc->numPages()) * 100 / bar->doc->numPages();
			QString progress = QString("%1\% searched, %2 hits")
				.arg(percent)
				.arg(hit_count);
			emit bar->update_label_text(progress);

			if (++page == bar->doc->numPages()) {
				page = 0;
			}
		} while (page != start);
#ifdef DEBUG
		cerr << "done!" << endl;
#endif
		emit bar->update_label_text(QString("done, %1 hits").arg(hit_count));
	}
}


//==[ SearchBar ]==============================================================
SearchBar::SearchBar(QString file, Viewer *v, QWidget *parent) :
		QWidget(parent),
		viewer(v) {
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
	line = new QLineEdit(parent);

	progress = new QLabel("done.");
	progress->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(line);
	layout->addWidget(progress);
	setLayout(layout);

	initialize(file, QByteArray());
}

void SearchBar::initialize(QString &file, const QByteArray &password) {
	worker = NULL;

	doc = Poppler::Document::load(file, QByteArray(), password);

	if (doc == NULL) {
		// poppler already prints a debug message
		return;
	}
	if (doc->isLocked()) {
		// poppler already prints a debug message
//		cerr << "missing password" << endl;
		delete doc;
		doc = NULL;
		return;
	}
	worker = new SearchWorker(this);
	worker->start();

	connect(line, SIGNAL(returnPressed()), this, SLOT(set_text()),
			Qt::UniqueConnection);
	connect(this, SIGNAL(update_label_text(const QString &)),
			progress, SLOT(setText(const QString &)), Qt::UniqueConnection);
}

SearchBar::~SearchBar() {
	shutdown();
	delete layout;
	delete progress;
	delete line;
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

void SearchBar::load(QString &file, const QByteArray &password) {
	shutdown();
	initialize(file, password);

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
	connect(this, SIGNAL(search_done(int, QList<QRectF> *)),
			c, SLOT(search_done(int, QList<QRectF> *)),
			Qt::UniqueConnection);
	connect(this, SIGNAL(search_visible(bool)),
			c, SLOT(search_visible(bool)), Qt::UniqueConnection);
}

void SearchBar::focus() {
	line->setText(term);
	line->setFocus(Qt::OtherFocusReason);
	line->selectAll();
	show();
}

bool SearchBar::event(QEvent *event) {
	if (event->type() == QEvent::Hide) {
		emit search_visible(false);
		return true;
	} else if (event->type() == QEvent::Show) {
		emit search_visible(true);
		return true;
	}
	return QWidget::event(event);
}

void SearchBar::set_text() {
	// prevent searching a non-existing document
	if (!is_valid()) {
		return;
	}
	// do not start the same search again but signal slots
	if (term == line->text()) {
		emit search_done(viewer->get_canvas()->get_layout()->get_page(), NULL);
		viewer->get_canvas()->setFocus(Qt::OtherFocusReason);
		return;
	}

	term_mutex.lock();
	start_page = viewer->get_canvas()->get_layout()->get_page();
	term = line->text();
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

