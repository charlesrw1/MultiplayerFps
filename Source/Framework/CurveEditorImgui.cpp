
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>

ImVec2 CurveEditorImgui::grid_to_screenspace(ImVec2 grid) const {
	ASSERT(scale.x > 0.0f && scale.y != 0.0f);
	return (grid - grid_offset) / base_scale * scale + BASE_SCREENPOS;
}

ImVec2 CurveEditorImgui::screenspace_to_grid(ImVec2 screen) const {
	ASSERT(scale.x > 0.0f && scale.y != 0.0f);
	return (screen - BASE_SCREENPOS) * base_scale * (ImVec2(1.0 / scale.x, 1.0 / scale.y)) + grid_offset;
}

void draw_rectangle_rotated(ImVec2 max, ImVec2 min, ImColor color) {
	ASSERT(max.x >= min.x || max.y != min.y); // degenerate diamond guard

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	auto center = max + min;
	center *= 0.5f;
	draw_list->AddTriangleFilled(ImVec2(max.x, center.y), ImVec2(center.x, max.y), ImVec2(min.x, center.y), color);
	draw_list->AddTriangleFilled(ImVec2(max.x, center.y), ImVec2(min.x, center.y), ImVec2(center.x, min.y), color);
	draw_list->AddLine(ImVec2(max.x, center.y), ImVec2(center.x, max.y), COLOR_WHITE.to_uint(), 1.f);
	draw_list->AddLine(ImVec2(center.x, max.y), ImVec2(min.x, center.y), COLOR_WHITE.to_uint(), 1.f);
	draw_list->AddLine(ImVec2(min.x, center.y), ImVec2(center.x, min.y), COLOR_WHITE.to_uint(), 1.f);
	draw_list->AddLine(ImVec2(center.x, min.y), ImVec2(max.x, center.y), COLOR_WHITE.to_uint(), 1.f);
}

bool track_overlaps(float start, float end, float s2, float end2) {
	ASSERT(end >= start && end2 >= s2);
	return start <= end2 && end >= s2;
}

glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3) {
	ASSERT(t >= 0.0f && t <= 1.0f);
	auto a = (1 - t) * (1 - t) * (1 - t) * p0;
	auto b = 3 * t * (1 - t) * (1 - t) * p1;
	auto c = 3 * t * t * (1 - t) * p2;
	auto d = t * t * t * p3;
	return a + b + c + d;
}

extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

void CurveEditorImgui::draw_editor_space() {
	ASSERT(WINDOW_SIZE.x > 0 && WINDOW_SIZE.y > 0);

	auto drawlist = ImGui::GetWindowDrawList();
	const Color32 background = {36, 36, 36};
	const Color32 edges = {0, 0, 0, 128};

	// draw background
	const float temp_ = WINDOW_SIZE.y;
	WINDOW_SIZE.y -= 20;
	WINDOW_SIZE.y = glm::max(WINDOW_SIZE.y, 0.1f);

	// top black bar for timeline
	const float TIMELINE_HEIGHT = 20.0;

	drawlist->AddRectFilled(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE, background.to_uint());

	ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
	ImVec2 canvas_size = WINDOW_SIZE;

	// to prevent moving window
	ImGui::InvisibleButton(
		"dummy234_",
		ImVec2(WINDOW_SIZE.x,
			   std::max(temp_ - 5.0f,
						0.01f) /* hacked value, not sure how to get the exact value, some padding variable likely*/));
	ImGui::PushClipRect(ImVec2(BASE_SCREENPOS), BASE_SCREENPOS + WINDOW_SIZE, true);

	const bool is_window_focused_and_mouse_in_region =
		ImGui::IsWindowFocused() && ImRect(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE).Contains(ImGui::GetMousePos());
	{
		auto min_ss = grid_to_screenspace(ImVec2(0, min_y_value));
		if (min_ss.y - BASE_SCREENPOS.y <= WINDOW_SIZE.y) {
			drawlist->AddRectFilled(ImVec2(BASE_SCREENPOS.x, min_ss.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
		}
		auto max_ss = grid_to_screenspace(ImVec2(0, max_y_value));
		if (max_ss.y - BASE_SCREENPOS.y >= 0) {
			drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, max_ss.y),
									edges.to_uint());
		}
		drawlist->AddRectFilled(BASE_SCREENPOS,
								ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, BASE_SCREENPOS.y + TIMELINE_HEIGHT),
								IM_COL32(0, 0, 0, 255));
	}

	const ImVec2 GRID_SPACING = ImVec2(64.0, 30.0);
	ImVec2 grid_size = GRID_SPACING * scale;

	const ImVec2 subdivisions(2, 2);
	const ImVec2 base_grid_size = GRID_SPACING;
	const ImVec2 subdiv_size = ImVec2(2, 2);
	const ImVec2 inv_subdiv_size = ImVec2(1.0 / subdiv_size.x, 1.0 / subdiv_size.y);
	if (grid_size.x < base_grid_size.x) {
		while (grid_size.x < base_grid_size.x * inv_subdiv_size.x)
			grid_size.x *= subdiv_size.x;
	} else {
		while (grid_size.x > base_grid_size.x)
			grid_size.x /= subdiv_size.x;
	}
	if (grid_size.y < base_grid_size.y) {
		while (grid_size.y < base_grid_size.y * inv_subdiv_size.y)
			grid_size.y *= subdiv_size.y;
	} else {
		while (grid_size.y > base_grid_size.y)
			grid_size.y /= subdiv_size.y;
	}

	ImVec2 subgrid_size = grid_size * ImVec2(1.0 / subdivisions.x, 1.0 / subdivisions.y);
	ImVec2 dxdy_subgrid = screenspace_to_grid(subgrid_size) - screenspace_to_grid(ImVec2(0, 0));
	if (dxdy_subgrid.x < 1.0) {
		grid_size.x = scale.x / base_scale.x * subdivisions.x;
		subgrid_size.x = grid_size.x / subdivisions.x;
	}

	ImVec2 dxdy_grid = screenspace_to_grid(grid_size) - screenspace_to_grid(ImVec2(0, 0));

	ImU32 col_grid = IM_COL32(255, 50, 50, 40);
	ImU32 col_subdiv = IM_COL32(200, 200, 200, 20);

	float valx = canvas_pos.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x);
	float valy = canvas_pos.y - fmod(grid_offset.y / base_scale.y * scale.y, grid_size.y);
	const ImVec2 grid = screenspace_to_grid(ImVec2(valx, valy));

	draw_editor_background_and_grid(drawlist, canvas_pos, canvas_size, grid_size, subgrid_size,
	                                dxdy_grid, grid, TIMELINE_HEIGHT, col_grid, col_subdiv);

	// stop dragging if mouse isn't down
	if (is_dragging_selected && !ImGui::GetIO().MouseDown[0]) {
		dragged_point_index = -1;
		dragged_point_type = 0;
		is_dragging_selected = false;
		sys_print(Debug, "stopped dragging\n");
	}

	const bool can_start_dragging = !is_dragging_selected && !dragging_scrubber;

	draw_curves_and_points(drawlist, can_start_dragging);

	bool is_any_event_hovered = false;
	draw_events_and_event_popups(drawlist, is_any_event_hovered);

	draw_editor_input_and_scrubber(drawlist, is_window_focused_and_mouse_in_region, is_any_event_hovered);

	ImGui::PopClipRect();
}
