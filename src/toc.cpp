#include <QEvent>
#include <QKeyEvent>
#include <QDomDocument>
#include <QDomNode>
#include <QHeaderView>
#include "toc.h"
#include "viewer.h"
#include "canvas.h"
#include "layout/layout.h"
#include "resourcemanager.h"
#include "util.h"


Q_DECLARE_METATYPE(Poppler::LinkDestination *)


Toc::Toc(Viewer *v, QWidget *parent) :
		QTreeWidget(parent), viewer(v) {
	setColumnCount(2);

	QHeaderView *h = header();
	h->setStretchLastSection(false);
	h->setResizeMode(0, QHeaderView::Stretch);
	h->setResizeMode(1, QHeaderView::ResizeToContents);

	QStringList list = QStringList() << "Contents" << QString();
	setHeaderLabels(list);

	setAlternatingRowColors(true);

	connect(this, SIGNAL(itemActivated(QTreeWidgetItem *, int)), this, SLOT(goto_link(QTreeWidgetItem *, int)), Qt::UniqueConnection);

	init();
}

void Toc::init() {
	shutdown();

	QDomDocument *contents = viewer->get_res()->get_toc();
	if (contents != NULL) {
		build(contents, invisibleRootItem());
		delete contents;
	}

	// indicate empty toc
	if (topLevelItemCount() == 0) {
		QTreeWidgetItem *item = new QTreeWidgetItem(invisibleRootItem(), QStringList("(empty)"));
		item->setFlags(Qt::NoItemFlags);
	}
}

Toc::~Toc() {
	shutdown();
}

void Toc::shutdown() {
	QTreeWidgetItemIterator it(this);
	while (*it) {
		delete (*it)->data(0, Qt::UserRole).value<Poppler::LinkDestination *>();
		++it;
	}

	clear();
}

void Toc::goto_link(QTreeWidgetItem *item, int column) {
	if (column == -1) {
		return;
	}
	// handle empty-indicator
	if (item->text(1).isEmpty()) {
		return;
	}

	Poppler::LinkDestination *link = item->data(0, Qt::UserRole).value<Poppler::LinkDestination *>();
	viewer->get_canvas()->get_layout()->goto_link_destination(*link);
	viewer->get_canvas()->setFocus(Qt::OtherFocusReason);
}

bool Toc::event(QEvent *e) {
	if (e->type() == QEvent::ShortcutOverride) {
		QKeyEvent *ke = static_cast<QKeyEvent *>(e);
		if (ke->key() < Qt::Key_Escape) {
			// don't accept -> other keyboard shortcuts take precedence
//			ke->accept();
		} else {
			switch (ke->key()) {
				case Qt::Key_Return:
				case Qt::Key_Enter:
				case Qt::Key_Delete:
				case Qt::Key_Home:
				case Qt::Key_End:
				case Qt::Key_Backspace:
				case Qt::Key_Left:
				case Qt::Key_Right:
				case Qt::Key_Up:
				case Qt::Key_Down:
				case Qt::Key_Tab:
					ke->accept();
				default:
					break;
			}
		}
	}
	return QTreeWidget::event(e);
}

void Toc::build(QDomNode *node, QTreeWidgetItem *parent) {
	if (node->isNull() || !node->hasChildNodes()) {
		return;
	}

	QDomNodeList list = node->childNodes();
	for (int i = 0; i < list.count(); i++) {
		QDomNode n = list.at(i);

		QStringList strings;
		strings << n.nodeName();
		QDomNamedNodeMap attributes = n.attributes();
		QDomNode dest = attributes.namedItem("Destination");
		Poppler::LinkDestination *link = NULL;
		if (!dest.isNull()) {
//			strings << dest.nodeValue();
			link = new Poppler::LinkDestination(dest.nodeValue());
		} else {
			dest = attributes.namedItem("DestinationName");
			if (!dest.isNull()) {
				link = viewer->get_res()->resolve_link_destination(dest.nodeValue());
//				if (dest_page >= 0) {
//					strings << QString::number(dest_page);
//				}
			}
		}
		if (!dest.isNull() && link != NULL) {
			strings << QString::number(link->pageNumber());
		}
		// TODO check "ExternalFileName"
		// TODO take "Open" into account?

		QTreeWidgetItem *item = new QTreeWidgetItem(parent, strings);
		item->setTextAlignment(1, Qt::AlignRight);
		item->setData(0, Qt::UserRole, QVariant::fromValue(link));

		build(&n, item);
	}
}

