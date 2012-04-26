#include "viewer.h"


Viewer::Viewer(ResourceManager *res, QWidget *parent) :
		QWidget(parent),
		valid(true) {
	canvas = new Canvas(res, this);
	if (!canvas->is_valid()) {
		valid = false;
		search_bar = NULL;
		layout = NULL;
		return;
	}

	search_bar = new QLineEdit(this);
	// TODO these sequences conflict between widgets
	add_sequence("F", &Viewer::toggle_fullscreen);
	add_sequence("Esc", &Viewer::close_search);

	layout = new QVBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(canvas);
	layout->addWidget(search_bar);
	this->setLayout(layout);

	this->setMinimumSize(50, 50);
	this->resize(500, 500);
	this->show();
	search_bar->hide();
}

Viewer::~Viewer() {
	delete layout;
	delete search_bar;
	delete canvas;
}

bool Viewer::is_valid() const {
	return valid;
}

void Viewer::focus_search() {
	search_bar->setFocus(Qt::OtherFocusReason);
	search_bar->selectAll();
	search_bar->show();
}

void Viewer::toggle_fullscreen() {
	setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void Viewer::close_search() {
	canvas->setFocus(Qt::OtherFocusReason);
	search_bar->hide();
}

bool Viewer::event(QEvent *event) {
	if (event->type() == QEvent::Shortcut) {
		QShortcutEvent *s = static_cast<QShortcutEvent*>(event);
		if (sequences.find(s->key()) != sequences.end()) {
			(this->*sequences[s->key()])();
		}
		return true;
	}
	return QWidget::event(event);
}

void Viewer::add_sequence(QString key, func_t action) {
	QKeySequence s(key);
	sequences[s] = action;
	grabShortcut(s, Qt::WindowShortcut);
}
