// Top-level draw() for CurveEditorImgui: ImGui window, sidebar table, curve list, popups.

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"

extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);
extern float evaluate_editing_curve(const EditingCurve& curve, float t);

void CurveEditorImgui::draw()
{
	ASSERT(!window_name.empty());

	set_scrubber_this_frame = false;
	if (!ImGui::Begin(window_name.c_str())) {
		ImGui::End();
		return;
	}

	draw_content();

	ImGui::End();
}

void CurveEditorImgui::draw_content()
{
	set_scrubber_this_frame = false;

	uint32_t ent_list_flags =
		ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg     | ImGuiTableFlags_Resizable |
		ImGuiTableFlags_Sortable;

	const int num_cols = 2;
	bool open_curve_popup = false;

	if (ImGui::BeginTable("curveedit", num_cols, ent_list_flags)) {
		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed,   100.0f, 0);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch,   0.0f, 0);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		// --- Left panel: options button, events row, curve list ---
		{
			uint32_t inner_flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
			if (ImGui::BeginTable("flags", 1, inner_flags)) {
				ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 50.0f, 0);
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				// Options popup
				if (ImGui::Button("Options.."))
					ImGui::OpenPopup("curve_ed_options");
				if (ImGui::BeginPopup("curve_ed_options")) {
					ImGui::Checkbox("Snap X to grid", &enable_grid_snapping_x);
					if (enable_grid_snapping_x) {
						ImGui::SameLine();
						ImGui::InputFloat("##amtx", &grid_snap_size.x, 0.1f);
						if (grid_snap_size.x < 0.001f)
							grid_snap_size.x = 0.001f;
					}
					ImGui::Checkbox("Snap Y to grid", &enable_grid_snapping_y);
					if (enable_grid_snapping_y) {
						ImGui::SameLine();
						ImGui::InputFloat("##amty", &grid_snap_size.y, 0.1f);
						if (grid_snap_size.y < 0.001f)
							grid_snap_size.y = 0.001f;
					}
					ImGui::InputFloat("Max X", &max_x_value);
					if (max_x_value <= 0.001f)
						max_x_value = 0.001f;
					ImGui::InputFloat("Min Y", &min_y_value);
					ImGui::InputFloat("Max Y", &max_y_value);
					if (max_y_value <= min_y_value)
						max_y_value = min_y_value + 0.01f;
					ImGui::Checkbox("Snap Time to grid", &snap_scrubber_to_grid);
					ImGui::EndPopup();
				}
				ImGui::Separator();

				// Events row
				ImGui::TableNextRow();
				ImGui::TableNextColumn();

				ImGuiSelectableFlags sel_flags =
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
				if (ImGui::Selectable("##selecteddevent", selecting_event, sel_flags, ImVec2(0, 24)))
					set_selected_event(-1, false);
				if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1])
					set_selected_event(-1, false);
				ImGui::SameLine();
				ImGui::Checkbox("##check", &drawing_events);
				ImGui::SameLine();
				ImGui::Text("Events");

				ImGui::Separator();

				// Curve rows
				for (int row_n = 0; row_n < (int)curves.size(); row_n++) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					auto& res = curves[row_n];

					ImGui::PushID(res.curve_id);
					ImGuiSelectableFlags sf =
						ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
					if (ImGui::Selectable("##selectednode", is_curve_selected(row_n), sf, ImVec2(0, 24)))
						set_selected_curve(row_n);
					if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) {
						open_curve_popup = true;
						set_selected_curve(row_n);
					}
					ImGui::SameLine();
					ImGui::Checkbox("##check", &res.visible);
					ImGui::SameLine();
					ImGui::TextColored(color32_to_imvec4(res.color), res.name.c_str());
					ImGui::PopID();
				}

				// Optional "Add row" button
				if (show_add_curve_button) {
					ImGui::TableNextRow();
					ImGui::TableNextColumn();

					ImGuiStyle& style = ImGui::GetStyle();
					float size  = ImGui::CalcTextSize("Add row").x + style.FramePadding.x * 2.0f;
					float avail = ImGui::GetContentRegionAvail().x;
					float off   = (avail - size) * 0.5f;
					if (off > 0.0f)
						ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
					if (ImGui::Button("Add row")) {
						EditingCurve ed;
						ed.name = "unnamed_row";
						add_curve(ed);
					}
				}

				ImGui::EndTable();
			}
		}

		// --- Right panel: the curve editor canvas ---
		ImGui::TableNextColumn();
		BASE_SCREENPOS = ImGui::GetCursorScreenPos();
		WINDOW_SIZE    = ImGui::GetContentRegionAvail();
		if (pending_fit && WINDOW_SIZE.x > 0 && WINDOW_SIZE.y > 0) {
			pending_fit = false;
			fit_to_content();
		}
		draw_editor_space();

		ImGui::EndTable();
	}

	// Open curve popup deferred (must happen outside the table row)
	if (open_curve_popup)
		ImGui::OpenPopup("curve_popup");

	// --- curve_popup (right-click on a curve row in the sidebar) ---
	if (ImGui::BeginPopup("curve_popup")) {
		if (!is_selected_curve_valid()) {
			sys_print(Warning, "curve_popup bad selected_curve\n");
			set_selected_curve(-1);
			ImGui::CloseCurrentPopup();
		} else {
			auto& curve = curves.at(selected_curve_or_event);

			ImGui::InputText("##name",
							 (char*)curve.name.data(),
							 curve.name.size() + 1 /* null terminator */,
							 ImGuiInputTextFlags_CallbackResize,
							 imgui_std_string_resize, &curve.name);

			ImVec4 v4 = color32_to_imvec4(curve.color);
			ImGui::ColorEdit3("##color", &v4.x, ImGuiColorEditFlags_NoInputs);
			curve.color.r = (uint8_t)(v4.x * 255);
			curve.color.g = (uint8_t)(v4.y * 255);
			curve.color.b = (uint8_t)(v4.z * 255);

			if (ImGui::Button("Delete")) {
				curves.erase(curves.begin() + selected_curve_or_event);
				set_selected_curve(-1);
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
}

void CurveEditorImgui::fit_to_content()
{
	float padding = 0.05f;
	float x_range = glm::max(max_x_value, 0.01f);
	float y_range = glm::max(max_y_value - min_y_value, 0.01f);

	float x0 = -x_range * padding;
	float y0 = min_y_value - y_range * padding;
	float y1 = max_y_value + y_range * padding;
	float total_x = x_range * (1.f + 2.f * padding);
	float total_y = y_range * (1.f + 2.f * padding);

	grid_offset = ImVec2(x0, y1);
	float win_w = (WINDOW_SIZE.x > 0) ? WINDOW_SIZE.x : 600.f;
	float win_h = (WINDOW_SIZE.y > 0) ? WINDOW_SIZE.y : 350.f;
	scale.x = glm::max(total_x / (win_w * base_scale.x), 0.01f);
	scale.y = glm::max(total_y / (-win_h * base_scale.y), 0.01f);
}

bool CurveEditorImgui::draw_curve_preview(const char* id, const EditingCurve& curve, float width, float height)
{
	ImGui::PushID(id);
	if (width <= 0.f)
		width = ImGui::GetContentRegionAvail().x;
	width = glm::max(width, 20.f);

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 size(width, height);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(40, 40, 40, 255));

	if (!curve.points.empty()) {
		const int segments = (int)width;
		for (int i = 0; i < segments; i++) {
			float t0 = (float)i / segments;
			float t1 = (float)(i + 1) / segments;
			float v0 = evaluate_editing_curve(curve, t0);
			float v1 = evaluate_editing_curve(curve, t1);
			v0 = glm::clamp(v0, 0.f, 1.f);
			v1 = glm::clamp(v1, 0.f, 1.f);
			ImVec2 p0(pos.x + t0 * size.x, pos.y + (1.f - v0) * size.y);
			ImVec2 p1(pos.x + t1 * size.x, pos.y + (1.f - v1) * size.y);
			draw_list->AddLine(p0, p1, IM_COL32(200, 200, 100, 255), 1.5f);
		}
	}

	draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 100, 100, 255));

	bool clicked = ImGui::InvisibleButton("##preview", size);
	ImGui::PopID();
	return clicked;
}
