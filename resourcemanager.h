#ifndef RESOURCEMANAGER_H
#define RESOURCEMANAGER_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QImage>
#include <iostream>

class ResourceManager {

public:
	ResourceManager(QString file);
	~ResourceManager();

	bool isNull() const;
	QImage *getPage(int newPage, int newWidth);
	int numPages() const;

private:
	bool render(int offset);

	Poppler::Document *doc;
	int basePage;
	QImage image[2];
	int width;

};

#endif

