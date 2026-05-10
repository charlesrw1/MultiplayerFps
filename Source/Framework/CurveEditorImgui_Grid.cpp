// @docs [[tooling/docs-cli]]
// Grid background and axis-label drawing for CurveEditorImgui.
// See: draw_editor_background_and_grid()

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>

// Draws X-axis grid lines and labels, then Y-axis lines and labels.
// Also draws the boundary-mask rectangles that shade regions outside
// [0, max_x_value] x [min_y_value, max_y_value].
void CurveEditorImgui::draw_editor_background_and_grid(
	ImDrawList* drawlist,
	ImVec2 canvas_pos, ImVec2 canvas_size,
	ImVec2 grid_size, ImVec2 subgrid_size,
	ImVec2 dxdy_grid, ImVec2 grid,
	float TIMELINE_HEIGHT,
	ImU32 col_grid, ImU32 col_subdiv)
{
	ASSERT(drawlist != nullptr);

	const ImVec2 subdivisions(2, 2);
	const Color32 edges = {0, 0, 0, 128};

	// --- X axis markings ---
	{
		int i = 0;
		for (float x = canvas_pos.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x);
			 x < canvas_pos.x + canvas_size.x; x += grid_size.x)
		{
			drawlist->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y), col_grid);
			drawlist->AddText(ImVec2(x, canvas_pos.y), IM_COL32(150, 150, 150, 255),
							  string_format("%.1f", grid.x + dxdy_grid.x * i));
			for (int j = 1; j < (int)subdivisions.x; j++) {
				drawlist->AddLine(ImVec2(x + j * subgrid_size.x, canvas_pos.y),
								  ImVec2(x + j * subgrid_size.x, canvas_pos.y + canvas_size.y), col_subdiv);
			}
			i++;
		}
	}

	// --- Boundary mask: origin (x < 0) and beyond max_x ---
	{
		auto origin_ss = grid_to_screenspace(ImVec2(0, 0));
		if (origin_ss.x - BASE_SCREENPOS.x >= 0) {
			drawlist->AddRectFilled(BASE_SCREENPOS,
									ImVec2(origin_ss.x, BASE_SCREENPOS.y + WINDOW_SIZE.y),
									edges.to_uint());
		}

		auto end_ss = grid_to_screenspace(ImVec2(max_x_value, 0));
		if (end_ss.x - BASE_SCREENPOS.x <= WINDOW_SIZE.x) {
			drawlist->AddRectFilled(ImVec2(end_ss.x, BASE_SCREENPOS.y),
									BASE_SCREENPOS + WINDOW_SIZE,
									edges.to_uint());
		}
	}

	// --- Y axis markings (drawn after mask boxes so labels appear on top) ---
	{
		int i = 0;
		for (float y = canvas_pos.y - fmod(grid_offset.y / base_scale.y * scale.y, grid_size.y);
			 y < canvas_pos.y + canvas_size.y; y += grid_size.y)
		{
			drawlist->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), col_grid);
			drawlist->AddText(ImVec2(canvas_pos.x, y), IM_COL32(200, 200, 200, 255),
							  string_format("%.1f", grid.y + dxdy_grid.y * i));
			for (int j = 1; j < (int)subdivisions.y; j++) {
				drawlist->AddLine(ImVec2(canvas_pos.x, y + j * subgrid_size.y),
								  ImVec2(canvas_pos.x + canvas_size.x, y + j * subgrid_size.y), col_subdiv);
			}
			i++;
		}
	}
}
