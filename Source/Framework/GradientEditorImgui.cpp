#define IMGUI_DEFINE_MATH_OPERATORS
#include "GradientEditorImgui.h"
#include "imgui_internal.h"
#include <algorithm>

static ImU32 color32_to_imu32(Color32 c) {
	return IM_COL32(c.r, c.g, c.b, c.a);
}

bool GradientEditorImgui::draw_inline(const char* label, Gradient& gradient, float width, float height)
{
	bool modified = false;
	ImGui::PushID(label);

	if (width <= 0.f)
		width = ImGui::GetContentRegionAvail().x;

	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImVec2 size(width, height);
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// draw checkerboard background for alpha
	draw_list->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(128, 128, 128, 255));

	// draw gradient bar using vertex colors
	int num_segments = 64;
	float segment_width = size.x / num_segments;
	for (int i = 0; i < num_segments; i++) {
		float t0 = (float)i / num_segments;
		float t1 = (float)(i + 1) / num_segments;
		Color32 c0 = gradient.evaluate(t0);
		Color32 c1 = gradient.evaluate(t1);
		ImVec2 p0(pos.x + t0 * size.x, pos.y);
		ImVec2 p1(pos.x + t1 * size.x, pos.y + size.y);
		draw_list->AddRectFilledMultiColor(p0, p1,
			color32_to_imu32(c0), color32_to_imu32(c1),
			color32_to_imu32(c1), color32_to_imu32(c0));
	}

	// draw border
	draw_list->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), IM_COL32(100, 100, 100, 255));

	// draw stop markers
	const float marker_h = 8.f;
	for (int i = 0; i < (int)gradient.keys.size(); i++) {
		auto& key = gradient.keys[i];
		float x = pos.x + key.time * size.x;
		float y = pos.y + size.y;

		ImVec2 tri[3] = {
			{x - 4, y + marker_h},
			{x + 4, y + marker_h},
			{x, y}
		};
		ImU32 fill = (i == selected_key) ? IM_COL32(255, 255, 0, 255) : IM_COL32(200, 200, 200, 255);
		draw_list->AddTriangleFilled(tri[0], tri[1], tri[2], fill);
		draw_list->AddTriangle(tri[0], tri[1], tri[2], IM_COL32(50, 50, 50, 255));
	}

	// invisible button for interaction
	ImGui::InvisibleButton("gradient_bar", ImVec2(size.x, size.y + marker_h + 2));

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
		ImVec2 mouse = ImGui::GetMousePos();
		float click_t = glm::clamp((mouse.x - pos.x) / size.x, 0.f, 1.f);

		// check if clicking on existing key
		int clicked_key = -1;
		for (int i = 0; i < (int)gradient.keys.size(); i++) {
			float kx = pos.x + gradient.keys[i].time * size.x;
			if (glm::abs(mouse.x - kx) < 6.f) {
				clicked_key = i;
				break;
			}
		}

		if (clicked_key >= 0) {
			selected_key = clicked_key;
		}
		else {
			GradientKey new_key;
			new_key.time = click_t;
			new_key.color = gradient.evaluate(click_t);
			new_key.alpha = new_key.color.a / 255.f;
			gradient.keys.push_back(new_key);
			gradient.sort_keys();
			for (int i = 0; i < (int)gradient.keys.size(); i++) {
				if (glm::abs(gradient.keys[i].time - click_t) < 0.001f) {
					selected_key = i;
					break;
				}
			}
			modified = true;
		}
	}

	// drag selected key
	if (selected_key >= 0 && selected_key < (int)gradient.keys.size() && ImGui::IsItemActive()) {
		ImVec2 mouse = ImGui::GetMousePos();
		float new_t = glm::clamp((mouse.x - pos.x) / size.x, 0.f, 1.f);
		if (gradient.keys[selected_key].time != new_t) {
			gradient.keys[selected_key].time = new_t;
			gradient.sort_keys();
			for (int i = 0; i < (int)gradient.keys.size(); i++) {
				if (glm::abs(gradient.keys[i].time - new_t) < 0.001f) {
					selected_key = i;
					break;
				}
			}
			modified = true;
		}
	}

	// delete key
	if (selected_key >= 0 && selected_key < (int)gradient.keys.size()
		&& ImGui::IsKeyPressed(ImGuiKey_Delete) && gradient.keys.size() > 1) {
		gradient.keys.erase(gradient.keys.begin() + selected_key);
		selected_key = -1;
		modified = true;
	}

	// double-click opens popup
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
		popup_open = true;
		ImGui::OpenPopup("GradientEditorPopup");
	}

	if (popup_open) {
		draw_popup("GradientEditorPopup", gradient);
	}

	ImGui::PopID();
	return modified;
}

void GradientEditorImgui::draw_popup(const char* popup_id, Gradient& gradient)
{
	if (ImGui::BeginPopup(popup_id)) {
		if (selected_key >= 0 && selected_key < (int)gradient.keys.size()) {
			auto& key = gradient.keys[selected_key];
			color_edit[0] = key.color.r / 255.f;
			color_edit[1] = key.color.g / 255.f;
			color_edit[2] = key.color.b / 255.f;
			color_edit[3] = key.alpha;

			if (ImGui::ColorEdit4("Color", color_edit)) {
				key.color = Color32(
					(int)(color_edit[0] * 255.f),
					(int)(color_edit[1] * 255.f),
					(int)(color_edit[2] * 255.f));
				key.alpha = color_edit[3];
			}
			ImGui::DragFloat("Time", &key.time, 0.01f, 0.f, 1.f);
		}
		else {
			ImGui::Text("Select a key to edit");
		}
		ImGui::EndPopup();
	}
	else {
		popup_open = false;
	}
}
