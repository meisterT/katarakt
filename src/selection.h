#ifndef SELECTIONPART_H
#define SELECTIONPART_H

#include <poppler/qt4/poppler-qt4.h>
#include <QRectF>


namespace Selection {
	enum Mode {
		Start,
		StartWord,
		StartLine,
		End
	};
}


class SelectionPart {
public:
	SelectionPart(Poppler::TextBox *box);
	~SelectionPart();

	void add_word(Poppler::TextBox *box);
	Poppler::TextBox *get_text() const;
	QRectF get_bbox() const;

private:
	Poppler::TextBox *text_box;
	QRectF bbox;
};

bool selection_less_y(const SelectionPart *a, const SelectionPart *b);


class SelectionLine {
public:
	SelectionLine(SelectionPart *part);
	~SelectionLine();

	void add_part(SelectionPart *part);
	const QList<SelectionPart *> &get_parts() const;
	QRectF get_bbox() const;

	void sort();

private:
	QList<SelectionPart *> parts;
	QRectF bbox;
};

bool selection_less_x(const SelectionPart *a, const SelectionPart *b);


class Cursor {
public:
	int page;
	QPointF click;
	int line;
	int part;
	int word;
	int character;
	bool inclusive;
	float x;
	SelectionLine *selectionline;

private:
	void find_part(bool from, enum Selection::Mode mode);
	void find_word(const Poppler::TextBox *text_box, bool from, enum Selection::Mode mode);
	void find_character(const Poppler::TextBox *text, bool from);

	void set_beginning_of_line(const SelectionLine *line, bool from);
	void set_end_of_line(const SelectionLine *line, bool from);

	void increment();
	void decrement();

	friend class MouseSelection;
};


class MouseSelection {
public:
	MouseSelection();

	void set_cursor(const QList<SelectionLine *> *lines, std::pair<int, QPointF> pos, enum Selection::Mode mode);
	Cursor get_cursor(bool from) const;
	QString get_selection_text(int page, const QList<SelectionLine *> *lines) const;

	void deactivate();
	bool is_active() const;

private:
	int bsearch(const QList<SelectionLine *> *lines, float value);
	void update_reversed(Cursor &c, const SelectionLine *line) ;
	bool calculate_reversed(Cursor &c, const SelectionLine *line) const;

	Cursor cursor[2];

	bool active;
	bool reversed;
	enum Selection::Mode mode;
};


#endif

