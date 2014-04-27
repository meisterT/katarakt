#ifndef VIEWER_H
#define VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QSocketNotifier>
#include <QLabel>
#include <QLineEdit>


class ResourceManager;
class Canvas;
class SearchBar;


class Viewer : public QWidget {
	Q_OBJECT

public:
	Viewer(QString _file, QWidget *parent = 0);
	~Viewer();

	bool is_valid() const;
	void focus_search();

	ResourceManager *get_res() const;
	Canvas *get_canvas() const;

public slots:
	void signal_slot();
	void inotify_slot();

	void toggle_fullscreen();
	void close_search();
	void reload();
	void open();

private:
	void update_info_widget();
	void add_action(const char *action, const char *slot);

	QString file;
	ResourceManager *res;
	Canvas *canvas;
	SearchBar *search_bar;
	QVBoxLayout *layout;

	// info bar
	QWidget info_widget;
	QHBoxLayout info_layout;
	QLabel info_label_icon;
	QLabel info_label_text;
	QLineEdit info_password;

	// signal handling
	static void signal_handler(int unused);
	static int sig_fd[2];
	QSocketNotifier *sig_notifier;

#ifdef __linux__
	int inotify_fd;
	int inotify_wd;
	QSocketNotifier *i_notifier;
#endif

	bool valid;
};

#endif

