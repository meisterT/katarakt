#ifndef PRESENTATIONLAYOUT_H
#define PRESENTATIONLAYOUT_H

#include "layout.h"


class PresentationLayout : public Layout {
public:
	PresentationLayout(ResourceManager *res, int page = 0);
	PresentationLayout(Layout& old_layout);
	~PresentationLayout() {};

	bool supports_smooth_scrolling() const;
	void render(QPainter *painter);

	bool advance_hit(bool forward = true);
	bool advance_invisible_hit(bool forward = true);

	bool click_mouse(int mx, int my);

	bool page_visible(int p) const;

private:
	int calculate_fit_width(int page) const;
	void view_hit();
};

#endif

