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

public slots:
	void signal_slot(); // reloads on SIGUSR1

	void toggle_fullscreen();
	void close_search();
	void reload(bool clamp = true);
	void open(); // ask user for filename
	void open(QString filename);
	void save();
	void jump_back();
	void jump_forward();
	void mark_jump();

private slots:
	void page_up();
	void page_down();
	void page_first();
	void page_last();
	void half_screen_up();
	void half_screen_down();
	void screen_up();
	void screen_down();
	void smooth_up();
	void smooth_down();
	void smooth_left();
	void smooth_right();
	void zoom_in();
	void zoom_out();
	void reset_zoom();
	void columns_inc();
	void columns_dec();
	void offset_inc();
	void offset_dec();
	void quit();
	void search();
	void search_backward();
	void next_hit();
	void previous_hit();
	void next_invisible_hit();
	void previous_invisible_hit();
	void rotate_left();
	void rotate_right();
	void invert_colors();
	void copy_to_clipboard();
	void toggle_toc();

private:
	void update_info_widget();
	void setup_keys(QWidget *base);

	ResourceManager *res;
	Splitter *splitter;
	Toc *toc;
	Canvas *canvas;
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

