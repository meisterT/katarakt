#ifndef TOC_H
#define TOC_H

#include <QTreeWidget>


class QDomDocument;
class QDomNode;
class Viewer;


class Toc : public QTreeWidget {
	Q_OBJECT

public:
	Toc(Viewer *v, QWidget *parent = 0);
	~Toc();

	void init();

public slots:
	void goto_link(QTreeWidgetItem *item, int column);

protected:
	bool event(QEvent *e);

private:
	void shutdown();
	void build(QDomNode *node, QTreeWidgetItem *parent);

	Viewer *viewer;
};

#endif

