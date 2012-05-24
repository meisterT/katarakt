#ifndef SEARCH_H
#define SEARCH_H

#include <poppler/qt4/poppler-qt4.h>
#include <QString>
#include <QThread>
#include <QMutex>
#include <QLineEdit>
#include <QRect>
#include <list>


class SearchBar;


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
	SearchBar(QString file, QWidget *parent = 0);
	~SearchBar();

	bool is_valid() const;

signals:
	void search_clear();
	void search_done(int page, std::list<Result> *hits);

private slots:
	void set_text();

private:
	void join_threads();

	Poppler::Document *doc;

	QMutex search_mutex;
	QMutex term_mutex;
	SearchWorker *worker;
	QString term;

	friend class SearchWorker;
};

#endif

