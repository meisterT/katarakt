#ifndef VIEWER_H
#define VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QSocketNotifier>
#include <map>


class ResourceManager;
class Canvas;
class SearchBar;


class Viewer : public QWidget {
	Q_OBJECT

	typedef void (Viewer::*func_t)();

public:
	Viewer(QString _file, int start_page, bool fullscreen, QWidget *parent = 0);
	~Viewer();

	bool is_valid() const;
	void focus_search();

	void reload();

	ResourceManager *get_res() const;
	Canvas *get_canvas() const;

public slots:
	void signal_slot();
	void inotify_slot();

protected:
	bool event(QEvent *event);

private:
	void toggle_fullscreen();
	void close_search();
	void add_sequence(QString key, func_t action);

	QString file;
	ResourceManager *res;
	Canvas *canvas;
	SearchBar *search_bar;
	QVBoxLayout *layout;

	// signal handling
	static void signal_handler(int unused);
	static int sig_fd[2];
	QSocketNotifier *sig_notifier;

#ifdef __linux__
	int inotify_fd;
	int inotify_wd;
	QSocketNotifier *i_notifier;
#endif

	// key sequences
	std::map<QKeySequence,func_t> sequences;
	bool valid;
};

#endif

