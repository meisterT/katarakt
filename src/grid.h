#ifndef GRID_H
#define GRID_H


class ResourceManager;


class Grid {
public:
	Grid(ResourceManager *_res, int columns, int offset);
	~Grid();

	bool set_columns(int columns);
	bool set_offset(int offset);

	float get_width(int col) const;
	float get_height(int row) const;
	int get_column_count() const;
	int get_row_count() const;
	int get_offset() const;

private:
	void rebuild_cells();

	ResourceManager *res;

	int column_count;
	int row_count;
	float *width;
	float *height;
	int page_offset;
};

#endif

