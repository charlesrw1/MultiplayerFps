#include "MyImguiLib.h"
#include "imgui_internal.h"

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