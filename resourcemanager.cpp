#include "resourcemanager.h"

using namespace std;

ResourceManager::ResourceManager(QString file) :
		basePage(-1) {
	doc = Poppler::Document::load(file);
	doc->setRenderHint(Poppler::Document::Antialiasing, true);
	doc->setRenderHint(Poppler::Document::TextAntialiasing, true);
	doc->setRenderHint(Poppler::Document::TextHinting, true);
	doc->setRenderHint(Poppler::Document::TextSlightHinting, true);
	if (doc == NULL) {
		cerr << "failed to open file" << endl;
		return;
	}
	if (doc->isLocked()) {
		cerr << "missing password" << endl;
		delete doc;
		doc = NULL;
		return;
	}
}

ResourceManager::~ResourceManager() {
	delete doc;
}

bool ResourceManager::isNull() const {
	return (doc == NULL);
}

QImage *ResourceManager::getPage(int newPage, int newWidth) {
	if (width != newWidth) {
		basePage = newPage;
		width = newWidth;
		image[1] = QImage();
		return render(0) ? &image[0] : NULL;
	}

	if (basePage == -1) {
		basePage = newPage;
		width = newWidth;
		return render(0) ? &image[0] : NULL;
	}
	switch (newPage - basePage) {
		case -1:
			basePage--;
			image[1] = image[0];
			return render(0) ? &image[0] : NULL;
			break;
		case 0:
			return &image[0];
			break;
		case 1:
			if (image[1].isNull()) {
				return render(1) ? &image[1] : NULL;
			} else {
				return &image[1];
			}
			break;
		case 2:
			basePage++;
			image[0] = image[1];
			return render(1) ? &image[1] : NULL;
			break;
		default:
			basePage = newPage;
			image[1] = QImage();
			return render(0) ? &image[0] : NULL;
	}

/*	if (page == newPage && width == newWidth) {
		return &image[0];
	}

	return &image[0]; */
}

bool ResourceManager::render(int offset) {
	using namespace std;
	cout << "    rerendering page " << (basePage + offset) << endl;
	Poppler::Page *p = doc->page(basePage + offset);
	if (p== NULL) {
		cerr << "failed to load page " << (basePage + offset) << endl;
		return false;
	}
	float dpi = 72.0 * width / p->pageSizeF().width();
	image[offset] = p->renderToImage(dpi, dpi);
	if (image[offset].isNull()) {
		cerr << "failed to render page " << (basePage + offset) << endl;
		return false;
	}
	return true;
}

int ResourceManager::numPages() const {
	return doc->numPages();
}

