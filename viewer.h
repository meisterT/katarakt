#ifndef VIEWER_H
#define VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QKeySequence>
#include <QSocketNotifier>
#include <map>


class ResourceManager;
class Canvas;


class Viewer : public QWidget {
	Q_OBJECT

	typedef void (Viewer::*func_t)();

public:
//	Viewer(ResourceManager *res, QWidget *parent = 0);
	Viewer(QString _file, QWidget *parent = 0);
	~Viewer();

	bool is_valid() const;
	void focus_search();

	void reload();

	ResourceManager *get_res() const;
	Canvas *get_canvas() const;

public slots:
	void signal_slot();

protected:
	bool event(QEvent *event);

private:
	void toggle_fullscreen();
	void close_search();
	void add_sequence(QString key, func_t action);

	QString file;
	ResourceManager *res;
	Canvas *canvas;
	QLineEdit *search_bar;
	QVBoxLayout *layout;

	// signal handling
	static void signal_handler(int unused);
	static int sig_fd[2];
	QSocketNotifier *sig_notifier;

	// key sequences
	std::map<QKeySequence,func_t> sequences;
	bool valid;
};

#endif

