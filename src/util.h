#ifndef UTIL_H
#define UTIL_H

#include <QRect>
#include <QRectF>

#define POPPLER_VERSION ((POPPLER_VERSION_MAJOR << 16) | (POPPLER_VERSION_MINOR << 8) | (POPPLER_VERSION_MICRO))

#define POPPLER_VERSION_CHECK(major,minor,micro) ((major << 16) | (minor << 8) | (micro))

// rounds a float when afterwards cast to int
// seems to fix the mismatch between calculated page height and actual image height
#define ROUND(x) ((x) + 0.5f)
//#define ROUND(x) (x)

const QRectF rotate_rect(const QRectF &rect, float w, float h, int rotation);
const QPointF rotate_point(const QPointF &point, float w, float h, int rotation);

QRect transform_rect(const QRectF &rect, float scale, int off_x, int off_y);
QRect transform_rect_expand(const QRectF &rect, float scale, int off_x, int off_y);

void add_action(QWidget *base, const char *action, const char *slot, QWidget *receiver);

#endif

