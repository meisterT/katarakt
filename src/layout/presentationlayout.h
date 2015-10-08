#ifndef PRESENTATIONLAYOUT_H
#define PRESENTATIONLAYOUT_H

#include "layout.h"


class PresentationLayout : public Layout {
public:
	PresentationLayout(Viewer *v, int render_index, int page = 0);
	~PresentationLayout() {};

	const QRect calculate_placement(int page) const;
	void render(QPainter *painter);

	void advance_invisible_hit(bool forward = true);

	std::pair<int, QPointF> get_location_at(int px, int py) const;

	bool page_visible(int p) const;

private:
	int calculate_fit_width(int page) const;
};

#endif

