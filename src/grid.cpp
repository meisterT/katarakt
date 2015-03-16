#include "grid.h"
#include "resourcemanager.h"
#include <iostream>

using namespace std;


Grid::Grid(ResourceManager *_res, int columns, int offset) :
		res(_res),
		column_count(-1),
		width(NULL), height(NULL),
		page_offset(offset) {
	set_columns(columns);
}

Grid::~Grid() {
	delete[] width;
	delete[] height;
}

bool Grid::set_columns(int columns) {
	int old_column_count = column_count;
	column_count = columns;
	if (column_count > res->get_page_count()) {
		column_count = res->get_page_count();
	}
	if (column_count < 1) {
		column_count = 1;
	}

	set_offset(page_offset);

	return old_column_count != column_count;
}

bool Grid::set_offset(int offset) {
	int old_page_offset = page_offset;
	page_offset = offset;
	if (page_offset < 0) {
		page_offset = 0;
	}
	if (page_offset > column_count - 1) {
		page_offset = column_count - 1;
	}

	rebuild_cells();
	return old_page_offset != page_offset;
}

float Grid::get_width(int col) const {
	// bounds check
	if (col < 0 || col >= column_count) {
		return -1;
	}

	return width[col];
}

float Grid::get_height(int row) const {
	// bounds check
	if (row < 0 || row >= row_count) {
		return -1;
	}

	return height[row];
}

int Grid::get_column_count() const {
	return column_count;
}

int Grid::get_row_count() const {
	return row_count;
}

int Grid::get_offset() const {
	return page_offset;
}

void Grid::rebuild_cells() {
	delete[] width;
	delete[] height;

	// implicit ceil
	row_count = (res->get_page_count() + column_count - 1 + page_offset) / column_count;

	width = new float[column_count];
	height = new float[row_count];

	for (int i = 0; i < column_count; i++) {
		width[i] = -1.0f;
	}
	for (int i = 0; i < row_count; i++) {
		height[i] = -1.0f;
	}

	for (int i = 0; i < res->get_page_count(); i++) {
		int col = ((i + page_offset) % column_count);
		int row = ((i + page_offset) / column_count);

		// calculate column width
		float new_width = res->get_page_width(i);
		if (width[col] < 0 || width[col] < new_width) {
			width[col] = new_width;
		}

		// calculate row height
		float new_height = res->get_page_height(i);
		if (height[row] < 0 || height[row] < new_height) {
			height[row] = new_height;
		}
	}
}

