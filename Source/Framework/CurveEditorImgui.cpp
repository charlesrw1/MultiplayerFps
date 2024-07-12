
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"
#include "MyImguiLib.h"
#include <algorithm>
void CurveEditorImgui::update(float dt)
{
}

inline ImVec2 CurveEditorImgui::grid_to_screenspace(ImVec2 grid) const {
    return (grid - grid_offset) / base_scale*scale + BASE_SCREENPOS;
}

inline ImVec2 CurveEditorImgui::screenspace_to_grid(ImVec2 screen) const {
    return (screen - BASE_SCREENPOS) * base_scale * (ImVec2(1.0/scale.x,1.0/scale.y)) + grid_offset;
}




void SequencerImgui::draw_scrollbar(int* first_frame, float scrollbar_width, int current_frame)
{
    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    const ImVec2 scrollBarSize(scrollbar_width, 20.f);
    auto& io = ImGui::GetIO();
    auto draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("scrollBar", scrollBarSize);
    ImVec2 scrollBarMin = ImGui::GetItemRectMin();
    ImVec2 scrollBarMax = ImGui::GetItemRectMax();

    // ratio = number of frames visible in control / number to total frames

    const int frameCount = ImMax(GetFrameMax() - GetFrameMin(), 1);
    float startFrameOffset = ((float)(*first_frame - GetFrameMin()) / (float)frameCount) * (scrollbar_width);
    ImVec2 scrollBarA(scrollBarMin.x, scrollBarMin.y - 2);
    ImVec2 scrollBarB(scrollBarMin.x + scrollbar_width, scrollBarMax.y - 1);
    draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF222222, 0);

    ImRect scrollBarRect(scrollBarA, scrollBarB);
    bool inScrollBar = scrollBarRect.Contains(io.MousePos);

    draw_list->AddRectFilled(scrollBarA, scrollBarB, 0xFF101010, 8);

    const int visibleFrameCount = (int)floorf((scrollbar_width) / frame_pixel_width);
    const float barWidthRatio = ImMin(visibleFrameCount / (float)frameCount, 1.f);
    const float barWidthInPixels = barWidthRatio * (scrollbar_width);
    ImVec2 scrollBarC(scrollBarMin.x + startFrameOffset, scrollBarMin.y);
    ImVec2 scrollBarD(scrollBarMin.x + barWidthInPixels + startFrameOffset, scrollBarMax.y - 2);
    draw_list->AddRectFilled(scrollBarC, scrollBarD, (inScrollBar || MovingScrollBar) ? 0xFF606060 : 0xFF505050, 6);

    ImRect barHandleLeft(scrollBarC, ImVec2(scrollBarC.x + 14, scrollBarD.y));
    ImRect barHandleRight(ImVec2(scrollBarD.x - 14, scrollBarC.y), scrollBarD);

    bool onLeft = barHandleLeft.Contains(io.MousePos);
    bool onRight = barHandleRight.Contains(io.MousePos);

    ImRect scrollBarThumb(scrollBarC, scrollBarD);
    static const float MinBarWidth = 44.f;

    {
        if (MovingScrollBar)
        {
            if (!io.MouseDown[0])
            {
                MovingScrollBar = false;
            }
            else
            {
                float framesPerPixelInBar = barWidthInPixels / (float)visibleFrameCount;
                *first_frame = int((io.MousePos.x - panningViewSource.x) / framesPerPixelInBar) - panningViewFrame;
                *first_frame = ImClamp(*first_frame, GetFrameMin(), ImMax(GetFrameMax() - visibleFrameCount, GetFrameMin()));
            }
        }
        else
        {
            if (scrollBarThumb.Contains(io.MousePos) && ImGui::IsMouseClicked(0) && !MovingCurrentFrame)
            {
                MovingScrollBar = true;
                panningViewSource = io.MousePos;
                panningViewFrame = -*first_frame;
            }

        }
    }

    if (ImGui::IsWindowHovered())
    {
        float  frameOverCursor = *first_frame + (int)(visibleFrameCount * ((io.MousePos.x - canvas_pos.x) / (scrollbar_width)));
        //frameOverCursor = max(min(*firstFrame - visibleFrameCount / 2, frameCount - visibleFrameCount), 0);

        /**firstFrame -= frameOverCursor;
        *firstFrame *= framePixelWidthTarget / framePixelWidth;
        *firstFrame += frameOverCursor;*/
        if (io.MouseWheel < -FLT_EPSILON)
        {
            *first_frame -= frameOverCursor;
            *first_frame = int(*first_frame * 1.1f);
            frame_pixel_width_target *= 0.9f;
            *first_frame += frameOverCursor;
        }

        if (io.MouseWheel > FLT_EPSILON)
        {
            *first_frame -= frameOverCursor;
            *first_frame = int(*first_frame * 0.9f);
            frame_pixel_width_target *= 1.1f;
            *first_frame += frameOverCursor;
        }
        *first_frame = ImClamp(*first_frame, GetFrameMin(), ImMax(GetFrameMax() - visibleFrameCount,GetFrameMin()));

    }
}
void SequencerImgui::draw_header( int first_frame, const float WIDTH, int* current_frame)
{
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const int ItemHeight = 20;

    const float header_height = 20.f;

    const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();

    ImGui::InvisibleButton("headerregion", ImVec2(WIDTH, header_height));

    //header frame number and lines
    int modFrameCount = 10;
    int frameStep = 1;
    while ((modFrameCount * frame_pixel_width) < 150)
    {
        modFrameCount *= 2;
        frameStep *= 2;
    };
    int halfModFrameCount = modFrameCount / 2;

    auto drawLine = [&](int i, int regionHeight) {
        bool baseIndex = ((i % modFrameCount) == 0) || (i == GetFrameMax() || i == GetFrameMin());
        bool halfIndex = (i % halfModFrameCount) == 0;
        int px = (int)canvas_pos.x + int(i * frame_pixel_width) /* + legendWidth*/ - int(first_frame * frame_pixel_width);
        int tiretStart = baseIndex ? 4 : (halfIndex ? 10 : 14);
        int tiretEnd = baseIndex ? regionHeight : ItemHeight;

        if (px <= (WIDTH + canvas_pos.x) && px >= (canvas_pos.x /* + legendWidth */))
        {
            draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), 0xFF606060, 1);

            draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)ItemHeight), ImVec2((float)px, canvas_pos.y + (float)regionHeight - 1), 0x30606060, 1);
        }

        if (baseIndex && px > (canvas_pos.x /* + legendWidth*/))
        {
            char tmps[512];
            ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", i);
            draw_list->AddText(ImVec2((float)px + 3.f, canvas_pos.y), 0xFFBBBBBB, tmps);
        }

    };

    auto drawLineContent = [&](int i, int /*regionHeight*/) {
        int px = (int)canvas_pos.x + int(i * frame_pixel_width) /* + legendWidth*/ - int(first_frame * frame_pixel_width);
        int tiretStart = int(canvas_pos.y);
        int tiretEnd = int(canvas_pos.y) + header_height;

        if (px <= (WIDTH + canvas_pos.x) && px >= (canvas_pos.x/* + legendWidth*/))
        {
            //draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y + (float)tiretEnd - 1), 0xFF606060, 1);

            draw_list->AddLine(ImVec2(float(px), float(tiretStart)), ImVec2(float(px), float(tiretEnd)), 0x30606060, 1);
        }
    };
    for (int i = GetFrameMin(); i <= GetFrameMax(); i += frameStep)
    {
        drawLine(i, ItemHeight);
    }
    drawLine(GetFrameMin(), ItemHeight);
    drawLine(GetFrameMax(), ItemHeight);
    const int firstFrameUsed = first_frame;
    // cursor
    if (*current_frame >= first_frame && *current_frame <= GetFrameMax())
    {
        static const float cursorWidth = 4.f;
        float cursorOffset = canvas_pos.x + (*current_frame - firstFrameUsed) * frame_pixel_width  - cursorWidth * 0.5f;
        draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y), ImVec2(cursorOffset, canvas_pos.y + 400.f), 0xA02A2AFF, cursorWidth);
        char tmps[512];
        ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", *current_frame);
        draw_list->AddText(ImVec2(cursorOffset + 10, canvas_pos.y + 2), 0xFF2A2AFF, tmps);
    }

    ImRect topRect(ImVec2(canvas_pos.x, canvas_pos.y), ImVec2(canvas_pos.x + WIDTH, canvas_pos.y + header_height));
    auto& io = ImGui::GetIO();
    if (!MovingCurrentFrame && !MovingScrollBar && *current_frame >= 0 && topRect.Contains(ImGui::GetIO().MousePos) && ImGui::GetIO().MouseDown[0])
    {
        MovingCurrentFrame = true;
    }
    if (MovingCurrentFrame)
    {
        const  int frameCount = ImMax(GetFrameMax() - GetFrameMin(), 1);
        if (frameCount)
        {
            *current_frame = (int)((io.MousePos.x - topRect.Min.x+frame_pixel_width*0.5) / frame_pixel_width) + firstFrameUsed;
            if (*current_frame < GetFrameMin())
                *current_frame = GetFrameMin();
            if (*current_frame >= GetFrameMax())
                *current_frame = GetFrameMax();
        }
        if (!io.MouseDown[0])
            MovingCurrentFrame = false;
    }
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


void SequencerImgui::draw_items(const float timeline_width)
{
    auto canvas_pos = ImGui::GetCursorScreenPos();

    const int num_rows = number_of_event_rows();

    const float item_height = 24.0;
    const float total_item_height = item_height * num_rows;
    auto draw_list = ImGui::GetWindowDrawList();

    ImGui::InvisibleButton("dummy234", ImVec2(timeline_width, total_item_height));
    ImGui::PushClipRect(ImVec2(canvas_pos.x, canvas_pos.y), canvas_pos + ImVec2(timeline_width, total_item_height), true);


    if (movingEntry != -1 && !ImGui::GetIO().MouseDown[0]) {
        movingEntry = -1;
        moving_right_side = false;
    }
    else if (movingEntry!=-1) {
        const  int frameCount = ImMax(GetFrameMax() - GetFrameMin(), 1);
        if (frameCount)
        {
            int frame = 0;

            frame = (int)((ImGui::GetIO().MousePos.x - canvas_pos.x + frame_pixel_width * 0.5) / frame_pixel_width) + firstFrame;
            if (frame < GetFrameMin())
                frame = GetFrameMin();
            if (frame >= GetFrameMax())
                frame = GetFrameMax();

            int track = 0;
            track = (int)((ImGui::GetIO().MousePos.y - canvas_pos.y) / item_height);
            const int num_rows_ex = number_of_event_rows_exclude(movingEntry);
            if (track < 0)
                track = 0;
            if (track > num_rows_ex)
                track = num_rows_ex;
            items[movingEntry]->track_index = track;

            if (moving_right_side) {
                items[movingEntry]->time_end = frame-1;
            }
            else
                items[movingEntry]->time_start = frame;
        }
    }

    for (int i = 0; i < items.size(); i++) {
        auto item = items[i].get();
        Color32 color = item->color;
        

        //size_t localCustomHeight = sequence->GetCustomHeight(i);

        ImVec2 pos = ImVec2(canvas_pos.x - firstFrame * frame_pixel_width, canvas_pos.y + item->track_index*item_height);
        
        ImVec2 slotP1(pos.x + item->time_start * frame_pixel_width, pos.y + 2);
        ImVec2 slotP2;
        if (item->instant_item) {
            auto size = ImGui::CalcTextSize(item->get_name().c_str());
            slotP2 = ImVec2(slotP1.x + size.x + 6.0, pos.y + item_height - 1);
        }
        else {
            slotP2 = ImVec2(pos.x + item->time_end * frame_pixel_width + frame_pixel_width, pos.y + item_height - 1);
        }
    
        bool left_hovered = false;
        bool right_hovered = false;
        bool item_hovered = false;
        if (slotP1.x <= (canvas_pos.x + timeline_width) && slotP2.x >= (canvas_pos.x))
        {
            //draw_list->AddRectFilled(slotP1, slotP3, slotColorHalf, 2);


            Color32 rect_color = color;
            rect_color.a = 128;
            if (selectedEntry == i)
                rect_color = { 252, 186, 3, 128 };

            draw_list->AddRectFilled(slotP1, slotP2, rect_color.to_uint() , 2);

            auto mousepos = ImGui::GetMousePos();
            ImRect rect(slotP1, slotP2);
            item_hovered = rect.Contains(mousepos);
           // printf("%d\n", (int)item_hovered);

            const float width = 6.0;

            ImVec2 handle_l_center = slotP1 + ImVec2(0, item_height * 0.5);
            left_hovered = ImLengthSqr(handle_l_center - mousepos) < width* width;

            bool is_color_white = left_hovered || (movingEntry == i && !moving_right_side);
            draw_rectangle_rotated(handle_l_center + ImVec2(width, width), handle_l_center - ImVec2(width, width), is_color_white ? COLOR_WHITE.to_uint() : color.to_uint());
        
            if (!item->instant_item) {
                ImVec2 handle_r_center = ImVec2(slotP2.x,slotP1.y) + ImVec2(0, item_height * 0.5);
                right_hovered = ImLengthSqr(handle_r_center - mousepos) < width * width;
                is_color_white = right_hovered || (movingEntry == i && moving_right_side);
                draw_rectangle_rotated(handle_r_center + ImVec2(width, width), handle_r_center - ImVec2(width, width), is_color_white ? COLOR_WHITE.to_uint() : color.to_uint());
            }
        }

        if (ImGui::GetIO().MouseClicked[0] && item_hovered && !left_hovered && !right_hovered && movingEntry==-1) {
            selectedEntry = i;
        }
        else if (movingEntry == -1 && left_hovered && ImGui::GetIO().MouseDown[0]) {
            movingEntry = i;
            moving_right_side = false;
        }
        else if (movingEntry == -1 && right_hovered && ImGui::GetIO().MouseDown[0]) {
            movingEntry = i;
            moving_right_side = true;
        }

      
        // Ensure grabbable handles
        const float max_handle_width = slotP2.x - slotP1.x / 3.0f;
        const float min_handle_width = ImMin(10.0f, max_handle_width);
        const float handle_width = ImClamp(frame_pixel_width / 2.0f, min_handle_width, max_handle_width);
        ImRect rects[3] = { ImRect(slotP1, ImVec2(slotP1.x + handle_width, slotP2.y))
            , ImRect(ImVec2(slotP2.x - handle_width, slotP1.y), slotP2)
            , ImRect(slotP1, slotP2) };

        if (slotP1.x <= (canvas_pos.x + timeline_width) && slotP2.x >= (canvas_pos.x))
        {
            draw_list->AddText(ImVec2(slotP1.x+6.0, slotP1.y), 0xffffffff, item->get_name().c_str());
        }

    }


    ImGui::PopClipRect();

}
void SequencerImgui::draw()
{

    if (!ImGui::Begin("Timeline")) {
        ImGui::End();
        return;
    }

    ImGui::PushItemWidth(100);
    ImGui::InputInt("Frame Min", &frameMin);
    ImGui::SameLine();
    ImGui::InputInt("Frame ", &currentFrame);
    ImGui::SameLine();
    ImGui::InputInt("Frame Max", &frameMax);

    frame_pixel_width_target = ImClamp(frame_pixel_width_target, 0.1f, 50.f);
    frame_pixel_width = ImLerp(frame_pixel_width, frame_pixel_width_target, 0.33f);

    ImGui::BeginGroup();
    const  uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
        ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
    float TIMELINEWIDTH = 0.0;
    if (ImGui::BeginTable("SequencerTable", 2, ent_list_flags))
    {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        TIMELINEWIDTH = ImGui::GetColumnWidth();

        draw_header(firstFrame, TIMELINEWIDTH, &currentFrame);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();

        ImGui::Text("Events");
        ImGui::TableNextColumn();
        draw_items(TIMELINEWIDTH);

        ImGui::EndTable();
    }

    // synced table for scrollbar
    if (ImGui::BeginTable("SequencerTable", 2, ent_list_flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TableNextColumn();

        draw_scrollbar(&firstFrame, TIMELINEWIDTH, currentFrame);

        ImGui::EndTable();
    }
    ImGui::EndGroup();

    ImGui::End();
}
void DrawGrid(ImDrawList* draw_list, const ImVec2& canvas_pos, const ImVec2& canvas_size, float zoom) {
   
    ImU32 col_grid = IM_COL32(255, 50, 50, 40);
    ImU32 col_subdiv = IM_COL32(200, 200, 200, 20);

    zoom = std::max(zoom, 0.001f);

    float base_grid_size = 400;
    float grid_size = base_grid_size * zoom;

    // Calculate the number of subdivisions based on zoom level
    const int subdivision = 5;
    const float subdiv_size = (2);
    const float inv_subdiv_size = 1.0 / subdiv_size;


    // want to find right grid size

    // optimal size: 
    if (grid_size < base_grid_size) {
        // too small
        while (grid_size < base_grid_size* inv_subdiv_size)
            grid_size *= subdiv_size;
    }
    else {
        while (grid_size > base_grid_size)
            grid_size /= subdiv_size;
    }
    float subgrid_size = grid_size / subdivision;


    for (float x = canvas_pos.x; x < canvas_pos.x + canvas_size.x; x += grid_size) {
        draw_list->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y), col_grid);
        for (int i = 1; i < subdivision; i++) {
            draw_list->AddLine(ImVec2(x + i * subgrid_size, canvas_pos.y), ImVec2(x + i * subgrid_size, canvas_pos.y + canvas_size.y), col_subdiv);
        }
    }

    for (float y = canvas_pos.y; y < canvas_pos.y + canvas_size.y; y += grid_size) {
        draw_list->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), col_grid);
        for (int i = 1; i < subdivision; i++) {
            draw_list->AddLine(ImVec2(canvas_pos.x, y + i * subgrid_size), ImVec2(canvas_pos.x + canvas_size.x, y + i * subgrid_size), col_subdiv);
        }
    }
}

// catlum rom spline evalutation https://iquilezles.org/articles/minispline/

static signed char coefs[16] = {
    -1, 2,-1, 0,
     3,-5, 0, 2,
    -3, 4, 1, 0,
     1,-1, 0, 0 };

void spline(const float* key, int num, int dim, float t, float* v)
{
    const int size = dim + 1;

    // find key
    int k = 0; while (key[k * size] < t) k++;

    // interpolant
    const float h = (t - key[(k - 1) * size]) / (key[k * size] - key[(k - 1) * size]);

    // init result
    for (int i = 0; i < dim; i++) v[i] = 0.0f;

    // add basis functions
    for (int i = 0; i < 4; i++)
    {
        int kn = k + i - 2; if (kn < 0) kn = 0; else if (kn > (num - 1)) kn = num - 1;
        const signed char* co = coefs + 4 * i;
        const float b = 0.5f * (((co[0] * h + co[1]) * h + co[2]) * h + co[3]);
        for (int j = 0; j < dim; j++) v[j] += b * key[kn * size + j + 1];
    }
}

glm::vec2 bezier_evaluate(float t, const glm::vec2 p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3)
{
    auto a = (1 - t) * (1 - t) * (1 - t) * p0;
    auto b = 3 * t * (1 - t) * (1 - t) * p1;
    auto c = 3 * t * t * (1 - t) * p2;
    auto d = t * t * t * p3;
    return a + b + c + d;
}

ENUM_START(CurvePointType)
    STRINGIFY_EUNM(CurvePointType::Linear,0),
    STRINGIFY_EUNM(CurvePointType::Constant, 1),
    STRINGIFY_EUNM(CurvePointType::Auto, 2),
    STRINGIFY_EUNM(CurvePointType::SplitTangents, 3),
    STRINGIFY_EUNM(CurvePointType::Aligned, 4)
ENUM_IMPL(CurvePointType)

void CurveEditorImgui::draw_editor_space()
{
    auto drawlist = ImGui::GetWindowDrawList();
    const Color32 background = { 36, 36, 36 };
    const Color32 edges = { 0, 0, 0, 128 };

    // draw background

    drawlist->AddRectFilled(BASE_SCREENPOS, BASE_SCREENPOS + WINDOW_SIZE, background.to_uint());

    ImVec2 BASE_GRIDSPACE = screenspace_to_grid(BASE_SCREENPOS);
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();


    // to prevent moving window
    ImGui::InvisibleButton("dummy234_", ImVec2(WINDOW_SIZE.x,
        std::max(WINDOW_SIZE.y - 36.0, 0.01) /* hacked value, not sure how to get the exact value, some padding variable likely*/));

    {
        auto min_ss = grid_to_screenspace(ImVec2(0, MIN_VALUE));
        if (min_ss.y - BASE_SCREENPOS.y >= 0) {
            drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, min_ss.y), edges.to_uint());
        }
        auto max_ss = grid_to_screenspace(ImVec2(0, MAX_VALUE));
        if (max_ss.y - BASE_SCREENPOS.y <= WINDOW_SIZE.y) {
            drawlist->AddRectFilled(ImVec2(BASE_SCREENPOS.x, max_ss.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
        }
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
    int i = 0;
    for (float x = canvas_pos.x - fmod(grid_offset.x / base_scale.x * scale.x, grid_size.x); x < canvas_pos.x + canvas_size.x; x += grid_size.x) {
        drawlist->AddLine(ImVec2(x, canvas_pos.y), ImVec2(x, canvas_pos.y + canvas_size.y), col_grid);
        drawlist->AddText(ImVec2(x, canvas_pos.y), IM_COL32(150, 150, 150, 255), string_format("%.1f", grid.x + dxdy_grid.x * i));
        for (int i = 1; i < subdivisions.x; i++) {
            drawlist->AddLine(ImVec2(x + i * subgrid_size.x, canvas_pos.y), ImVec2(x + i * subgrid_size.x, canvas_pos.y + canvas_size.y), col_subdiv);
        }
        i++;
    }




    // draw/update points

    ImU32 curvecol = curve.color.to_uint();
    auto& points = curve.thecurve.points;

    // draw the points
    for (int i = 0; i < (int)curve.thecurve.points.size() - 1; i++) {
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
        else if (point.type == CurvePointType::Auto) {

            float data[12] = {
               -1.0,points[i - 1].time, points[i - 1].value,
               0.0,points[i].time,points[i].value,
               1.0,points[i + 1].time,points[i + 1].value,
               2.0,points[i + 2].time,points[i + 2].value
            };
            const int subdivisions = 20;
            glm::vec2 points[subdivisions];

            for (int j = 0; j < subdivisions; j++) {
                float t = j / (float(subdivisions) - 1.0);
                spline(data, 12, 2, t, (float*)&points[j]);
            }

            for (int j = 0; j < subdivisions - 1; j++) {
                drawlist->AddLine(
                    grid_to_screenspace(ImVec2(points[j].x, points[j].y)),
                    grid_to_screenspace(ImVec2(points[j + 1].x, points[j + 1].y)),
                    IM_COL32(255, 0, 128, 255));
            }
        }
        else if (point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned) {
            const int subdivisions = 20;
            glm::vec2 pointsout[subdivisions];
            glm::vec2 p0 = { point.time,point.value };
            glm::vec2 p1 = p0 + point.tangent1;
            glm::vec2 p3 = { points[i + 1].time,points[i + 1].value };
            glm::vec2 p2 = p3 + points[i + 1].tangent0;
            for (int j = 0; j < subdivisions; j++) {
                float t = j / (float(subdivisions) - 1.0);
                pointsout[j] = bezier_evaluate(t, p0, p1, p2, p3);
            }
            for (int j = 0; j < subdivisions - 1; j++) {
                drawlist->AddLine(
                    grid_to_screenspace(ImVec2(pointsout[j].x, pointsout[j].y)),
                    grid_to_screenspace(ImVec2(pointsout[j + 1].x, pointsout[j + 1].y)),
                    curvecol);
            }
        }
    }

    if (!points.empty()) {
        auto pointfront = points.front();
        auto ss_start = grid_to_screenspace(ImVec2(pointfront.time, pointfront.value));
        auto ss_end = ImVec2(BASE_SCREENPOS.x, ss_start.y);
        drawlist->AddLine(ss_start, ss_end, curvecol);
        auto& pointback = points.back();
        ss_start = grid_to_screenspace(ImVec2(pointback.time, pointback.value));
        ss_end = ImVec2(BASE_SCREENPOS.x + WINDOW_SIZE.x, ss_start.y);
        drawlist->AddLine(ss_start, ss_end, curvecol);
    }


    static bool dragging_point = false;
    static int dragged_point_index = -1;
    static int dragged_point_type = 0;   // 0 = point, 1=tangent0,2=tangent1
    if (dragging_point && !ImGui::GetIO().MouseDown[0]) {
        dragged_point_index = -1;
        dragged_point_type = 0;
        dragging_point = false;
    }

    static int point_index_for_popup = -1;
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
        if (i == dragged_point_index)
            draw_tooltip = true;
        if (ImRect(min_sel, max_sel).Contains(ImGui::GetMousePos())) {
            if (dragged_point_index == -1)
                draw_tooltip = true;

            if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
                dragged_point_index = i;
                dragged_point_type = 0;  // point type
                dragging_point = true;
            }

            if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
                ImGui::OpenPopup("point_popup");
                point_index_for_popup = i;
            }
        }
        const bool draw_tangents = point.type == CurvePointType::SplitTangents || point.type == CurvePointType::Aligned;
        if (draw_tangents) {
            auto tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent0.x, points[i].value + points[i].tangent0.y));
            ImVec2 min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
            ImVec2 max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

            if (ImRect(min_sel_tan, max_sel_tan).Contains(ImGui::GetMousePos())) {

                if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
                    dragged_point_index = i;
                    dragged_point_type = 1;  // tangent0 type
                    dragging_point = true;
                }
            }

            tan_point_ss = grid_to_screenspace(ImVec2(points[i].time + points[i].tangent1.x, points[i].value + points[i].tangent1.y));
            min_sel_tan = tan_point_ss - ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);
            max_sel_tan = tan_point_ss + ImVec2(POINT_SELECTION_RADIUS, POINT_SELECTION_RADIUS);

            if (ImRect(min_sel_tan, max_sel_tan).Contains(ImGui::GetMousePos())) {

                if (ImGui::GetIO().MouseDown[0] && ImGui::IsWindowFocused()) {
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

    if (ImGui::BeginPopup("point_popup")) {
        if (point_index_for_popup < 0 || point_index_for_popup >= points.size()) {
            sys_print("??? point_index_for_popup invalid\n");
            point_index_for_popup = -1;
            ImGui::CloseCurrentPopup();
        }
        else {
            std::string current_type = EnumTrait<CurvePointType>::StaticType.get_enum_str((int)points[point_index_for_popup].type);
            current_type = current_type.substr(current_type.rfind("::") + 2);

            bool close_the_popup = false;
            if (ImGui::BeginCombo("##type", current_type.c_str())) {
                for (int i = 0; i < EnumTrait<CurvePointType>::StaticType.str_count; i++) {
                    std::string str = EnumTrait<CurvePointType>::StaticType.get_enum_str(i);
                    str = str.substr(str.rfind("::") + 2);
                    bool selected = i == (int)points[point_index_for_popup].type;
                    if (ImGui::Selectable(str.c_str(), &selected)) {
                        points[point_index_for_popup].type = CurvePointType(i);
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

    if (dragging_point) {
        auto mousepos = ImGui::GetMousePos();
        auto gridspace = screenspace_to_grid(mousepos);

        if (dragged_point_index < 0 || dragged_point_index >= points.size()) {
            sys_print("??? dragged_point_index invalid\n");
            dragged_point_index = -1;
            dragging_point = false;
        }
        else {
            clamp_point_to_grid(gridspace);
            auto& point = points[dragged_point_index];
            if (dragged_point_type == 0) {
                points[dragged_point_index].time = gridspace.x;
                points[dragged_point_index].value = gridspace.y;
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


    static ImVec2 clickpos;
    if (ImGui::GetIO().MouseClicked[1] && ImGui::IsWindowFocused()) {
        ImGui::OpenPopup("curve_edit_popup");
        clickpos = screenspace_to_grid(ImGui::GetMousePos());
    }
    if (ImGui::BeginPopup("curve_edit_popup")) {
        if (ImGui::Button("Add point")) {

            clamp_point_to_grid(clickpos);

            curve.thecurve.points.push_back({ clickpos.y,clickpos.x });
            auto& points = curve.thecurve.points;

            std::sort(points.begin(), points.end(), [](const auto& p, const auto& p2)->bool { return p.time < p2.time; });
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }


    {

        auto origin_ss = grid_to_screenspace(ImVec2(0, 0));
        if (origin_ss.x - BASE_SCREENPOS.x >= 0) {
            drawlist->AddRectFilled(BASE_SCREENPOS, ImVec2(origin_ss.x, BASE_SCREENPOS.y + WINDOW_SIZE.y), edges.to_uint());
        }

        auto end_ss = grid_to_screenspace(ImVec2(MAX_TIME, 0));
        if (end_ss.x - BASE_SCREENPOS.x <= WINDOW_SIZE.x) {
            drawlist->AddRectFilled(ImVec2(end_ss.x, BASE_SCREENPOS.y), BASE_SCREENPOS + WINDOW_SIZE, edges.to_uint());
        }
    }

    // draw y after drawing mask boxes
    i = 0;
    for (float y = canvas_pos.y - fmod(grid_offset.y / base_scale.y * scale.y, grid_size.y); y < canvas_pos.y + canvas_size.y; y += grid_size.y) {
        drawlist->AddLine(ImVec2(canvas_pos.x, y), ImVec2(canvas_pos.x + canvas_size.x, y), col_grid);
        drawlist->AddText(ImVec2(canvas_pos.x, y), IM_COL32(200, 200, 200, 255), string_format("%.1f", grid.y + dxdy_grid.y * i));
        for (int i = 1; i < subdivisions.y; i++) {
            drawlist->AddLine(ImVec2(canvas_pos.x, y + i * subgrid_size.y), ImVec2(canvas_pos.x + canvas_size.x, y + i * subgrid_size.y), col_subdiv);
        }
        i++;
    }



    // middle mouse down
    static bool started_pan = false;
    static ImVec2 pan_start = {};
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

    if (std::abs(ImGui::GetIO().MouseWheel) > 0.00001 && ImGui::IsWindowFocused()) {
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
    // printf("%f\n",grid_offset)

}

void CurveEditorImgui::draw()
{

    if (!ImGui::Begin("curve edit")) {
        ImGui::End();
        return;
    }

   uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
       ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;

   const int num_cols = 2;

   if (ImGui::BeginTable("curveedit", num_cols, ent_list_flags))
   {
       ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
       ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

       ImGui::TableNextRow();
       ImGui::TableNextColumn();

       // 
       {
           uint32_t inner_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
               ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
           if (ImGui::BeginTable("flags", 1, inner_flags))
           {
               ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthFixed, 50.0f, 0);
               ImGui::TableNextRow();
               ImGui::TableNextColumn();
               for (int row_n = 0; row_n < curves.size(); row_n++)
               {
                   auto& res = curves[row_n];

                   ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
                   if (ImGui::Selectable("##selectednode", false, selectable_flags, ImVec2(0, 0))) {

                   }

                   ImGui::SameLine();
                   ImGui::Text(res.name.c_str());
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


   ImGui::End();

}