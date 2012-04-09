#ifndef GRID_H
#define GRID_H

#include "resourcemanager.h"


class Grid {
public:
	Grid(ResourceManager *_res, int columns);
	~Grid();

	void set_columns(int columns);

	float get_width(int page) const;
	float get_height(int page) const;
	int get_column_count() const;
	int get_row_count() const;

private:
	void rebuild_cells();

	ResourceManager *res;

	int column_count;
	int row_count;
	float *width;
	float *height;
};

#endif

