#ifndef GOTOLINE_H
#define GOTOLINE_H

#include <QLineEdit>
#include <QIntValidator>
#include <QFocusEvent>


class GotoLine : public QLineEdit {
	Q_OBJECT

public:
	GotoLine(int page_count, QWidget *parent = 0);
	~GotoLine() {delete v;};

	void set_page_count(int page_count);

protected:
	void focusOutEvent(QFocusEvent *event);

private:
	QIntValidator *v;
};

#endif

