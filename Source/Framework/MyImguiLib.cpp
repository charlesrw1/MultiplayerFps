#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui.h"
#include "MyImguiLib.h"


void MyImSeperator(float x1, float x2, float width)
{
    using namespace ImGui;

    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;

    const float thickness = 1.0f; // Cannot use g.Style.SeparatorTextSize yet for various reasons.

    {
        // Horizontal Separator


        // We don't provide our width to the layout so that it doesn't get feed back into AutoFit
        // FIXME: This prevents ->CursorMaxPos based bounding box evaluation from working (e.g. TableEndCell)
        const float thickness_for_layout = (thickness == 1.0f) ? 0.0f : thickness; // FIXME: See 1.70/1.71 Separator() change: makes legacy 1-px separator not affect layout yet. Should change.
        const ImRect bb(ImVec2(x1+5.0, window->DC.CursorPos.y), ImVec2(x2-5.0, window->DC.CursorPos.y + width));
        ItemSize(ImVec2(0.0f, thickness_for_layout));
        

        const bool item_visible = ItemAdd(bb, 0);
        if (item_visible)
        {
            // Draw
            window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(ImGuiCol_Separator));
        }

    }
}

ImVec2 get_screen_pos(ImRect frame, ImVec2 param, ImVec2 min, ImVec2 max)
{
    ImVec2 width = max - min;
    ImVec2 normalized = (param - min) / width;
    ImVec2 screen = (frame.Max - frame.Min) * normalized + frame.Min;
    return screen;
}

bool MyImDrawBlendSpace(
    const char* label,
    const std::vector<ImVec2>& verts,
    const std::vector<int>& indicies,
    const std::vector<const char*>& vert_names,
    ImVec2 minbound,
    ImVec2 maxbound, ImVec2* hover_pos)
{
    assert(indicies.size() % 3 == 0);

    using namespace ImGui;

    ImVec2 size = ImVec2(maxbound.x - minbound.x, maxbound.y - minbound.y);
    float height_scale = size.y / size.x;

    ImGui::Text("placeholder");

    ImGuiWindow* window = GetCurrentWindow();
    auto& style = GetStyle();
    auto id = window->GetID(label);

    PushItemWidth(-FLT_MIN);

    float width = CalcItemWidth();
    float height = width * height_scale;
    const ImVec2 frame_size = ImVec2(width, height); // Arbitrary default of 8 lines high for multi-line

    const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, color32_to_imvec4({ 128,128,128 }));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
    bool child_visible = ImGui::BeginChildEx(label, id, frame_bb.GetSize(), true, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoMouseInputs);
  
    window = GetCurrentWindow();

    float border_width = 5.0;

    const ImRect sub_bb(window->Pos + ImVec2(border_width,border_width), window->Pos + window->Size - ImVec2(border_width,border_width));

    PopStyleVar(3);
    PopStyleColor();
    if (!child_visible)
    {
        EndChild();
        return false;
     }
    ImGui::EndChild();

    for (int i = 0; i < indicies.size(); i+=3) {
        ImVec2 p1 = get_screen_pos(sub_bb, verts[indicies[i]] , minbound,maxbound);
        ImVec2 p2 = get_screen_pos(sub_bb,verts[indicies[i+1]], minbound,maxbound);
        ImVec2 p3 = get_screen_pos(sub_bb,verts[indicies[i+2]], minbound,maxbound);

        window->DrawList->AddLine(p1, p2, color32_to_int({ 200,200,200 }));
        window->DrawList->AddLine(p2, p3, color32_to_int({ 200,200,200 }));
        window->DrawList->AddLine(p3, p1, color32_to_int({ 200,200,200 }));
    }

    auto restore_pos = GetCursorScreenPos();
    for (int i = 0; i < verts.size(); i++) {

        const char* name = vert_names[i];
        ImVec2 p = get_screen_pos(sub_bb, verts[i], minbound, maxbound);

        window->DrawList->AddCircleFilled(p,10, color32_to_int({ 5, 226, 255,160 }), 8);

        SetCursorScreenPos(p- ImVec2(10,10));
        ImGui::PushID(i);
        InvisibleButton("##blendbutton", ImVec2(20,20));
        if (IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped))
        {
            if (ImGui::BeginTooltip()) {
                ImGui::Text("%s\n( %.3f, %.3f )\n", name, verts[i].x, verts[i].y);
                EndTooltip();
            }
        }
        ImGui::PopID();

    }
    SetCursorScreenPos(restore_pos);

    return false;
}

#include "IEditorTool.h"
#include "Render/Texture.h"
#ifdef EDITOR_BUILD
bool my_imgui_image_button(const Texture* t, int size)
{
    ImVec2 sz_to_use(size, size);
    if (size == -1) {
        auto s = t->get_size();
        sz_to_use = ImVec2(s.x, s.y);
    }

    return (ImGui::ImageButton(ImTextureID(uint64_t(t->get_internal_render_handle())), sz_to_use));
}
void my_imgui_image(const Texture* t, int size)
{
    ImVec2 sz_to_use(size, size);
    if (size == -1) {
        auto s = t->get_size();
        sz_to_use = ImVec2(s.x, s.y);
    }
    ImGui::Image(ImTextureID(uint64_t(t->get_internal_render_handle())), sz_to_use);
}
ImGuiID dock_over_viewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags dockspace_flags,IEditorTool* tool, const ImGuiWindowClass* window_class)
{
    using namespace ImGui;

    if (viewport == NULL)
        viewport = GetMainViewport();

    SetNextWindowPos(viewport->WorkPos);
    SetNextWindowSize(viewport->WorkSize);
    SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        host_window_flags |= ImGuiWindowFlags_NoBackground;

    PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    Begin("MAIN DOCKWIN", NULL, host_window_flags);
    PopStyleVar(3);

    ImGuiID dockspace_id = GetID("DockSpace");
    DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, window_class);

    if (tool)
        tool->draw_menu_bar();

    End();

    return dockspace_id;
}
#endif