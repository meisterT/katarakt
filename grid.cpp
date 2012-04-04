#include "grid.h"
//#include <cstring>
#include <iostream>

using namespace std;


Grid::Grid(ResourceManager *_res, int columns) :
		res(_res),
		column_count(-1),
		width(NULL), height(NULL) {
	set_columns(columns);
}

Grid::~Grid() {
	delete width;
	delete height;
}

void Grid::set_columns(int columns) {
	column_count = columns;
	if (column_count < 1) {
		column_count = 1;
	}

	rebuild_cells();
}

float Grid::get_width(int page) const {
	// TODO implement first page offset
	// bounds check
	if (page < 0 || page >= res->get_page_count()) {
		return -1;
	}

	return width[page % column_count];
}

float Grid::get_height(int page) const {
	// bounds check
	if (page < 0 || page >= res->get_page_count()) {
		return -1;
	}

	return height[page / column_count];
}

int Grid::get_column_count() const {
	return column_count;
}

void Grid::rebuild_cells() {
	delete width;
	delete height;

	// implicit ceil
	int row_count = (res->get_page_count() + column_count - 1) / column_count;

	width = new float[column_count];
	height = new float[row_count];
	// this breaks
//	memset(width, 0, sizeof(width));
//	memset(height, 0, sizeof(height));

	// TODO better solution?
	for (int i = 0; i < column_count; i++) {
		width[i] = -1.0f;
	}
	for (int i = 0; i < row_count; i++) {
		height[i] = -1.0f;
	}

	for (int i = 0; i < res->get_page_count(); i++) {
		int col = (i % column_count);
		int row = (i / column_count);

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

