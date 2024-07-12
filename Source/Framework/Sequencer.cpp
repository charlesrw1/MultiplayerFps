#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "CurveEditorImgui.h"

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
        *first_frame = ImClamp(*first_frame, GetFrameMin(), ImMax(GetFrameMax() - visibleFrameCount, GetFrameMin()));

    }
}
void SequencerImgui::draw_header(int first_frame, const float WIDTH, int* current_frame)
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
        float cursorOffset = canvas_pos.x + (*current_frame - firstFrameUsed) * frame_pixel_width - cursorWidth * 0.5f;
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
            *current_frame = (int)((io.MousePos.x - topRect.Min.x + frame_pixel_width * 0.5) / frame_pixel_width) + firstFrameUsed;
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
    else if (movingEntry != -1) {
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
                items[movingEntry]->time_end = frame - 1;
            }
            else
                items[movingEntry]->time_start = frame;
        }
    }

    for (int i = 0; i < items.size(); i++) {
        auto item = items[i].get();
        Color32 color = item->color;


        //size_t localCustomHeight = sequence->GetCustomHeight(i);

        ImVec2 pos = ImVec2(canvas_pos.x - firstFrame * frame_pixel_width, canvas_pos.y + item->track_index * item_height);

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

            draw_list->AddRectFilled(slotP1, slotP2, rect_color.to_uint(), 2);

            auto mousepos = ImGui::GetMousePos();
            ImRect rect(slotP1, slotP2);
            item_hovered = rect.Contains(mousepos);
            // printf("%d\n", (int)item_hovered);

            const float width = 6.0;

            ImVec2 handle_l_center = slotP1 + ImVec2(0, item_height * 0.5);
            left_hovered = ImLengthSqr(handle_l_center - mousepos) < width * width;

            bool is_color_white = left_hovered || (movingEntry == i && !moving_right_side);
            draw_rectangle_rotated(handle_l_center + ImVec2(width, width), handle_l_center - ImVec2(width, width), is_color_white ? COLOR_WHITE.to_uint() : color.to_uint());

            if (!item->instant_item) {
                ImVec2 handle_r_center = ImVec2(slotP2.x, slotP1.y) + ImVec2(0, item_height * 0.5);
                right_hovered = ImLengthSqr(handle_r_center - mousepos) < width * width;
                is_color_white = right_hovered || (movingEntry == i && moving_right_side);
                draw_rectangle_rotated(handle_r_center + ImVec2(width, width), handle_r_center - ImVec2(width, width), is_color_white ? COLOR_WHITE.to_uint() : color.to_uint());
            }
        }

        if (ImGui::GetIO().MouseClicked[0] && item_hovered && !left_hovered && !right_hovered && movingEntry == -1) {
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
            draw_list->AddText(ImVec2(slotP1.x + 6.0, slotP1.y), 0xffffffff, item->get_name().c_str());
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
