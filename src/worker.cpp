#include "worker.h"
#include "resourcemanager.h"
#include "kpage.h"
#include "canvas.h"
#include "selection.h"
#include <list>
#include <iostream>
#include <poppler/qt4/poppler-qt4.h>

using namespace std;


Worker::Worker(ResourceManager *res) :
		die(false),
		res(res) {
}

void Worker::run() {
	while (1) {
		res->requestSemaphore.acquire(1);
		if (die) {
			break;
		}

		// get next page to render
		res->requestMutex.lock();
		int page, width, index;
		map<int,pair<int,int> >::iterator less = res->requests.lower_bound(res->center_page);
		map<int,pair<int,int> >::iterator greater = less--;

		if (greater != res->requests.end()) {
			if (greater != res->requests.begin()) {
				// favour nearby page, go down first
				if (greater->first + less->first <= res->center_page * 2) {
					page = greater->first;
					index = greater->second.first;
					width = greater->second.second;
					res->requests.erase(greater);
				} else {
					page = less->first;
					index = less->second.first;
					width = less->second.second;
					res->requests.erase(less);
				}
			} else {
				page = greater->first;
				index = greater->second.first;
				width = greater->second.second;
				res->requests.erase(greater);
			}
		} else {
			page = less->first;
			index = less->second.first;
			width = less->second.second;
			res->requests.erase(less);
		}
		res->requestMutex.unlock();

		// check for duplicate requests
		res->k_page[page].mutex.lock();
		if (res->k_page[page].status[index] == width &&
				res->k_page[page].rotation[index] == res->rotation) {
			res->k_page[page].mutex.unlock();
			continue;
		}
		int rotation = res->rotation;
		res->k_page[page].mutex.unlock();

		// open page
#ifdef DEBUG
		cerr << "    rendering page " << page << " for index " << index << endl;
#endif
		Poppler::Page *p = res->doc->page(page);
		if (p == NULL) {
			cerr << "failed to load page " << page << endl;
			continue;
		}

		// render page
		float dpi = 72.0 * width / res->get_page_width(page);
		QImage img = p->renderToImage(dpi, dpi, -1, -1, -1, -1,
				static_cast<Poppler::Page::Rotation>(rotation));

		if (img.isNull()) {
			cerr << "failed to render page " << page << endl;
			continue;
		}

		// invert to current color setting
		if (res->inverted_colors) {
			img.invertPixels();
		}

		// put page
		res->k_page[page].mutex.lock();
		if (!res->k_page[page].img[index].isNull()) {
			res->k_page[page].img[index] = QImage(); // assign null image
		}

		// adjust all available images to current color setting
		if (res->k_page[page].inverted_colors != res->inverted_colors) {
			res->k_page[page].inverted_colors = res->inverted_colors;
			for (int i = 0; i < 3; i++) {
				res->k_page[page].img[i].invertPixels();
			}
			res->k_page[page].thumbnail.invertPixels();
		}
		res->k_page[page].img[index] = img;
		res->k_page[page].status[index] = width;
		res->k_page[page].rotation[index] = rotation;
		res->k_page[page].mutex.unlock();

		res->garbageMutex.lock();
		res->garbage.insert(page); // TODO add index information?
		res->garbageMutex.unlock();

		emit page_rendered(page);

		// collect goto links
		res->link_mutex.lock();
		if (res->k_page[page].links == NULL) {
			res->link_mutex.unlock();

			QList<Poppler::Link *> *links = new QList<Poppler::Link *>;
			QList<Poppler::Link *> l = p->links();
			links->swap(l);

			res->link_mutex.lock();
			res->k_page[page].links = links;
		}
		if (res->k_page[page].text == NULL) {
			res->link_mutex.unlock();

			QList<Poppler::TextBox *> text = p->textList();
			// assign boxes to lines
			// make single parts from chained boxes
			set<Poppler::TextBox *> used;
			QList<SelectionPart *> selection_parts;
			Q_FOREACH(Poppler::TextBox *box, text) {
				if (used.find(box) != used.end()) {
					continue;
				}
				used.insert(box);

				SelectionPart *p = new SelectionPart(box);
				selection_parts.push_back(p);
				Poppler::TextBox *next = box->nextWord();
				while (next != NULL) {
					used.insert(next);
					p->add_word(next);
					next = next->nextWord();
				}
			}

			// sort by y coordinate
			qStableSort(selection_parts.begin(), selection_parts.end(), selection_less_y);

			QRectF line_box;
			QList<SelectionLine *> *lines = new QList<SelectionLine *>();
			Q_FOREACH(SelectionPart *part, selection_parts) {
				QRectF box = part->get_bbox();
				// box fits into line_box's line
				if (!lines->empty() && box.y() <= line_box.center().y() && box.bottom() > line_box.center().y()) {
					float ratio_w = box.width() / line_box.width();
					float ratio_h = box.height() / line_box.height();
					if (ratio_w < 1.0f) {
						ratio_w = 1.0f / ratio_w;
					}
					if (ratio_h < 1.0f) {
						ratio_h = 1.0f / ratio_h;
					}
					if (ratio_w > 1.3f && ratio_h > 1.3f) {
						lines->back()->sort();
						lines->push_back(new SelectionLine(part));
						line_box = part->get_bbox();
					} else {
						lines->back()->add_part(part);
					}
				// it doesn't fit, create new line
				} else {
					if (!lines->empty()) {
						lines->back()->sort();
					}
					lines->push_back(new SelectionLine(part));
					line_box = part->get_bbox();
				}
			}
			if (!lines->empty()) {
				lines->back()->sort();
			}

			res->link_mutex.lock();
			res->k_page[page].text = lines;
		}
		res->link_mutex.unlock();

		delete p;
	}
}


