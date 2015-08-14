#include <QAction>
#include <QObject>
#include "util.h"
#include "config.h"


const QRectF rotate_rect(const QRectF &rect, float w, float h, int rotation) {
	if (rotation == 0) {
		return rect;
	} else if (rotation == 1) {
		return QRectF(w - rect.bottom(), rect.left(), rect.height(), rect.width());
	} else if (rotation == 2) {
		return QRectF(w - rect.right(), h - rect.bottom(), rect.width(), rect.height());
	} else {
		return QRectF(rect.top(), h - rect.right(), rect.height(), rect.width());
	}
}

const QPointF rotate_point(const QPointF &point, float w, float h, int rotation) {
	if (rotation == 0) {
		return point;
	} else if (rotation == 1) {
		return QPointF(h - point.y(), point.x());
	} else if (rotation == 2) {
		return QPointF(w - point.x(), h - point.y());
	} else {
		return QPointF(point.y(), w - point.x());
	}
}

QRect transform_rect(const QRectF &rect, float scale, int off_x, int off_y) {
	return QRect(rect.x() * scale + off_x,
			rect.y() * scale + off_y,
			rect.width() * scale,
			rect.height() * scale);
}

QRect transform_rect_expand(const QRectF &rect, float scale, int off_x, int off_y) {
	static int rect_expansion = CFG::get_instance()->get_value("rect_expansion").toInt();
	return QRect(rect.x() * scale + off_x - rect_expansion,
			rect.y() * scale + off_y - rect_expansion,
			rect.width() * scale + 2 * rect_expansion,
			rect.height() * scale + 2 * rect_expansion);
}

void add_action(QWidget *base, const char *action, const char *slot, QWidget *receiver) {
	QStringListIterator i(CFG::get_instance()->get_keys(action));
	while (i.hasNext()) {
		QAction *a = new QAction(base);
		a->setShortcut(QKeySequence(i.next()));
		base->addAction(a);
		QObject::connect(a, SIGNAL(triggered()), receiver, slot);
	}
}

