#ifndef PRESENTERLAYOUT_H
#define PRESENTERLAYOUT_H

#include "layout.h"

class PresenterLayout : public Layout {
public:
	PresenterLayout(Viewer *v, int render_index, int page = 0);
	virtual ~PresenterLayout();

	void rebuild(bool clamp = true);
	void resize(int w, int h);

	void render(QPainter *painter);

	void advance_invisible_hit(bool forward = true);

	std::pair<int, QPointF> get_location_at(int pixel_x, int pixel_y) const;
	bool page_visible(int p) const;

protected:
	int calculate_fit_width(int page) const;

	float main_ratio;
	float optimized_ratio;
	bool horizontal_split; // true if main slide is on the left
};

#endif

