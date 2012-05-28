#include "gotoline.h"

GotoLine::GotoLine(int page_count, QWidget *parent) :
		QLineEdit(parent) {
	v = new QIntValidator(1, page_count);
	setValidator(v);
}

void GotoLine::set_page_count(int page_count) {
	v->setTop(page_count);
}

void GotoLine::focusOutEvent(QFocusEvent * /*event*/) {
	hide();
}

