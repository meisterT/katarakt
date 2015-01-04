#ifndef BEAMERWINDOW_H
#define BEAMERWINDOW_H

#include <QWidget>


class Viewer;
class Layout;

// TODO make subclass of Canvas?
class BeamerWindow : public QWidget {
	Q_OBJECT

public:
	BeamerWindow(Viewer *v, QWidget *parent = 0);
	~BeamerWindow();

	bool is_valid() const;

	Layout *get_layout() const;
	void set_page(int page);

public slots:
	void toggle_fullscreen();

protected:
	// QT event handling
	void paintEvent(QPaintEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);
	void resizeEvent(QResizeEvent *event);

private slots:
	void page_rendered(int page);

private:
	Viewer *viewer;
	Layout *layout;

	int mx_down, my_down;

	int mouse_wheel_factor;

	bool valid;
};

#endif

