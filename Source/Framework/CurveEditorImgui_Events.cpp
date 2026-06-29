// Sequencer event drawing, hit-testing, and popup handling for CurveEditorImgui.
// See: draw_events_and_event_popups()

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"

// Forward declaration of diamond-shape helper defined in CurveEditorImgui.cpp
void draw_rectangle_rotated(ImVec2 max, ImVec2 min, ImColor color);

void CurveEditorImgui::draw_events_and_event_popups(ImDrawList* drawlist, bool& is_any_event_hovered)
{
	ASSERT(drawlist != nullptr);

	// --- Lambda: draw a single event block and return true if it is hovered ---
	auto draw_event = [&](int ITEM_INDEX) -> bool {
		ASSERT(ITEM_INDEX >= 0 && ITEM_INDEX < (int)events.size());

		const float EVENT_ITEM_HEIGHT = 20.0f;
		auto  item  = events[ITEM_INDEX].get();
		Color32 color = item->color;

		float x_start_ss = grid_to_screenspace(ImVec2(item->time_start, 0)).x;
		float x_end_ss   = grid_to_screenspace(ImVec2(item->time_end,   0)).x;
		float y_start_ss = grid_to_screenspace(ImVec2(0, item->y_coord)).y;
		float y_end_ss   = y_start_ss + EVENT_ITEM_HEIGHT;

		if (item->instant_item) {
			auto size = ImGui::CalcTextSize(item->get_name().c_str());
			x_end_ss = x_start_ss + size.x + 8.0f;
		}

		Color32 rect_color = color;
		rect_color.a = 128;
		if (is_event_selected(ITEM_INDEX))
			rect_color = {252, 186, 3, 200};

		drawlist->AddRectFilled(ImVec2(x_start_ss, y_start_ss), ImVec2(x_end_ss, y_end_ss),
								rect_color.to_uint(), 2.0f);

		bool return_hovered = false;
		{
			const float   HANDLE_SELECT_RADIUS   = 8;
			const float   HANDLE_RADIUS          = 4;
			const ImVec2  HANDLE_RADIUS_V        = ImVec2(HANDLE_RADIUS, HANDLE_RADIUS);
			const ImVec2  HANDLE_SELECT_RADIUS_V = ImVec2(HANDLE_SELECT_RADIUS, HANDLE_SELECT_RADIUS);

			ImVec2 lefthandle_c = ImVec2(x_start_ss, y_start_ss + EVENT_ITEM_HEIGHT * 0.5f);

			if (!is_dragging_selected &&
			    ImRect(lefthandle_c - HANDLE_SELECT_RADIUS_V, lefthandle_c + HANDLE_SELECT_RADIUS_V)
			        .Contains(ImGui::GetMousePos()))
			{
				if (ImGui::GetIO().MouseClicked[0])
					set_selected_event(ITEM_INDEX, true);
			}
			{
				const bool is_select =
					is_dragging_selected && is_event_selected(ITEM_INDEX) && !moving_right_side;
				draw_rectangle_rotated(lefthandle_c + HANDLE_RADIUS_V, lefthandle_c - HANDLE_RADIUS_V,
									   is_select ? COLOR_WHITE.to_uint() : color.to_uint());
			}

			if (!item->instant_item) {
				ImVec2 righthandle_c = ImVec2(x_end_ss, y_start_ss + EVENT_ITEM_HEIGHT * 0.5f);
				if (!is_dragging_selected &&
				    ImRect(righthandle_c - HANDLE_SELECT_RADIUS_V, righthandle_c + HANDLE_SELECT_RADIUS_V)
				        .Contains(ImGui::GetMousePos()))
				{
					if (ImGui::GetIO().MouseClicked[0])
						set_selected_event(ITEM_INDEX, false);
				}
				const bool is_select =
					is_dragging_selected && is_event_selected(ITEM_INDEX) && moving_right_side;
				draw_rectangle_rotated(righthandle_c + HANDLE_RADIUS_V, righthandle_c - HANDLE_RADIUS_V,
									   is_select ? COLOR_WHITE.to_uint() : color.to_uint());
			}

			if (!is_dragging_selected &&
			    ImRect(ImVec2(x_start_ss, y_start_ss), ImVec2(x_end_ss, y_end_ss))
			        .Contains(ImGui::GetMousePos()))
			{
				if (ImGui::GetIO().MouseClicked[0]) {
					set_selected_event(ITEM_INDEX, true);
					is_dragging_selected = false; // intentionally false: body click doesn't start drag
				}
				return_hovered = true;
				if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
					set_selected_event(ITEM_INDEX, true);
					ImGui::OpenPopup("item_popup");
				}
			}

			drawlist->AddText(ImVec2(x_start_ss + 6.0f, y_start_ss + 1.0f),
							  COLOR_WHITE.to_uint(), item->get_name().c_str());
		}
		return return_hovered;
	};

	// --- Draw all events ---
	is_any_event_hovered = false;
	if (drawing_events) {
		for (int i = 0; i < (int)events.size(); i++)
			is_any_event_hovered |= draw_event(i);
	}

	// --- item_popup (right-click on an event block) ---
	if (ImGui::BeginPopup("item_popup")) {
		if (!is_selected_event_valid()) {
			sys_print(Warning, "item_popup invalid item\n");
			set_selected_event(-1, true);
			ImGui::CloseCurrentPopup();
		} else {
			if (ImGui::Button("Delete")) {
				events_changed_this_frame_ = true;
				events.erase(events.begin() + selected_curve_or_event);
				selected_curve_or_event = -1;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}
