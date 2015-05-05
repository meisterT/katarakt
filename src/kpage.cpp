#include "kpage.h"
#include <QList>
#include "selection.h"

using namespace std;

KPage::KPage() :
		links(NULL),
		inverted_colors(false),
		text(NULL) {
	for (int i = 0; i < 3; i++) {
		status[i] = 0;
		rotation[i] = 0;
	}
}

KPage::~KPage() {
	if (links != NULL) {
		Q_FOREACH(Poppler::Link *l, *links) {
			delete l;
		}
	}
	delete links;
	if (text != NULL) {
		Q_FOREACH(SelectionLine *line, *text) {
			delete line;
		}
	}
	delete text;
}

const QImage *KPage::get_image(int index) const {
	// return any available image, try the right index first
	for (int i = 3; i > 0; i--) {
		if (!img[(index + i) % 3].isNull()) {
			return &img[(index + i) % 3];
		}
	}
	if (thumbnail.isNull()) {
		return NULL;
	} else {
		return &thumbnail;
	}
}

int KPage::get_width(int index) const {
	return status[index];
}

char KPage::get_rotation(int index) const {
	return rotation[index];
}

const QList<SelectionLine *> *KPage::get_text() const {
	return text;
}

//QString KPage::get_label() const {
//	return label;
//}


