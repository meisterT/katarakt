#include "selection.h"

using namespace std;


SelectionPart::SelectionPart(Poppler::TextBox *box) :
		text_box(box) {
	bbox = text_box->boundingBox();
}

SelectionPart::~SelectionPart() {
	Poppler::TextBox *next;
	do {
		next = text_box->nextWord();
		delete text_box;
		text_box = next;
	} while (next != NULL);
}

void SelectionPart::add_word(Poppler::TextBox *box) {
	bbox = bbox.united(box->boundingBox());
}

Poppler::TextBox *SelectionPart::get_text() const {
	return text_box;
}

QRectF SelectionPart::get_bbox() const {
	return bbox;
}

bool selection_less_y(const SelectionPart *a, const SelectionPart *b) {
	return a->get_bbox().center().y() < b->get_bbox().center().y();
}


SelectionLine::SelectionLine(SelectionPart *part) {
	parts.push_back(part);
	bbox = part->get_bbox();
}

SelectionLine::~SelectionLine() {
	Q_FOREACH(SelectionPart *p, parts) {
		delete p;
	}
}

void SelectionLine::add_part(SelectionPart *part) {
	parts.push_back(part);
	bbox = bbox.united(part->get_bbox());
}

const QList<SelectionPart *> &SelectionLine::get_parts() const {
	return parts;
}

QRectF SelectionLine::get_bbox() const {
	return bbox;
}

void SelectionLine::sort() {
	qStableSort(parts.begin(), parts.end(), selection_less_x);
}

bool selection_less_x(const SelectionPart *a, const SelectionPart *b) {
	return a->get_bbox().center().x() < b->get_bbox().center().x();
}


void Cursor::find_part(bool from, enum Selection::Mode mode) {
	const QList<SelectionPart *> parts = selectionline->get_parts();
	// select beginning/end of line when gap between lines is big enough
	if (selectionline->get_bbox().top() - click.y() > selectionline->get_bbox().height()) {
		set_beginning_of_line(selectionline, from);
		return;
	}
	if (click.y() - selectionline->get_bbox().bottom() > selectionline->get_bbox().height()) {
		set_end_of_line(selectionline, from);
		return;
	}

	if (mode == Selection::StartLine) {
		if (from) {
			set_beginning_of_line(selectionline, from);
		} else {
			set_end_of_line(selectionline, from);
		}
		return;
	}

	if (from) { // selection grows to the left
		for (part = 0; part < parts.size(); part++) {
			if (click.x() <= parts.at(part)->get_bbox().right()) {
				break;
			}
		}
		if (part >= parts.size()) {
			part = parts.size() - 1;
		}
	} else { // selection grows to the right
		for (part = parts.size() - 1; part >= 0; part--) {
			if (click.x() >= parts.at(part)->get_bbox().left()) {
				break;
			}
		}
		if (part < 0) {
			part = 0;
		}
	}
	find_word(parts.at(part)->get_text(), from, mode);
}

void Cursor::find_word(const Poppler::TextBox *text_box, bool from, enum Selection::Mode mode) {
	word = 0;
	const Poppler::TextBox *next = text_box;
	if (from) {
		while (true) {
			if (click.x() <= next->boundingBox().right()) {
				break;
			}

			if (next->nextWord() == NULL) {
				break;
			}
			next = next->nextWord();
			word++;
		}
	} else {
		const Poppler::TextBox *prev = next;
		while (next != NULL) {
			if (click.x() < next->boundingBox().left()) {
				if (word > 0) {
					word--;
					next = prev;
				}
				break;
			}

			if (next->nextWord() == NULL) {
				break;
			}
			prev = next;
			next = next->nextWord();
			word++;
		}
	}
	if (mode == Selection::Start) {
		find_character(next, from);
	} else if (mode == Selection::StartWord) {
		if (from) {
			character = 0;
			inclusive = true;
			x = next->charBoundingBox(0).left();
		} else {
			character = next->text().size() - 1;
			inclusive = true;
			x = next->charBoundingBox(character).right();
		}
	}
}

void Cursor::find_character(const Poppler::TextBox *text, bool from) {
	if (from) { // selection grows to the left
		for (character = 0; character < text->text().size(); character++) {
			if (click.x() <= text->charBoundingBox(character).right()) {
				x = text->charBoundingBox(character).left();
				inclusive = true;
				break;
			}
		}
		if (character >= text->text().size()) {
			character = text->text().size() - 1;
			x = text->charBoundingBox(character).right();
			inclusive = false;
		}
	} else { // selection grows to the right
		for (character = text->text().size() - 1; character >= 0; character--) {
			if (click.x() >= text->charBoundingBox(character).left()) {
				x = text->charBoundingBox(character).right();
				inclusive = true;
				break;
			}
		}
		if (character < 0) {
			character = 0;
			x = text->charBoundingBox(character).left();
			inclusive = false;
		}
	}
}

void Cursor::set_beginning_of_line(const SelectionLine *line, bool from) {
	part = 0;
	word = 0;
	character = 0;
	inclusive = from;
	x = line->get_parts().at(0)->get_text()->charBoundingBox(0).left();
}

void Cursor::set_end_of_line(const SelectionLine *line, bool from) {
	part = line->get_parts().size() - 1;
	Poppler::TextBox *text_box = line->get_parts().at(part)->get_text();
	word = 0;
	while (text_box->nextWord() != NULL) {
		text_box = text_box->nextWord();
		word++;
	}
	character = text_box->text().size() - 1;
	inclusive = !from;
	x = text_box->charBoundingBox(character).right();
}

void Cursor::increment() {
	Poppler::TextBox *text_box = selectionline->get_parts().at(part)->get_text();
	for (int i = 0; i < word; i++) {
		text_box = text_box->nextWord();
	}

	if (!inclusive) {
		inclusive = true;
		x = text_box->charBoundingBox(character).left();
		return;
	}

	character++;
	if (character >= text_box->text().size()) {
		if (text_box->nextWord() == NULL) {
			part++;
			if (part >= selectionline->get_parts().size()) {
				part--;
				character--;
				inclusive = false;
				x = text_box->charBoundingBox(character).right();
				return;
			}
			word = 0;
			character = 0;
			inclusive = true;
			x = selectionline->get_parts().at(part)->get_text()->charBoundingBox(0).left();
			return;
		}
		text_box = text_box->nextWord();
		word++;
		character = 0;
		inclusive = true;
		x = text_box->charBoundingBox(0).left();
		return;
	}
	x = text_box->charBoundingBox(character).left();
}

void Cursor::decrement() {
	Poppler::TextBox *text_box = selectionline->get_parts().at(part)->get_text();
	Poppler::TextBox *prev = text_box;
	for (int i = 0; i < word; i++) {
		prev = text_box;
		text_box = text_box->nextWord();
	}

	if (!inclusive) {
		inclusive = true;
		x = text_box->charBoundingBox(character).right();
		return;
	}

	character--;
	if (character < 0) {
		if (word == 0) {
			if (part == 0) {
				character = 0;
				inclusive = false;
				x = text_box->charBoundingBox(0).left();
				return;
			}
			part--;
			text_box = selectionline->get_parts().at(part)->get_text();
			word = 0;
			while (text_box->nextWord() != NULL) {
				text_box = text_box->nextWord();
				word++;
			}
			character = text_box->text().size() - 1;
			inclusive = true;
			x = text_box->charBoundingBox(character).right();
			return;
		}
		word--;
		character = prev->text().size() - 1;
		inclusive = true;
		x = prev->charBoundingBox(character).right();
		return;
	}
	x = text_box->charBoundingBox(character).right();
}


MouseSelection::MouseSelection() :
		active(false) {
}

void MouseSelection::set_cursor(const QList<SelectionLine *> *lines,
		pair<int, QPointF> pos, enum Selection::Mode _mode) {
	// first = true: first cursor that was created (beginning of selection)
	// from = true: cursor that comes first (from the top left)
	bool first = _mode != Selection::End;
	if (first) {
		mode = _mode;
	}

	if (first) {
		active = false;
		reversed = false;
	} else {
		active = true;
	}

	Cursor &c = cursor[first ? 0 : 1];
	c.page = pos.first;
	c.click = pos.second;

	if (lines == NULL || lines->empty()) {
		return;
	}

	c.line = bsearch(lines, c.click.y());
	if (c.line < lines->size() - 1) {
		// the click lies between line and line + 1
		float prev = lines->at(c.line)->get_bbox().center().y();
		float next = lines->at(c.line + 1)->get_bbox().center().y();

		if (first) {
			// beginning of selection -> set to closest line
			if (c.click.y() - prev > next - c.click.y()) {
				c.line++;
			}
		} else {
			// continued selection
			// only set to closest line if the beginning is on that line
			// or if the current and next box overlap
			// else the selection direction has to be considered
			bool line_added = false;
			if (c.click.y() - prev > next - c.click.y()) {
				if (c.line + 1 == cursor[0].line ||
						lines->at(c.line)->get_bbox().bottom() > lines->at(c.line + 1)->get_bbox().top()) {
					c.line++;
					line_added = true;
				}
			}
			update_reversed(c, lines->at(c.line));

			// inside actual box?
			if (!line_added) {
				if (lines->at(c.line)->get_bbox().bottom() <= lines->at(c.line + 1)->get_bbox().top()) {
					if (reversed && c.line < cursor[0].line) {
						if (c.click.y() > lines->at(c.line)->get_bbox().bottom()) {
							c.line++;
						}
					} else {
						if (c.click.y() >= lines->at(c.line + 1)->get_bbox().top()) {
							c.line++;
						}
					}
				}
			}

			update_reversed(c, lines->at(c.line));
		}
	} else {
		// last line
		if (!first) {
			update_reversed(c, lines->at(c.line));
		}
	}

	// find closest part/word/character
	c.selectionline = lines->at(c.line);
	c.find_part(first ^ reversed, mode);

	// word or line selection -> set second cursor right away
	if (first && (mode == Selection::StartWord || mode == Selection::StartLine)) {
		cursor[1] = c;
		cursor[1].find_part(!first ^ reversed, mode);
		active = true;
	}
}

Cursor MouseSelection::get_cursor(bool from) const {
	if (from ^ reversed) {
		return cursor[0];
	} else {
		return cursor[1];
	}
}

QString MouseSelection::get_selection_text(int page, const QList<SelectionLine *> *lines) const {
	QString text;
	if (lines != NULL && lines->size() != 0 && is_active()) {
		Cursor from = get_cursor(true);
		Cursor to = get_cursor(false);
		if (from.page <= page && to.page >= page) {
			if (from.page < page) {
				from.line = 0;
				from.set_beginning_of_line(lines->at(from.line), true);
			}
			if (to.page > page) {
				to.line = lines->size() - 1;
				to.set_end_of_line(lines->at(to.line), false);
			}

			bool add_space = false;

			for (int line = from.line; line <= to.line; line++) {
				float last_x = 0;
				int last_x_index = -2;
				const QList<SelectionPart *> p = lines->at(line)->get_parts();

				for (int part = from.part; part < p.size(); part++) {
					if (to.line == line && to.part < part) {
						break;
					}
					int word = 0;
					Poppler::TextBox *box = p.at(part)->get_text();
					while (box != NULL) {
						if (to.line == line && to.part == part && to.word < word) {
							break;
						}

						if ((from.part == part && word >= from.word) || part > from.part) {
							QString tmp = box->text();
							if (to.line == line && to.part == part && to.word == word) {
								tmp.truncate(to.character + 1);
								if (!to.inclusive) {
									tmp.chop(1);
								}
							}
							if (from.line == line && from.part == part && from.word == word) {
								tmp.remove(0, from.character);
								if (!from.inclusive) {
									tmp.remove(0, 1);
								}
							}

							if (add_space) {
								text += " ";
								add_space = false;
							}
							// big gap in front of current box, add <tab>
							if (word == 0 && part == last_x_index + 1) {
								if (box->boundingBox().left() - last_x > box->boundingBox().height()) {
									text += "\t";
								}
							}

							text += tmp;
							if (box->hasSpaceAfter()) {
								add_space = true;
							}
						}

						last_x = box->boundingBox().right();
						last_x_index = part;
						box = box->nextWord();
						word++;
					}
				}

				if (line + 1 < lines->size()) {
					from.set_beginning_of_line(lines->at(line + 1), true);
					if (line < to.line) {
						text += "\n";
					}
				}
			}
		}
	}
	return text;
}

void MouseSelection::deactivate() {
	active = false;
}

bool MouseSelection::is_active() const {
	return active;
}

int MouseSelection::bsearch(const QList<SelectionLine *> *lines, float value) {
	int from = 0, to = lines->size();
	int mid;

	if (to == 0) {
		return 0;
	}

	if (value <= lines->at(0)->get_bbox().center().y()) {
		return 0;
	} else if (value > lines->at(to - 1)->get_bbox().center().y()) {
		return to - 1;
	}

	while (to - from > 1) {
		mid = (from + to) / 2;
		float cur = lines->at(mid)->get_bbox().center().y();
		if (cur == value) {
			return mid;
		} else if (cur < value) {
			from = mid;
		} else {
			to = mid;
		}
	}

	return from;
}

void MouseSelection::update_reversed(Cursor &c, const SelectionLine *line) {
	if (reversed != calculate_reversed(c, line)) {
		// flip reversed flag, adjust first cursor's character
		if (reversed) {
			cursor[0].increment();
		} else {
			cursor[0].decrement();
		}
		reversed = !reversed;
	}
}

bool MouseSelection::calculate_reversed(Cursor &c, const SelectionLine *line) const {
	if (c.page < cursor[0].page) {
		return true;
	} else if (c.page > cursor[0].page) {
		return false;
	}
	// same page
	if (c.line < cursor[0].line) {
		return true;
	} else if (c.line > cursor[0].line) {
		return false;
	}
	// same line
	if (line->get_bbox().top() - c.click.y() > line->get_bbox().height()) {
		return true;
	}
	if (c.click.y() - line->get_bbox().bottom() > line->get_bbox().height()) {
		return false;
	}
	if (c.click.x() < cursor[0].x) {
		return true;
	} else {
		return false;
	}
}

