#ifndef KPAGE_H
#define KPAGE_H

#include <QImage>
#include <QMutex>
#include <poppler/qt4/poppler-qt4.h>


class KPage {
private:
	KPage();
	~KPage();

public:
	const QImage *get_image(int index = 0) const;
	int get_width(int index = 0) const;
	char get_rotation(int index = 0) const;
//	QString get_label() const;

private:
	float width;
	float height;
	QImage img[3];
	QImage thumbnail;
//	QString label;
	std::list<Poppler::LinkGoto *> *links;
	QMutex mutex;
	int status[3];
	char rotation[3];
	bool inverted_colors; // img[]s and thumb must be consistent

	friend class Worker;
	friend class ResourceManager;
};

#endif

