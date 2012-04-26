#ifndef VIEWER_H
#define VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QKeySequence>

#include "canvas.h"


class ResourceManager;

class Viewer : public QWidget {
	Q_OBJECT

	typedef void (Viewer::*func_t)();

public:
	Viewer(ResourceManager *res, QWidget *parent = 0);
	~Viewer();

	bool is_valid() const;
	void focus_search();

protected:
	bool event(QEvent *event);

private:
	void toggle_fullscreen();
	void close_search();
	void add_sequence(QString key, func_t action);

	QVBoxLayout *layout;
	QLineEdit *search_bar;
	Canvas *canvas;

	// key sequences
	std::map<QKeySequence,func_t> sequences;
	bool valid;
};

#endif

