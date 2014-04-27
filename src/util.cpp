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

QRect transform_rect(const QRectF &rect, float scale, int off_x, int off_y) {
	static int rect_expansion = CFG::get_instance()->get_value("rect_expansion").toInt();
	return QRect(rect.x() * scale + off_x - rect_expansion,
			rect.y() * scale + off_y - rect_expansion,
			rect.width() * scale + 2 * rect_expansion,
			rect.height() * scale + 2 * rect_expansion);
}

