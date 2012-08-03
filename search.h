#ifndef SEARCH_H
#define SEARCH_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QLineEdit>
#include <QRect>
#include <QEvent>
#include <list>


class SearchBar;
class Canvas;
class Viewer;


class Result {
public:
	Result(double _x1 = 0.0, double _y1 = 0.0, double _x2 = 0.0, double _y2 = 0.0);
	QRect scale_translate(double factor, double off_x = 0.0, double off_y = 0.0);

	double x1, y1, x2, y2;
};


class SearchWorker : public QThread {
	Q_OBJECT

public:
	SearchWorker(SearchBar *_bar);
	void run();

	volatile bool stop;
	volatile bool die;

private:
	SearchBar *bar;
};


class SearchBar : public QLineEdit {
	Q_OBJECT

public:
	SearchBar(QString file, Viewer *v, QWidget *parent = 0);
	~SearchBar();

	void load(QString file);
	bool is_valid() const;
	void connect_canvas(Canvas *c) const;

signals:
	void search_clear();
	void search_done(int page, std::list<Result> *hits);
	void search_visible(bool visible);

protected:
	// QT event handling
	bool event(QEvent *event);

private slots:
	void set_text();

private:
	void initialize(QString file);
	void join_threads();
	void shutdown();

	Poppler::Document *doc;
	Viewer *viewer;

	QMutex search_mutex;
	QMutex term_mutex;
	SearchWorker *worker;
	QString term;
	int start_page;

	friend class SearchWorker;
};

#endif

