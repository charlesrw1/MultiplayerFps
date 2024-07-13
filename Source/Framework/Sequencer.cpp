#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
inline ImVec2 SequencerImgui::grid_to_screenspace(ImVec2 grid) const {
    return (grid - grid_offset) / base_scale * scale + BASE_SCREENPOS;
}

inline ImVec2 SequencerImgui::screenspace_to_grid(ImVec2 screen) const {
    return (screen - BASE_SCREENPOS) * base_scale * (ImVec2(1.0 / scale.x, 1.0 / scale.y)) + grid_offset;
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


bool track_overlaps(float start, float end, float s2, float end2)
{
    return start <= end2 && end >= s2;
}


bool SequencerImgui::draw_items()
{
    auto canvas_pos = ImGui::GetCursorScreenPos();

    const int num_rows = number_of_event_rows();
    bool hovered_any_items = false;
    const float item_height = 24.0;
    const float total_item_height = item_height * num_rows;
    auto draw_list = ImGui::GetWindowDrawList();
    const float HEADER_HEIGHT = 20.0;
    if (!ImGui::GetIO().MouseDown[0]) {
        is_dragging_selected = false;
    }

    if (is_dragging_selected) {
        if (selectedEntry == -1 || selectedEntry >= items.size()) {
            sys_print("??? is_dragging_slected null entry\n");
        }
        else {
            auto mousepos = ImGui::GetMousePos();
            auto gridspace = screenspace_to_grid(mousepos);

            clamp_point_to_grid(gridspace);

            auto& item = items[selectedEntry];
            
            if (moving_right_side) {
                item->time_end = gridspace.x;
            }
            else {
                item->time_start = gridspace.x;
            }
            if (!item->instant_item) {
                if (item->time_end <= item->time_start)
                    item->time_end = item->time_start + 1.0;
            }

            // find track index
            mousepos.y -= BASE_SCREENPOS.y;
            mousepos.y -= HEADER_HEIGHT;
            int track = std::floor(mousepos.y / item_height);
            item->track_index = track;
            if (item->track_index < 0)item->track_index = 0;
            if (item->track_index > 4)item->track_index = 4;

        }
    }

    for (int i = 0; i < items.size(); i++) {
        auto item = items[i].get();
        Color32 color = item->color;

        // x coord is in grid space, y is in screen space

        float x_start_ss = grid_to_screenspace(ImVec2(item->time_start, 0)).x;
        float x_end_ss = grid_to_screenspace(ImVec2(item->time_end, 0)).x;

        float y_start_ss = BASE_SCREENPOS.y + HEADER_HEIGHT + item->track_index * item_height;
        float y_end_ss = y_start_ss + item_height;

        if (item->instant_item) {
            auto size = ImGui::CalcTextSize(item->get_name().c_str());
            x_end_ss = x_start_ss + size.x + 8.0;
        }

        Color32 rect_color = color;
        rect_color.a = 128;
        if (selectedEntry == i)
            rect_color = { 252, 186, 3, 200 };
        
        draw_list->AddRectFilled(ImVec2(x_start_ss, y_start_ss), ImVec2(x_end_ss, y_end_ss), rect_color.to_uint(),2.0);

        {
            // check draggables
            const float HANDLE_SELECT_RADIUS = 8;
            const float HANDLE_RADIUS = 4;
            const ImVec2 HANDLE_RADIUS_V = ImVec2(HANDLE_RADIUS, HANDLE_RADIUS);

            const ImVec2 HANDLE_SELECT_RADIUS_V = ImVec2(HANDLE_SELECT_RADIUS, HANDLE_SELECT_RADIUS);
            ImVec2 lefthandle_c = ImVec2(x_start_ss, y_start_ss + item_height * 0.5);

            if (!is_dragging_selected &&
                ImRect(lefthandle_c - HANDLE_SELECT_RADIUS_V, lefthandle_c + HANDLE_SELECT_RADIUS_V).Contains(ImGui::GetMousePos())
                )
            {
                if (ImGui::GetIO().MouseClicked[0]) {
                    selectedEntry = i;
                    moving_right_side = false;
                    is_dragging_selected = true;
                }
            }
            {
                const bool is_select = is_dragging_selected && i == selectedEntry && !moving_right_side;
                draw_rectangle_rotated(lefthandle_c + HANDLE_RADIUS_V, lefthandle_c - HANDLE_RADIUS_V, is_select ? COLOR_WHITE.to_uint() : color.to_uint());
            }
            
            if (!item->instant_item) {
                ImVec2 righthandle_c = ImVec2(x_end_ss, y_start_ss + item_height * 0.5);
                if (!is_dragging_selected &&
                    ImRect(righthandle_c - HANDLE_SELECT_RADIUS_V, righthandle_c + HANDLE_SELECT_RADIUS_V).Contains(ImGui::GetMousePos())
                    )
                {
                    if (ImGui::GetIO().MouseClicked[0]) {
                        selectedEntry = i;
                        moving_right_side = true;
                        is_dragging_selected = true;
                    }
                }
                const bool is_select = is_dragging_selected && i == selectedEntry && moving_right_side;
                draw_rectangle_rotated(righthandle_c + HANDLE_RADIUS_V, righthandle_c - HANDLE_RADIUS_V, is_select ? COLOR_WHITE.to_uint() : color.to_uint());
            }

            if (!is_dragging_selected && ImRect(ImVec2(x_start_ss, y_start_ss), ImVec2(x_end_ss, y_end_ss)).Contains(ImGui::GetMousePos())) {
                if (ImGui::GetIO().MouseClicked[0]) {
                    selectedEntry = i;
                    is_dragging_selected = false;   // FALSE
                }
                hovered_any_items = true;
                if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
                    // set curve
                    selectedEntry = i;
                    ImGui::OpenPopup("item_popup");
                }
            }

            draw_list->AddText(ImVec2(x_start_ss+6.0, y_start_ss+1.0), COLOR_WHITE.to_uint(), item->get_name().c_str());

        }


    }
    if (ImGui::BeginPopup("item_popup")) {
        if (selectedEntry == -1 || selectedEntry >= items.size()) {
            sys_print("??? item_popup invalid item\n");
            selectedEntry = -1;
            ImGui::CloseCurrentPopup();
        }
        else {
            if (ImGui::Button("Delete")) {
                items.erase(items.begin() + selectedEntry); // calls destructor
                selectedEntry = -1;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    return hovered_any_items;
}
void SequencerImgui::draw()
{
    set_scrubber_this_frame = false;

    if (!ImGui::Begin("Timeline")) {
        ImGui::End();
        return;
    }

    // >>> COPIED FROM CURVE EDITOR
    BASE_SCREENPOS = ImGui::GetCursorScreenPos();
    WINDOW_SIZE = ImGui::GetContentRegionAvail();
 
    auto drawlist = ImGui::GetWindowDrawList();
    const Color32 background = { 36, 36, 36 };
    const Color32 edges = { 0, 0, 0, 128 };

    // draw background
    const float temp_ = WINDOW_SIZE.y;
    WINDOW_SIZE.y -= 20;
    WINDOW_SIZE.y = glm::max(WINDOW_SIZE.y, 0.1f);

    // top black bar for timeline
    const float TIMELINE_HEIGHT = 20.0;

    // Base background
    drawlist->AddRectFilled(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE, background.to_uint());


    // to prevent moving window
    ImGui::InvisibleButton("dummy234_", ImVec2(WINDOW_SIZE.x,
        std::max(temp_ - 5.0f, 0.01f) /* hacked value, not sure how to get the exact value, some padding variable likely*/));
    ImGui::PushClipRect(ImVec2(BASE_SCREENPOS), BASE_SCREENPOS + WINDOW_SIZE, true);

    const bool is_window_focused_and_mouse_in_region = ImGui::IsWindowFocused() && ImRect(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE).Contains(ImGui::GetMousePos());

    {

        auto min_ss = grid_to_screenspace(ImVec2(0, min_y_value));

        //if (min_ss.y - BASE_SCREENPOS.y <= WINDOW_SIZE.y) {
        //    drawlist->AddRectFilled(ImVec2(BASE_SCREENPOS.x, min_ss.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
        //}
        //auto max_ss = grid_to_screenspace(ImVec2(0, max_y_value));
        //if (max_ss.y - BASE_SCREENPOS.y >= 0) {
        //    drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, max_ss.y), edges.to_uint());
        //}


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
    float valx = BASE_SCREENPOS.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x);
    float valy = BASE_SCREENPOS.y - fmod(grid_offset.y / base_scale.y * scale.y, grid_size.y);

    const ImVec2 grid = screenspace_to_grid(ImVec2(valx, valy));
    {
        // X axis markings
        int i = 0;
        for (float x = BASE_SCREENPOS.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x); x < BASE_SCREENPOS.x + WINDOW_SIZE.x; x += grid_size.x) {
            drawlist->AddLine(ImVec2(x, BASE_SCREENPOS.y), ImVec2(x, BASE_SCREENPOS.y + WINDOW_SIZE.y), col_grid);
            drawlist->AddText(ImVec2(x, BASE_SCREENPOS.y), IM_COL32(150, 150, 150, 255), string_format("%.1f", grid.x + dxdy_grid.x * i));
            for (int i = 1; i < subdivisions.x; i++) {
                drawlist->AddLine(ImVec2(x + i * subgrid_size.x, BASE_SCREENPOS.y), ImVec2(x + i * subgrid_size.x, BASE_SCREENPOS.y + WINDOW_SIZE.y), col_subdiv);
            }
            i++;
        }
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

    // Dont draw horizontal markings for timeline!

   const bool was_any_hovered =  draw_items();


    // middle mouse down
    if (ImGui::GetIO().MouseDown[2] && ImGui::IsWindowFocused()) {
        if (!started_pan)
            pan_start = screenspace_to_grid(ImGui::GetMousePos());
        started_pan = true;
        auto mousepose = ImGui::GetMousePos();
        auto in_gs = screenspace_to_grid(mousepose);
        // set offset such that pan_start == in_gs

        auto grid_wo_offset = screenspace_to_grid(mousepose) - grid_offset;

        grid_offset.x = pan_start.x - grid_wo_offset.x;
    }
    else
        started_pan = false;

    const float MOUSE_SCALE_EXP = 0.25;
    const float MIN_SCALE = 0.01;

    if (std::abs(ImGui::GetIO().MouseWheel) > 0.00001 && ImGui::IsWindowFocused() && ImGui::IsWindowHovered()) {
        auto mousepos = ImGui::GetMousePos();
        auto start = screenspace_to_grid(mousepos);
        bool ctrl_is_down = ImGui::GetIO().KeyCtrl;

        float wh = ImGui::GetIO().MouseWheel;
        if (wh > 0) {
          //  if (ctrl_is_down)
       //         scale.y += scale.y * MOUSE_SCALE_EXP;
     //       else
                scale.x += scale.x * MOUSE_SCALE_EXP;
        }
        /* Dont allow Y scaling */
        else {
           // if (ctrl_is_down) {
           //     scale.y -= scale.y * MOUSE_SCALE_EXP;
           //     if (scale.y < MIN_SCALE) scale.y = MIN_SCALE;
           // }
             {
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
        && ImGui::IsWindowFocused()  /* && not dragging point !!*/) {
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
        drawlist->AddLine(ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y), ImVec2(pos_of_scrubber.x, BASE_SCREENPOS.y + WINDOW_SIZE.y), IM_COL32(27, 145, 247, 255), 2.0);
        drawlist->AddRectFilled(ImVec2(pos_of_scrubber.x - off, BASE_SCREENPOS.y), ImVec2(pos_of_scrubber.x + off, BASE_SCREENPOS.y + TIMELINE_HEIGHT), IM_COL32(27, 145, 247, 255), 6.0);
        drawlist->AddText(ImVec2(pos_of_scrubber.x - off + 2.0, BASE_SCREENPOS.y), IM_COL32(255, 255, 255, 255), time_str);
    }

    if (!dragging_scrubber && !is_dragging_selected && ImGui::IsWindowFocused() && !was_any_hovered && ImGui::GetIO().MouseClicked[1]) {
        ImGui::OpenPopup("creation_ctx_menu");
    }
    if (ImGui::BeginPopup("creation_ctx_menu")) {
        context_menu_callback();
        ImGui::EndPopup();
    }

    // printf("%f\n",grid_offset)
    ImGui::PopClipRect();

    ImGui::End();
}
