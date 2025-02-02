
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>


inline ImVec2 CurveEditorImgui::grid_to_screenspace(ImVec2 grid) const {
    return (grid - grid_offset) / base_scale*scale + BASE_SCREENPOS;
}

inline ImVec2 CurveEditorImgui::screenspace_to_grid(ImVec2 screen) const {
    return (screen - BASE_SCREENPOS) * base_scale * (ImVec2(1.0/scale.x,1.0/scale.y)) + grid_offset;
}

static void draw_rectangle_rotated(ImVec2 max, ImVec2 min, ImColor color)
{

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

glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3)
{
    auto a = (1 - t) * (1 - t) * (1 - t) * p0;
    auto b = 3 * t * (1 - t) * (1 - t) * p1;
    auto c = 3 * t * t * (1 - t) * p2;
    auto d = t * t * t * p3;
    return a + b + c + d;
}

int imgui_std_string_resize(ImGuiInputTextCallbackData* data)
{
    std::string* user = (std::string*)data->UserData;
    assert(user);

    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
        user->resize(data->BufSize);
        data->Buf = (char*)user->data();
    }

    return 0;
}

void CurveEditorImgui::draw_editor_space()
{

    auto drawlist = ImGui::GetWindowDrawList();
    const Color32 background = { 36, 36, 36 };
    const Color32 edges = { 0, 0, 0, 128 };

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
    ImGui::InvisibleButton("dummy234_", ImVec2(WINDOW_SIZE.x,
        std::max(temp_-5.0f, 0.01f) /* hacked value, not sure how to get the exact value, some padding variable likely*/));
    ImGui::PushClipRect(ImVec2(BASE_SCREENPOS), BASE_SCREENPOS + WINDOW_SIZE, true);

    const bool is_window_focused_and_mouse_in_region = ImGui::IsWindowFocused() && ImRect(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE).Contains(ImGui::GetMousePos());
    {

        auto min_ss = grid_to_screenspace(ImVec2(0, min_y_value));
 
        if (min_ss.y - BASE_SCREENPOS.y <= WINDOW_SIZE.y) {
            drawlist->AddRectFilled(ImVec2(BASE_SCREENPOS.x, min_ss.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
        }
        auto max_ss = grid_to_screenspace(ImVec2(0, max_y_value));
        if (max_ss.y - BASE_SCREENPOS.y >= 0) {
            drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, max_ss.y), edges.to_uint());
        }


        drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, BASE_SCREENPOS.y + TIMELINE_HEIGHT),
            IM_COL32(0, 0, 0, 255));
    }

    const ImVec2 GRID_SPACING = ImVec2(64.0, 30.0);
    ImVec2 grid_size = GRID_SPACING * scale;

    const ImVec2 subdivisions(2, 2);
    const ImVec2 base_grid_size = GRID_SPACING;
    const ImVec2 subdiv_size = ImVec2(2, 2);
    const ImVec2 inv_subdiv_size = ImVec2(1.0 / subdiv_size.x, 1.0 / subdiv_size.y);
    if (grid_size.x < base_grid_size.x) {
        // too small
        while (grid_size.x < base_grid_size.x * inv_subdiv_size.x)
            grid_size.x *= subdiv_size.x;
    }
    else {
        while (grid_size.x > base_grid_size.x)
            grid_size.x /= subdiv_size.x;
    }
    if (grid_size.y < base_grid_size.y) {
        // too small
        while (grid_size.y < base_grid_size.y * inv_subdiv_size.y)
            grid_size.y *= subdiv_size.y;
    }
    else {
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
    {
        // X axis markings
        int i = 0;
        for (float x = canvas_pos.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x); x < canvas_pos.x + canvas_size.x; x += grid_size.x) {
            drawlist->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y), col_grid);
            drawlist->AddText(ImVec2(x, canvas_pos.y), IM_COL32(150, 150, 150, 255), string_format("%.1f", grid.x + dxdy_grid.x * i));
            for (int i = 1; i < subdivisions.x; i++) {
                drawlist->AddLine(ImVec2(x + i * subgrid_size.x, canvas_pos.y), ImVec2(x + i * subgrid_size.x, canvas_pos.y + canvas_size.y), col_subdiv);
            }
            i++;
        }
    }

    // draw/update points
    for (int curve_index = 0; curve_index < curves.size(); curve_index++) {
        
        if (!curves[curve_index].visible)
            continue;

        ImU32 curvecol = 0;
        {
            auto color = curves[curve_index].color;
            if (curve_index != selected_curve)
                color.a = 100;
            curvecol = color.to_uint();
        }


        auto& points = curves[curve_index].points;

        // draw the points
        for (int i = 0; i < (int)points.size() - 1; i++) {
            auto& point = points[i];
            if (point.type == CurvePointType::Constant) {
                auto ss_start = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
                auto ss_end = grid_to_screenspace(ImVec2(points[i + 1].time, points[i].value));
                drawlist->AddLine(ss_start, ss_end, curvecol);
            }
            else if (point.type == CurvePointType::Linear) {
                auto ss_start = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
                auto ss_end = grid_to_screenspace(ImVec2(points[i + 1].time, points[i + 1].value));
                drawlist->AddLine(ss_start, ss_end, curvecol);
            }
            else if (point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned) {
                const int BEZIER_CURVE_SUBDIV = 30;
                glm::vec2 pointsout[BEZIER_CURVE_SUBDIV];
                glm::vec2 p0 = { point.time,point.value };
                glm::vec2 p1 = p0 + point.tangent1;
                glm::vec2 p3 = { points[i + 1].time,points[i + 1].value };
                glm::vec2 p2 = p3 + points[i + 1].tangent0;
                for (int j = 0; j < BEZIER_CURVE_SUBDIV; j++) {
                    float t = j / (float(BEZIER_CURVE_SUBDIV) - 1.0);
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

        if (!points.empty()) {
            auto& pointfront = points.front();
            auto ss_start = grid_to_screenspace(ImVec2(pointfront.time, pointfront.value));
            auto ss_end = ImVec2(BASE_SCREENPOS.x, ss_start.y);
            drawlist->AddLine(ss_start, ss_end, curvecol);
            auto& pointback = points.back();
            ss_start = grid_to_screenspace(ImVec2(pointback.time, pointback.value));
            ss_end = ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, ss_start.y);
            drawlist->AddLine(ss_start, ss_end, curvecol);
        }

        // stop dragging if mouse isnt down
        if (dragging_point && !ImGui::GetIO().MouseDown[0]) {
            dragged_point_index = -1;
            dragged_point_type = 0;
            dragging_point = false;
        }

        const float POINT_RADIUS_SS = 4;
        const float POINT_SELECTION_RADIUS = 10;
        for (int i = 0; i < points.size(); i++) {

            auto& point = points[i];

            auto ss_point = grid_to_screenspace(ImVec2(points[i].time, points[i].value));
            ImVec2 min = ss_point - ImVec2(POINT_RADIUS_SS, POINT_RADIUS_SS);
            ImVec2 max = ss_point + ImVec2(POINT_RADIUS_SS, POINT_RADIUS_SS);
            ImVec2 min_sel = ss_point - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
            ImVec2 max_sel = ss_point + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

            bool draw_tooltip = false;
            if (i == dragged_point_index && selected_curve == curve_index)
                draw_tooltip = true;
            if (ImRect(min_sel, max_sel).Contains(ImGui::GetMousePos())) {
                if (dragged_point_index == -1)
                    draw_tooltip = true;

                if (!dragging_point&&!dragging_scrubber&&ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
                    // set curve
                    set_selected_curve(curve_index);

                    dragged_point_index = i;
                    dragged_point_type = 0;  // point type
                    dragging_point = true;
                }

                if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
                    // set curve
                    set_selected_curve(curve_index);

                    ImGui::OpenPopup("point_popup");
                    point_index_for_popup = i;
                }
            }
            const bool draw_tangents = selected_curve == curve_index && (point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned);
            if (draw_tangents) {
                auto tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent0.x, points[i].value + points[i].tangent0.y));
                ImVec2 min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
                ImVec2 max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

                if (!dragging_point&&!dragging_scrubber && ImRect(min_sel_tan, max_sel_tan).Contains(ImGui::GetMousePos())) {

                    if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
                        set_selected_curve(curve_index);
                        dragged_point_index = i;
                        dragged_point_type = 1;  // tangent0 type
                        dragging_point = true;
                    }
                }

                tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent1.x, points[i].value + points[i].tangent1.y));
                min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
                max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

                if (!dragging_point && !dragging_scrubber&&ImRect(min_sel_tan, max_sel_tan).Contains(ImGui::GetMousePos())) {

                    if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
                        set_selected_curve(curve_index);
                        dragged_point_index = i;
                        dragged_point_type = 2;  // tangent1 type
                        dragging_point = true;
                    }
                }
            }

            if (draw_tooltip) {
                if (ImGui::BeginTooltip()) {

                    ImGui::Text("%f %f", points[i].time, points[i].value);
                    ImGui::EndTooltip();
                }
            }


            if (draw_tangents) {
                auto tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent0.x, points[i].value + points[i].tangent0.y));
                drawlist->AddLine(ss_point, tan_point_ss, IM_COL32(200, 200, 200, 180));
                drawlist->AddCircleFilled(tan_point_ss, POINT_RADIUS_SS, IM_COL32(222, 222, 222, 255));
                tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent1.x, points[i].value + points[i].tangent1.y));
                drawlist->AddLine(ss_point, tan_point_ss, IM_COL32(200, 200, 200, 180));
                drawlist->AddCircleFilled(tan_point_ss, POINT_RADIUS_SS, IM_COL32(222, 222, 222, 255));
            }

            draw_rectangle_rotated(min, max, curvecol);

        }
    }
    if (ImGui::BeginPopup("point_popup")) {
        if (selected_curve == -1 || selected_curve >= curves.size()) {
            sys_print(Warning, "selected_curve == -1 or invalid in point_popup\n");
            set_selected_curve(-1);
            ImGui::CloseCurrentPopup();
        }
        else if (point_index_for_popup < 0 || point_index_for_popup >= curves[selected_curve].points.size()) {
            sys_print(Warning, "point_index_for_popup invalid\n");
            point_index_for_popup = -1;
            ImGui::CloseCurrentPopup();
        }
        else {
            auto& points = curves[selected_curve].points;
            auto enum_ =  EnumTrait<CurvePointType>::StaticType.find_for_value((int)points[point_index_for_popup].type);
            ASSERT(enum_);


            bool close_the_popup = false;
            if (ImGui::BeginCombo("##type", enum_->name)) {
                for (auto& enum_ : EnumTrait<CurvePointType>::StaticType) {
                    bool selected = enum_.value == (int)points[point_index_for_popup].type;
                    if (ImGui::Selectable(enum_.name, &selected)) {
                        points[point_index_for_popup].type = CurvePointType(enum_.value);
                        close_the_popup = true;
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Delete point")) {
                points.erase(points.begin() + point_index_for_popup);
                close_the_popup = true;
            }

            if (close_the_popup) {
                point_index_for_popup = -1;
                ImGui::CloseCurrentPopup();
            }

        }
        ImGui::EndPopup();
    }
    else
        point_index_for_popup = -1;

    if (dragging_point) {
        auto mousepos = ImGui::GetMousePos();
        auto gridspace = screenspace_to_grid(mousepos);
        if (selected_curve == -1 || selected_curve >= curves.size()) {
            sys_print(Warning, "dragging_point: selected_curve invalid\n");
            set_selected_curve(-1);
            dragged_point_index = -1;
            dragging_point = false;
        }
        else if (dragged_point_index < 0 || dragged_point_index >= curves[selected_curve].points.size()) {
            sys_print(Warning, "dragged_point_index invalid\n");
            dragged_point_index = -1;
            dragging_point = false;
        }
        else {
            clamp_point_to_grid(gridspace);
            auto& points = curves[selected_curve].points;
            auto& point = points[dragged_point_index];
            if (dragged_point_type == 0) {
                points[dragged_point_index].time = gridspace.x;
                points[dragged_point_index].value = gridspace.y;

                // sort points
                std::sort(points.begin(), points.end(), [](const auto& p, const auto& p2)->bool { return p.time < p2.time; });

                // FIXME:
                for (int i = 0; i < points.size(); i++) {
                    if (points[i].time == gridspace.x && points[i].value == gridspace.y) {
                        dragged_point_index = i;
                        break;
                    }
                }

            }
            else if (dragged_point_type == 1) {
                point.tangent0.x = gridspace.x - point.time; // because tangents are relative to point position
                point.tangent0.y = gridspace.y - point.value;
            }
            else if (dragged_point_type == 2) {
                point.tangent1.x = gridspace.x - point.time;
                point.tangent1.y = gridspace.y - point.value;
            }
            if (point.type == CurvePointType::Aligned) {
                if (dragged_point_type == 1) {
                    point.tangent1 = -point.tangent0;
                }
                else if (dragged_point_type == 2) {
                    point.tangent0 = -point.tangent1;
                }
            }
        }
    }


    if (ImGui::GetIO().MouseClicked[1] && is_window_focused_and_mouse_in_region && selected_curve != -1 && point_index_for_popup==-1/*hack*/) {
        ImGui::OpenPopup("curve_edit_popup");
        clickpos = screenspace_to_grid(ImGui::GetMousePos());
    }
    if (ImGui::BeginPopup("curve_edit_popup")) {
        if (selected_curve == -1 || selected_curve >= curves.size()) {
            sys_print(Warning, "selected_curve null in curve_edit_popup\n");
            ImGui::CloseCurrentPopup();

        }
        else {
            if (ImGui::Button("Add point")) {
                auto& points = curves[selected_curve].points;
                clamp_point_to_grid(clickpos);

               points.push_back({ clickpos.y,clickpos.x });


                std::sort(points.begin(), points.end(), [](const auto& p, const auto& p2)->bool { return p.time < p2.time; });
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }


    {

        auto origin_ss = grid_to_screenspace(ImVec2(0, 0));
        if (origin_ss.x - BASE_SCREENPOS.x >= 0) {
            drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(origin_ss.x, BASE_SCREENPOS.y + WINDOW_SIZE.y), edges.to_uint());
        }

        auto end_ss = grid_to_screenspace(ImVec2(max_x_value, 0));
        if (end_ss.x - BASE_SCREENPOS.x <= WINDOW_SIZE.x) {
            drawlist->AddRectFilled(ImVec2(end_ss.x, BASE_SCREENPOS.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
        }
    }

    // draw y after drawing mask boxes
    {
        int i = 0;
        for (float y = canvas_pos.y - fmod(grid_offset.y / base_scale.y * scale.y, grid_size.y); y < canvas_pos.y + canvas_size.y; y += grid_size.y) {
            drawlist->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), col_grid);
            drawlist->AddText(ImVec2(canvas_pos.x, y), IM_COL32(200, 200, 200, 255), string_format("%.1f", grid.y + dxdy_grid.y * i));
            for (int i = 1; i < subdivisions.y; i++) {
                drawlist->AddLine(ImVec2(canvas_pos.x, y + i * subgrid_size.y), ImVec2(canvas_pos.x + canvas_size.x, y + i * subgrid_size.y), col_subdiv);
            }
            i++;
        }
    }



    // middle mouse down
    if (ImGui::GetIO().MouseDown[2] && ImGui::IsWindowFocused()) {
        if (!started_pan)
            pan_start = screenspace_to_grid(ImGui::GetMousePos());
        started_pan = true;
        auto mousepose = ImGui::GetMousePos();
        auto in_gs = screenspace_to_grid(mousepose);
        // set offset such that pan_start == in_gs

        auto grid_wo_offset = screenspace_to_grid(mousepose) - grid_offset;

        grid_offset = pan_start - grid_wo_offset;
    }
    else
        started_pan = false;


    const float MOUSE_SCALE_EXP = 0.25;
    const float MIN_SCALE = 0.01;

    if (std::abs(ImGui::GetIO().MouseWheel) > 0.00001 && ImGui::IsWindowFocused()&&ImGui::IsWindowHovered()) {
        auto mousepos = ImGui::GetMousePos();
        auto start = screenspace_to_grid(mousepos);
        bool ctrl_is_down = ImGui::GetIO().KeyCtrl;

        float wh = ImGui::GetIO().MouseWheel;
        if (wh > 0) {
            if (ctrl_is_down)
                scale.y += scale.y * MOUSE_SCALE_EXP;
            else
                scale.x += scale.x * MOUSE_SCALE_EXP;
        }
        else {
            if (ctrl_is_down) {
                scale.y -= scale.y * MOUSE_SCALE_EXP;
                if (scale.y < MIN_SCALE) scale.y = MIN_SCALE;
            }
            else {
                scale.x -= scale.x * MOUSE_SCALE_EXP;
                if (scale.x < MIN_SCALE) scale.x = MIN_SCALE;
            }
        }
        // set grid offsets to maintain positon
        auto in_gs = screenspace_to_grid(mousepos);
        auto grid_wo_offset = screenspace_to_grid(mousepos) - grid_offset;
        grid_offset = start - grid_wo_offset;
    }

    if (!ImGui::GetIO().MouseDown[0])
        dragging_scrubber = false;

    // draw and update timeline scrubber
    if (ImRect(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, BASE_SCREENPOS.y + TIMELINE_HEIGHT)).Contains(ImGui::GetMousePos())
        && ImGui::IsWindowFocused() && !dragging_point) {
        if (ImGui::GetIO().MouseDown[0]) {
            dragging_scrubber = true;
        }
    }
    if (dragging_scrubber) {
        auto gridspace = screenspace_to_grid(ImGui::GetMousePos());
        current_time = gridspace.x;
        if (snap_scrubber_to_grid) {
            current_time = std::round(current_time / grid_snap_size.x) * grid_snap_size.x;
        }

        if (current_time < 0.0)
            current_time = 0.0;
        if (current_time > max_x_value)
            current_time = max_x_value;

        set_scrubber_this_frame = true;
    }
    {
        auto pos_of_scrubber = grid_to_screenspace(ImVec2(current_time, 0));
        const char* time_str = string_format("%.2f", current_time);
        auto& style = ImGui::GetStyle();
        float time_str_width = ImGui::CalcTextSize(time_str).x + style.FramePadding.x * 1.0f;
        float off = time_str_width * 0.5;
        drawlist->AddLine(ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y),ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y + WINDOW_SIZE.y), IM_COL32(27, 145, 247, 255),2.0);
        drawlist->AddRectFilled(ImVec2(pos_of_scrubber.x - off, BASE_SCREENPOS.y), ImVec2(pos_of_scrubber.x+off, BASE_SCREENPOS.y + TIMELINE_HEIGHT), IM_COL32(27, 145, 247,255),6.0);
        drawlist->AddText(ImVec2(pos_of_scrubber.x - off + 2.0, BASE_SCREENPOS.y), IM_COL32(255, 255, 255, 255), time_str);
    }

    // printf("%f\n",grid_offset)
    ImGui::PopClipRect();
}

void CurveEditorImgui::draw()
{
    set_scrubber_this_frame = false;
    if (!ImGui::Begin("curve edit")) {
        ImGui::End();
        return;
    }

   uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
       ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

   const int num_cols = 2;

   bool open_curve_popup = false;

   if (ImGui::BeginTable("curveedit", num_cols, ent_list_flags))
   {
       ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 100.0f, 0);
       ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

       ImGui::TableNextRow();
       ImGui::TableNextColumn();

       // 
       {
           uint32_t inner_flags =  
               ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable;
           if (ImGui::BeginTable("flags", 1, inner_flags))
           {
               ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 50.0f, 0);

               ImGui::TableNextRow();
               ImGui::TableNextColumn();

               if (ImGui::Button("Options.."))
                   ImGui::OpenPopup("curve_ed_options");
               if (ImGui::BeginPopup("curve_ed_options")) {
                   ImGui::Checkbox("Snap X to grid", &enable_grid_snapping_x);
                   if (enable_grid_snapping_x) {
                       ImGui::SameLine();
                       ImGui::InputFloat("##amtx", &grid_snap_size.x,0.1);
                       if (grid_snap_size.x < 0.001)
                           grid_snap_size.x = 0.001;
                   }
                   ImGui::Checkbox("Snap Y to grid", &enable_grid_snapping_y);
                   if (enable_grid_snapping_y) {
                       ImGui::SameLine();
                       ImGui::InputFloat("##amty", &grid_snap_size.y, 0.1);
                       if (grid_snap_size.y < 0.001)
                           grid_snap_size.y = 0.001;
                   }
                   ImGui::InputFloat("Max X", &max_x_value);
                   if (max_x_value <= 0.001) max_x_value = 0.001;
                   ImGui::InputFloat("Min Y", &min_y_value);
                   ImGui::InputFloat("Max Y", &max_y_value);
                   if (max_y_value <= min_y_value) max_y_value = min_y_value + 0.01;
                   ImGui::Checkbox("Snap Time to grid", &snap_scrubber_to_grid);
                   ImGui::EndPopup();
               }
               ImGui::Separator();


               for (int row_n = 0; row_n < curves.size(); row_n++)
               {
                   ImGui::TableNextRow();
                   ImGui::TableNextColumn();
                   auto& res = curves[row_n];

                   ImGui::PushID(res.curve_id);
                   ImGuiSelectableFlags selectable_flags =  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                   if (ImGui::Selectable("##selectednode", row_n == selected_curve, selectable_flags, ImVec2(0, 24))) {
                       set_selected_curve(row_n);
                   }
                   if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) {
                       open_curve_popup = true;
                       set_selected_curve(row_n);
                   }
                   
                   ImGui::SameLine();
                   ImGui::Checkbox("##check", &res.visible);
                   ImGui::SameLine();
                   ImGui::TextColored(color32_to_imvec4(res.color),res.name.c_str());
                   ImGui::PopID();
               }
               ImGui::TableNextRow();
               ImGui::TableNextColumn();

               ImGuiStyle& style = ImGui::GetStyle();
               float size = ImGui::CalcTextSize("Add row").x + style.FramePadding.x * 2.0f;
               float avail = ImGui::GetContentRegionAvail().x;
               float off = (avail - size) * 0.5;
               if (off > 0.0f)
                   ImGui::SetCursorPosX(ImGui::GetCursorPosX() + off);
               if (ImGui::Button("Add row")) {
                   EditingCurve ed;
                   ed.name = "New row";

                   add_curve(ed);
               }

               ImGui::EndTable();
           }
       }

       ImGui::TableNextColumn();

       // editor space
       BASE_SCREENPOS = ImGui::GetCursorScreenPos();
       WINDOW_SIZE = ImGui::GetContentRegionAvail();
        draw_editor_space();

       ImGui::EndTable();
   }
   if(open_curve_popup)
       ImGui::OpenPopup("curve_popup");
   if (ImGui::BeginPopup("curve_popup")) {
       if (selected_curve < 0 || selected_curve >= curves.size()) {
           sys_print(Warning, "curve_popup bad selected_curve\n");
           set_selected_curve(-1);
           ImGui::CloseCurrentPopup();
       }
       else {
           auto& curve = curves[selected_curve];
           
           ImGui::InputText("##name", (char*)curve.name.data(), curve.name.size() + 1/*null terminator, FIXME?*/, ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &curve.name);


           ImVec4 v4 = color32_to_imvec4(curve.color);
           ImGui::ColorEdit3("##color", &v4.x, ImGuiColorEditFlags_NoInputs);
           //fixme conversion stuff
           curve.color.r = v4.x * 255;
           curve.color.g = v4.y * 255;
           curve.color.b = v4.z * 255;

           if (ImGui::Button("Delete")) {
               curves.erase(curves.begin() + selected_curve);
               set_selected_curve(-1);
               ImGui::CloseCurrentPopup();
           }
       }
       ImGui::EndPopup();
   }


   ImGui::End();

}
class ClassBase;
class CurvePointSerialize : public IPropertySerializer
{
public:
    virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override {
        CurvePoint* point = (CurvePoint*)inst;
        std::string outs = string_format("\"%i %f %f %f %f %f %f\"", point->type, point->time, point->value, point->tangent0.x, point->tangent0.y, point->tangent1.x, point->tangent1.y);
        return outs;
    }
    virtual void unserialize(DictParser& in, const PropertyInfo& info,  void* inst, StringView token, ClassBase* user) override {
        std::string str = std::string(token.str_start, token.str_len);
        CurvePoint* point = (CurvePoint*)inst;
        int param = 0;
        int read = sscanf(str.c_str(), "%i %f %f %f %f %f %f", &param, &point->time, &point->value, &point->tangent0.x, &point->tangent0.y, &point->tangent1.x, &point->tangent1.y);
        if (param < 0 || param >= 4)param = 0;
        point->type = CurvePointType(param);
    }
};
#include "AddClassToFactory.h"
ADDTOFACTORYMACRO(CurvePointSerialize, IPropertySerializer);

#include "Framework/Curve.h"
#include "BinaryReadWrite.h"
void BakedCurve::write_to(FileWriter& out)
{
    out.write_int32(total_keyframes);
    out.write_float(total_length);

}
void BakedCurve::read_from(FileReader& in)
{

}
void BakedCurve::bake_from(const std::vector<EditingCurve>& curves, float max_time, float min_y, float frames_per_second)
{
    this->total_length = max_time;
    this->total_keyframes = max_time * frames_per_second;

    const float step = 1.0/frames_per_second;
    for (int curve_idx = 0; curve_idx < curves.size(); curve_idx++) {
        auto& curve = curves[curve_idx];
        auto& points = curve.points;
        // now to evaluate: xdddd
        struct outstruct {
            float time=0;
            float value=0;
            bool constant = false;
        };
        std::vector< outstruct> outsorted;
        for (int pointidx = 0; pointidx < points.size() - 1; pointidx++) {
            auto& point = points[pointidx];

            if (point.type == CurvePointType::Constant) {
                const float interval = points[pointidx + 1].time - point.time;
                for (float t = 0.0; t < interval; t += step) {
                    outsorted.push_back({ point.time + t, point.value,true });
                }
            }
            else if (point.type == CurvePointType::Linear) {
                const float interval = points[pointidx + 1].time - point.time;
                const float slope = (points[pointidx + 1].value - point.value) / interval;
                for (float t = 0.0; t < interval; t += step) {
                    outsorted.push_back({ point.time + t, point.value + slope * t });
                }
            }
            else if (point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned) {
                // now for the hacky part
                const int BEZIER_CURVE_SUBDIV = 30;
                glm::vec2 pointsout[BEZIER_CURVE_SUBDIV];
                glm::vec2 p0 = { point.time,point.value };
                glm::vec2 p1 = p0 + point.tangent1;
                glm::vec2 p3 = { points[pointidx + 1].time,points[pointidx + 1].value };
                glm::vec2 p2 = p3 + points[pointidx + 1].tangent0;
                for (int j = 0; j < BEZIER_CURVE_SUBDIV - 1; j++) {
                    float t = j / (float(BEZIER_CURVE_SUBDIV) - 1.0);
                    pointsout[j] = bezier_evaluate(t, p0, p1, p2, p3);
                    outsorted.push_back({ pointsout[j].x,pointsout[j].y });
                }
            }
        }
        if (!points.empty()) {
            auto& pointfront = points.front();
             float interval = pointfront.time - 0.0;
            for (float t = 0.0; t < interval; t += step) {
                outsorted.push_back({0.0f + t, pointfront.value });
            }

            auto& pointback = points.back();
             interval = max_time - pointback.time;
            for (float t = 0.0; t < interval; t += step) {
                outsorted.push_back({ pointback.time + t, pointback.value });
            }
        }
        std::vector<outstruct> outfinal;
        outfinal.resize(total_keyframes);
        std::sort(outsorted.begin(), outsorted.end(), [](const auto& p1, const auto& p2) -> bool { return p1.time < p2.time; });
        for (int i = 0; i < total_keyframes; i++) {
            const float time = i * frames_per_second;
            auto find = std::lower_bound(outsorted.begin(), outsorted.end(), time, [](const outstruct& p1,float time) -> bool { return p1.time < time; });
            if (find == outsorted.end()) {
                // set to last point value
                outfinal[i] = outsorted.front();
            }
            else {
                const int index = std::distance(outsorted.begin(), find);
                if (index > 0 && !find->constant) {
                    // lerp
                    float this_time = find->time;
                    float prev_time = outsorted[index - 1].time;
                    assert(prev_time < this_time);
                    float INTERP = (time - prev_time) / (this_time - prev_time);
                    float interped_val = glm::mix(outsorted[index - 1].value, find->value, INTERP);
                    outfinal[i] = { time,interped_val,false };
                }
                else
                    outfinal[i] = *find;
            }
        }



    }
}