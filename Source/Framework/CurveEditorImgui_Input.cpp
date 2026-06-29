// Mouse input handling for CurveEditorImgui: point/event dragging, pan, zoom, scrubber.
// See: draw_editor_input_and_scrubber()

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>
#include <cmath>

void CurveEditorImgui::draw_editor_input_and_scrubber(
	ImDrawList* drawlist,
	bool is_window_focused_and_mouse_in_region,
	bool is_any_event_hovered)
{
	ASSERT(drawlist != nullptr);

	// --- Drag: events ---
	if (is_dragging_selected && selecting_event) {
		events_changed_this_frame_ = true;
		if (!is_selected_event_valid()) {
			sys_print(Warning, "is_dragging_selected null entry\n");
		} else {
			auto mousepos  = ImGui::GetMousePos();
			auto gridspace = screenspace_to_grid(mousepos);
			clamp_point_to_grid(gridspace);

			auto& item = events.at(selected_curve_or_event);
			if (moving_right_side)
				item->time_end = gridspace.x;
			else
				item->time_start = gridspace.x;

			if (!item->instant_item) {
				if (item->time_end <= item->time_start)
					item->time_end = item->time_start + 1.0f;
			}
			item->y_coord = gridspace.y;
		}
	}

	// --- Drag: curve control points / tangents ---
	if (is_dragging_selected && !selecting_event) {
		auto mousepos  = ImGui::GetMousePos();
		auto gridspace = screenspace_to_grid(mousepos);

		if (!is_selected_curve_valid()) {
			sys_print(Warning, "dragging_point: selected_curve invalid\n");
			set_selected_curve(-1);
			dragged_point_index  = -1;
			is_dragging_selected = false;
		} else if (!is_selected_point_valid()) {
			sys_print(Warning, "dragged_point_index invalid\n");
			dragged_point_index  = -1;
			is_dragging_selected = false;
		} else {
			clamp_point_to_grid(gridspace);
			auto& points = curves.at(selected_curve_or_event).points;
			auto& point  = points[dragged_point_index];

			if (dragged_point_type == 0) {
				points[dragged_point_index].time  = gridspace.x;
				points[dragged_point_index].value = gridspace.y;

				// keep sorted by time
				std::sort(points.begin(), points.end(),
						  [](const auto& p, const auto& p2) { return p.time < p2.time; });

				// re-find the dragged point after sort
				for (int i = 0; i < (int)points.size(); i++) {
					if (points[i].time == gridspace.x && points[i].value == gridspace.y) {
						dragged_point_index = i;
						break;
					}
				}
			} else if (dragged_point_type == 1) {
				point.tangent0.x = gridspace.x - point.time;
				point.tangent0.y = gridspace.y - point.value;
			} else if (dragged_point_type == 2) {
				point.tangent1.x = gridspace.x - point.time;
				point.tangent1.y = gridspace.y - point.value;
			}

			if (point.type == CurvePointType::Aligned) {
				if (dragged_point_type == 1)
					point.tangent1 = -point.tangent0;
				else if (dragged_point_type == 2)
					point.tangent0 = -point.tangent1;
			}
		}
	}

	// --- Open "Add point" context menu on canvas right-click ---
	if (ImGui::GetIO().MouseClicked[1] && is_window_focused_and_mouse_in_region &&
	    is_selecting_a_curve() && dragged_point_index == -1)
	{
		ImGui::OpenPopup("curve_edit_popup");
		clickpos = screenspace_to_grid(ImGui::GetMousePos());
	}
	if (ImGui::BeginPopup("curve_edit_popup")) {
		if (!is_selected_curve_valid()) {
			sys_print(Warning, "selected_curve null in curve_edit_popup\n");
			ImGui::CloseCurrentPopup();
		} else {
			if (ImGui::Button("Add point")) {
				auto& pts = curves.at(selected_curve_or_event).points;
				clamp_point_to_grid(clickpos);
				pts.push_back({clickpos.y, clickpos.x});
				std::sort(pts.begin(), pts.end(),
						  [](const auto& p, const auto& p2) -> bool { return p.time < p2.time; });
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	// --- Pan (middle mouse button) ---
	if (ImGui::GetIO().MouseDown[2] && ImGui::IsWindowFocused()) {
		if (!started_pan)
			pan_start = screenspace_to_grid(ImGui::GetMousePos());
		started_pan = true;
		auto mousepose   = ImGui::GetMousePos();
		auto grid_wo_off = screenspace_to_grid(mousepose) - grid_offset;
		grid_offset      = pan_start - grid_wo_off;
	} else {
		started_pan = false;
	}

	// --- Zoom (mouse wheel) ---
	const float MOUSE_SCALE_EXP = 0.25f;
	const float MIN_SCALE       = 0.01f;

	const bool canvas_hovered = ImRect(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE).Contains(ImGui::GetMousePos());

	// Consume scroll whenever over the canvas so the outer inspector window doesn't also scroll.
	if (canvas_hovered && std::abs(ImGui::GetIO().MouseWheel) > 0.00001f)
		ImGui::GetIO().MouseWheel = 0.f;

	if (std::abs(ImGui::GetIO().MouseWheel) > 0.00001f &&
	    ImGui::IsWindowFocused() && ImGui::IsWindowHovered())
	{
		auto mousepos = ImGui::GetMousePos();
		auto start    = screenspace_to_grid(mousepos);
		bool ctrl_is_down = ImGui::GetIO().KeyCtrl;
		float wh = ImGui::GetIO().MouseWheel;

		if (wh > 0) {
			if (ctrl_is_down)
				scale.y += scale.y * MOUSE_SCALE_EXP;
			else
				scale.x += scale.x * MOUSE_SCALE_EXP;
		} else {
			if (ctrl_is_down) {
				scale.y -= scale.y * MOUSE_SCALE_EXP;
				if (scale.y < MIN_SCALE)
					scale.y = MIN_SCALE;
			} else {
				scale.x -= scale.x * MOUSE_SCALE_EXP;
				if (scale.x < MIN_SCALE)
					scale.x = MIN_SCALE;
			}
		}
		// Maintain the world position under the cursor
		auto grid_wo_off = screenspace_to_grid(mousepos) - grid_offset;
		grid_offset      = start - grid_wo_off;
	}

	// --- Scrubber update ---
	if (!ImGui::GetIO().MouseDown[0])
		dragging_scrubber = false;

	const float TIMELINE_HEIGHT = 20.0f;
	if (ImRect(BASE_SCREENPOS,
	           ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, BASE_SCREENPOS.y + TIMELINE_HEIGHT))
	        .Contains(ImGui::GetMousePos()) &&
	    ImGui::IsWindowFocused() && !is_dragging_selected)
	{
		if (ImGui::GetIO().MouseDown[0])
			dragging_scrubber = true;
	}

	if (dragging_scrubber) {
		auto gridspace = screenspace_to_grid(ImGui::GetMousePos());
		current_time   = gridspace.x;
		if (snap_scrubber_to_grid)
			current_time = std::round(current_time / grid_snap_size.x) * grid_snap_size.x;
		if (current_time < 0.0f)
			current_time = 0.0f;
		if (current_time > max_x_value)
			current_time = max_x_value;
		set_scrubber_this_frame = true;
	}

	// --- Draw scrubber line and time label ---
	{
		auto pos_of_scrubber = grid_to_screenspace(ImVec2(current_time, 0));
		const char* time_str = string_format("%.2f", current_time);
		auto& style          = ImGui::GetStyle();
		float time_str_width = ImGui::CalcTextSize(time_str).x + style.FramePadding.x * 1.0f;
		float off            = time_str_width * 0.5f;

		drawlist->AddLine(ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y),
						  ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y + WINDOW_SIZE.y),
						  IM_COL32(27, 145, 247, 255), 2.0f);
		drawlist->AddRectFilled(ImVec2(pos_of_scrubber.x - off, BASE_SCREENPOS.y),
								ImVec2(pos_of_scrubber.x + off, BASE_SCREENPOS.y + TIMELINE_HEIGHT),
								IM_COL32(27, 145, 247, 255), 6.0f);
		drawlist->AddText(ImVec2(pos_of_scrubber.x - off + 2.0f, BASE_SCREENPOS.y),
						  IM_COL32(255, 255, 255, 255), time_str);
	}

	// --- Generic creation context menu (right-click on empty canvas) ---
	if (!dragging_scrubber && !is_dragging_selected &&
	    ImGui::IsWindowFocused() && !is_any_event_hovered &&
	    ImGui::GetIO().MouseClicked[1])
	{
		ImGui::OpenPopup("creation_ctx_menu");
	}
	if (ImGui::BeginPopup("creation_ctx_menu")) {
		if (ImGui::MenuItem("Fit to Window"))
			fit_to_content();
		if (callback) {
			ImGui::Separator();
			callback(this);
		}
		ImGui::EndPopup();
	}
}
