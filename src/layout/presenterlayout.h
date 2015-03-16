#ifndef PRESENTERLAYOUT_H
#define PRESENTERLAYOUT_H

#include "layout.h"

class PresenterLayout : public Layout {
public:
	PresenterLayout(Viewer *v, int page = 0);
	PresenterLayout(Layout &old_layout);
	virtual ~PresenterLayout();

	void rebuild(bool clamp = true);
	void resize(int w, int h);

	bool supports_smooth_scrolling() const;
	void render(QPainter *painter);

	bool advance_hit(bool forward = true);
	bool advance_invisible_hit(bool forward = true);

	bool click_mouse(int mx, int my);
	bool page_visible(int p) const;

protected:
	int calculate_fit_width(int page) const;
	bool view_hit();

	float main_ratio;
	float optimized_ratio;
	bool horizontal_split; // true if main slide is on the left
};

#endif

