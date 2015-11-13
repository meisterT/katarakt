#ifndef VIEWER_H
#define VIEWER_H

#include <QWidget>
#include <QVBoxLayout>
#include <QKeySequence>
#include <QSocketNotifier>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>


class ResourceManager;
class Canvas;
class SearchBar;
class BeamerWindow;
class Splitter;
class Toc;


class Viewer : public QWidget {
	Q_OBJECT

public:
	Viewer(const QString &file, QWidget *parent = 0);
	~Viewer();

	bool is_valid() const;

	ResourceManager *get_res() const;
	Canvas *get_canvas() const;
	SearchBar *get_search_bar() const;
	BeamerWindow *get_beamer() const;

	void layout_updated(int new_page, bool page_changed);
	void show_progress(bool show);

public slots:
	void signal_slot(); // reloads on SIGUSR1

	void reload(bool clamp = true);
	void open(QString filename);

private slots:
	// movement
	void page_up();
	void page_down();
	void top();
	void bottom();
	void half_screen_up();
	void half_screen_down();
	void screen_up();
	void screen_down();
	void smooth_up();
	void smooth_down();
	void smooth_left();
	void smooth_right();
	void next_hit();
	void previous_hit();
	void next_invisible_hit();
	void previous_invisible_hit();
	void jump_back();
	void jump_forward();
	// layout
	void zoom_in();
	void zoom_out();
	void reset_zoom();
	void increase_columns();
	void decrease_columns();
	void increase_offset();
	void decrease_offset();
	void rotate_left();
	void rotate_right();
	// viewer
	void quit();
	void search();
	void search_backward();
	void close_search();
	void mark_jump();
	void invert_colors();
	void copy_to_clipboard();
	void toggle_fullscreen();
	void open(); // ask user for filename
	void print();
	void save();
	void toggle_toc();

private:
	void update_info_widget();
	void setup_keys(QWidget *base);

	ResourceManager *res;
	Splitter *splitter;
	Toc *toc;
	Canvas *canvas;
	QProgressBar presenter_progress;
	SearchBar *search_bar;
	QVBoxLayout *layout;

	// config options
	int smooth_scroll_delta;
	float screen_scroll_factor;

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

	BeamerWindow *beamer;

	bool valid;
};

#endif

