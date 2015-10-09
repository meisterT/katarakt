#ifndef CANVAS_H
#define CANVAS_H

#include <QWidget>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QList>
#include <QTimer>
#include <sys/socket.h>


class Viewer;
class Layout;
class PresentationLayout;
class GridLayout;
class PresenterLayout;
class GotoLine;
class QLabel;


class Canvas : public QWidget {
	Q_OBJECT

public:
	Canvas(Viewer *v, QWidget *parent = 0);
	~Canvas();

	bool is_valid() const;
	void reload(bool clamp);

	void set_search_visible(bool visible);

	Layout *get_layout() const;

	void update_page_overlay();

protected:
	// QT event handling
	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void mouseDoubleClickEvent(QMouseEvent * event);
	void resizeEvent(QResizeEvent *event);

signals:
	/**
	 * Emitted when the user requests to see the source code of a
	 * particular point on a particular page.
	 */
	void synchronize_editor(int page, int x, int y);

private slots:
	void page_rendered(int page);
	void goto_page();

	// primitive actions
	void set_presentation_layout();
	void set_grid_layout();
	void set_presenter_layout();

	void toggle_overlay();
	void focus_goto();

	void disable_triple_click();

	void swap_selection_and_panning_buttons();

private:
	void setup_keys(QWidget *base);

	Viewer *viewer;
	Layout *cur_layout;
	PresentationLayout *presentation_layout;
	GridLayout *grid_layout;
	PresenterLayout *presenter_layout;

	GotoLine *goto_line;
	QLabel *page_overlay;

	int mx, my;
	int mx_down, my_down;
	bool triple_click_possible;
	QTimer scroll_timer;

	bool valid;

	// config options
	QColor background;
	QColor background_fullscreen;
	int mouse_wheel_factor;

	Qt::MouseButton click_link_button;
	Qt::MouseButton drag_view_button;
	Qt::MouseButton select_text_button;
};

#endif

