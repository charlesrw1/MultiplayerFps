#pragma once
#include <vector>
using std::vector;
struct Map_Square
{
	int floor_height;
	int ceiling_height;
	short wall_texture;
	short floor_texture;
	short ceiling_texture;

	char floor_offsets[4];
	char ceiling_offsets[4];

	enum {
		SPACE,
		CORNER,
		WALL,
	};
};

struct Map_Square_Chunk
{
	int x, y;
	Map_Square squares[1024];	// 32x32
};

class Map_Edit
{
	vector<Map_Square_Chunk> chunks;
};