// Curve segment and control-point rendering for CurveEditorImgui.
// See: draw_curves_and_points()

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>

// Forward declarations of helpers defined in CurveEditorImgui.cpp
void draw_rectangle_rotated(ImVec2 max, ImVec2 min, ImColor color);
glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1,
                           const glm::vec2& p2, const glm::vec2& p3);

void CurveEditorImgui::draw_curves_and_points(ImDrawList* drawlist, bool can_start_dragging)
{
	ASSERT(drawlist != nullptr);

	for (int curve_index = 0; curve_index < (int)curves.size(); curve_index++) {
		if (!curves[curve_index].visible)
			continue;

		ImU32 curvecol = 0;
		{
			auto color = curves[curve_index].color;
			if (!is_curve_selected(curve_index))
				color.a = 100;
			curvecol = color.to_uint();
		}

		auto& points = curves[curve_index].points;

		// --- Draw segments between consecutive points ---
		for (int i = 0; i < (int)points.size() - 1; i++) {
			auto& point = points[i];
			if (point.type == CurvePointType::Constant) {
				auto ss_start = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
				auto ss_end   = grid_to_screenspace(ImVec2(points[i + 1].time, points[i].value));
				drawlist->AddLine(ss_start, ss_end, curvecol);
			} else if (point.type == CurvePointType::Linear) {
				auto ss_start = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
				auto ss_end   = grid_to_screenspace(ImVec2(points[i + 1].time, points[i + 1].value));
				drawlist->AddLine(ss_start, ss_end, curvecol);
			} else if (point.type == CurvePointType::SplitTangents ||
			           point.type == CurvePointType::Aligned) {
				const int BEZIER_CURVE_SUBDIV = 30;
				glm::vec2 pointsout[BEZIER_CURVE_SUBDIV];
				glm::vec2 p0 = {point.time, point.value};
				glm::vec2 p1 = p0 + point.tangent1;
				glm::vec2 p3 = {points[i + 1].time, points[i + 1].value};
				glm::vec2 p2 = p3 + points[i + 1].tangent0;
				for (int j = 0; j < BEZIER_CURVE_SUBDIV; j++) {
					float t = j / (float(BEZIER_CURVE_SUBDIV) - 1.0f);
					pointsout[j] = bezier_evaluate(t, p0, p1, p2, p3);
				}
				for (int j = 0; j < BEZIER_CURVE_SUBDIV - 1; j++) {
					drawlist->AddLine(
						grid_to_screenspace(ImVec2(pointsout[j].x, pointsout[j].y)),
						grid_to_screenspace(ImVec2(pointsout[j + 1].x, pointsout[j + 1].y)),
						curvecol);
				}
			}
		}

		// --- Extend flat lines to the left/right edges ---
		if (!points.empty()) {
			auto& pointfront = points.front();
			auto ss_start = grid_to_screenspace(ImVec2(pointfront.time, pointfront.value));
			auto ss_end   = ImVec2(BASE_SCREENPOS.x, ss_start.y);
			drawlist->AddLine(ss_start, ss_end, curvecol);

			auto& pointback = points.back();
			ss_start = grid_to_screenspace(ImVec2(pointback.time, pointback.value));
			ss_end   = ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, ss_start.y);
			drawlist->AddLine(ss_start, ss_end, curvecol);
		}

		// --- Draw control points and tangent handles ---
		const float POINT_RADIUS_SS      = 4;
		const float POINT_SELECTION_RADIUS = 10;

		for (int i = 0; i < (int)points.size(); i++) {
			auto& point = points[i];

			auto ss_point = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
			ImVec2 min_pt  = ss_point - ImVec2(POINT_RADIUS_SS, POINT_RADIUS_SS);
			ImVec2 max_pt  = ss_point + ImVec2(POINT_RADIUS_SS, POINT_RADIUS_SS);
			ImVec2 min_sel = ss_point - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
			ImVec2 max_sel = ss_point + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

			bool draw_tooltip = false;
			if (is_dragging_selected && i == dragged_point_index && is_curve_selected(curve_index))
				draw_tooltip = true;

			if (ImRect(min_sel, max_sel).Contains(ImGui::GetMousePos())) {
				if (dragged_point_index == -1)
					draw_tooltip = true;

				if (can_start_dragging && ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
					set_selected_curve(curve_index);
					dragged_point_index = i;
					dragged_point_type  = 0; // point
					is_dragging_selected = true;
				}

				if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
					set_selected_curve(curve_index);
					ImGui::OpenPopup("point_popup");
					dragged_point_index = i;
				}
			}

			const bool draw_tangents =
				is_curve_selected(curve_index) &&
				(point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned);

			// --- Tangent handle hit-testing ---
			if (draw_tangents) {
				auto tan_point_ss = grid_to_screenspace(
					ImVec2(points[i].time + points[i].tangent0.x, points[i].value + points[i].tangent0.y));
				ImVec2 min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
				ImVec2 max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

				auto tangent_select = [&](int point_type) {
					if (can_start_dragging &&
					    ImRect(min_sel_tan, max_sel_tan).Contains(ImGui::GetMousePos())) {
						if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
							set_selected_curve(curve_index);
							dragged_point_index  = i;
							dragged_point_type   = point_type;
							is_dragging_selected = true;
						}
					}
				};
				tangent_select(1 /* tangent0 */);

				tan_point_ss = grid_to_screenspace(
					ImVec2(points[i].time + points[i].tangent1.x, points[i].value + points[i].tangent1.y));
				min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
				max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
				tangent_select(2 /* tangent1 */);
			}

			if (draw_tooltip) {
				if (ImGui::BeginTooltip()) {
					ImGui::Text("%f %f", points[i].time, points[i].value);
					ImGui::EndTooltip();
				}
			}

			// --- Tangent handle visuals ---
			if (draw_tangents) {
				auto tan_point_ss = grid_to_screenspace(
					ImVec2(points[i].time + points[i].tangent0.x, points[i].value + points[i].tangent0.y));
				drawlist->AddLine(ss_point, tan_point_ss, IM_COL32(200, 200, 200, 180));
				drawlist->AddCircleFilled(tan_point_ss, POINT_RADIUS_SS, IM_COL32(222, 222, 222, 255));

				tan_point_ss = grid_to_screenspace(
					ImVec2(points[i].time + points[i].tangent1.x, points[i].value + points[i].tangent1.y));
				drawlist->AddLine(ss_point, tan_point_ss, IM_COL32(200, 200, 200, 180));
				drawlist->AddCircleFilled(tan_point_ss, POINT_RADIUS_SS, IM_COL32(222, 222, 222, 255));
			}

			draw_rectangle_rotated(min_pt, max_pt, curvecol);
		}
	}

	// --- point_popup (right-click on a control point) ---
	if (ImGui::BeginPopup("point_popup")) {
		if (!is_selected_curve_valid()) {
			sys_print(Warning, "selected_curve == -1 or invalid in point_popup\n");
			set_selected_curve(-1);
			ImGui::CloseCurrentPopup();
		} else if (!is_selected_point_valid()) {
			sys_print(Warning, "point_index_for_popup invalid\n");
			dragged_point_index = -1;
			ImGui::CloseCurrentPopup();
		} else {
			auto& pts   = curves.at(selected_curve_or_event).points;
			auto  enum_ = EnumTrait<CurvePointType>::StaticEnumType.find_for_value(
				(int)pts.at(dragged_point_index).type);
			ASSERT(enum_);

			bool close_the_popup = false;
			if (ImGui::BeginCombo("##type", enum_->name)) {
				for (auto& e : EnumTrait<CurvePointType>::StaticEnumType) {
					bool selected = e.value == (int)pts[dragged_point_index].type;
					if (ImGui::Selectable(e.name, &selected)) {
						pts[dragged_point_index].type = CurvePointType(e.value);
						close_the_popup = true;
					}
				}
				ImGui::EndCombo();
			}
			if (ImGui::Button("Delete point")) {
				pts.erase(pts.begin() + dragged_point_index);
				close_the_popup = true;
			}
			if (close_the_popup) {
				dragged_point_index = -1;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

}
