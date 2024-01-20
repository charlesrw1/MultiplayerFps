#pragma once
#include "SDL2/SDL.h"
#include "Types.h"
#include "BVH.h"
#include "MeshBuilder.h"
#include "GlmInclude.h"
#include <vector>
using std::vector;


// represents a "column" grid square
struct Map_Column
{
	int floor_height = 0;
	int ceiling_height = 5;
	short wall_texture;
	short floor_texture;
	short ceiling_texture;

	char floor_offsets[4];
	char ceiling_offsets[4];

	enum {
		SPACE,
		CORNER,
		WALL,
	} type = SPACE;
};

struct Map_Chunk
{
	static const int GRID_WIDTH = 32;
	Map_Chunk(int x, int y) : x(x), y(y) {
		grid.resize(GRID_WIDTH * GRID_WIDTH);
	}

	int x, y;
	vector<Map_Column> grid;

	Map_Column& get(int x, int y) {
		return grid.at(y * GRID_WIDTH + x);
	}
	void append_quad(vec3 v1, vec3 v2, vec3 v3, vec3 v4);
};

struct Map_Column_Index
{
	int chunk_index = -1;
	int x = 0;
	int y = 0;
};

class Map_Edit
{
public:
	void key_input(const SDL_Event& event);
	void update();
	void draw();
	Map_Column_Index trace_squares(vec3 origin, vec3 direction);
	vec3 ray_pick(int x, int y);
	void rebuild_map_mesh();
	void append_column(Map_Chunk& c, int x, int y);


	MeshBuilder mesh;
	vector<Map_Column_Index> triangle_id;	// indicies/3

	vector<Map_Chunk> chunks;
	Fly_Camera camera;
	View_Setup vs;
};