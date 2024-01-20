#include "Edit.h"
#include "Game_Engine.h"

// controls: selecting squares
// 1,2,3,4 selects grid size
// if click, ray cast squares to find selected one
// if click and hold, then ray cast again to make square selection on ground
// if shift click and something already selected, make square selection

// with squares selected:
// move squares up and down, move edges up and down
// convert square to corner or wall
// texture wall, ceiling or floor


const float COL_WIDTH = 0.5f;

vec3 Map_Edit::ray_pick(int x, int y)
{
	float x1 = ((float)x / vs.width) * 2.f - 1.0;
	float y1 = ((float)y / vs.height) * 2.f - 1.0;
	vec4 ndc(x1, y1, 0, 1);
	mat4 inv_view_proj = glm::inverse(vs.viewproj);
	vec4 worldspace = inv_view_proj * ndc;
	worldspace /= worldspace.w;
	return normalize(vec3(worldspace) - camera.position);
}

void Map_Edit::append_column(Map_Chunk& c, int x, int y)
{
	// things to do:
	// the walls or sides of floors/ceilings of columns that are neighboring
	// the ceiling of this column
	
	Map_Column& co = c.get(x, y);
	if (co.type == Map_Column::WALL) return;
	// the floor of the column
	vec3 floor_corners[4];
	floor_corners[0] = vec3(c.x + x, co.floor_height, c.y + y);
	floor_corners[1] = vec3(c.x + x + COL_WIDTH, co.floor_height, c.y + y);
	floor_corners[2] = vec3(c.x + x + COL_WIDTH, co.floor_height, c.y + y + COL_WIDTH);
	floor_corners[3] = vec3(c.x + x, co.floor_height, c.y + y + COL_WIDTH);
	int floor_base = mesh.GetBaseVertex();
	for (int i = 0; i < 4; i++) mesh.AddVertex(MbVertex(floor_corners[i], COLOR_WHITE));
	mesh.AddQuad(floor_base, floor_base + 1, floor_base + 2, floor_base + 3);
	// ceiling column
	vec3 ceiling_corners[4];
	ceiling_corners[0] = vec3(c.x + x, co.ceiling_height, c.y + y);
	ceiling_corners[1] = vec3(c.x + x + COL_WIDTH, co.ceiling_height, c.y + y);
	ceiling_corners[3] = vec3(c.x + x, co.ceiling_height, c.y + y + COL_WIDTH);
	ceiling_corners[2] = vec3(c.x + x + COL_WIDTH, co.ceiling_height, c.y + y + COL_WIDTH);
	int ceiling_base = mesh.GetBaseVertex();
	for (int i = 0; i < 4; i++) mesh.AddVertex(MbVertex(ceiling_corners[i], COLOR_WHITE));
	mesh.AddQuad(ceiling_base, ceiling_base + 3, ceiling_base + 2, ceiling_base + 1);	// reverse the winding
	// walls
	for (int i = 0; i < 4; i++) {
		int xp = x - 1;
		int yp = y;
		if (i == 1) {
			xp = x;
			yp = y - 1;
		}
		else if (i == 2) {
			xp = x+1;
			yp = y;
		}
		else if (i == 3) {
			xp = x;
			yp = y+1;
		}


		if (xp < 0 || xp >= Map_Chunk::GRID_WIDTH || yp < 0 || yp >= Map_Chunk::GRID_WIDTH)
			continue;
		Map_Column& co2 = c.get(xp, yp);

		vec3 lower_wall[2];


	}


}


void Map_Edit::rebuild_map_mesh()
{
	mesh.Begin();
	
	for (auto& c : chunks)
		for (int y = 0; y < Map_Chunk::GRID_WIDTH; y++)
			for (int x = 0; x < Map_Chunk::GRID_WIDTH; x++)
				append_column(c, x, y);
}

Map_Column_Index Map_Edit::trace_squares(vec3 o, vec3 d)
{
	return Map_Column_Index();
}

void Map_Edit::key_input(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_MOUSEBUTTONDOWN:
		break;
	case SDL_MOUSEBUTTONUP: {
		vec3 dir = ray_pick(event.button.x, event.button.y);
		auto square = trace_squares(camera.position, dir);

	}break;



	}
}

void Map_Edit::draw()
{

}

void Map_Edit::update()
{
	int x, y;
	SDL_GetRelativeMouseState(&x, &y);
	camera.update_from_input(engine.keys, x, y);
}