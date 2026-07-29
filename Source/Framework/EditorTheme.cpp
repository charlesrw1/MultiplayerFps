#include "EditorTheme.h"
#include "imgui.h"

ImFont* g_prop_bold_font    = nullptr;
ImFont* g_prop_regular_font = nullptr;

// Colour palette — edit these to retune the whole UI at once
namespace theme {
    // Bases — blue-tinted slate charcoal (Autodesk Revit dark mode)
    static constexpr ImVec4 bg_deep    = {0.086f,0.094f,0.110f,1.f}; // #16181c  deepest bg / canvas
    static constexpr ImVec4 bg_panel   = {0.188f,0.200f,0.227f,1.f}; // #30333a  panel / window
    static constexpr ImVec4 bg_widget  = {0.133f,0.141f,0.161f,1.f}; // #22242a  input / frame bg
    static constexpr ImVec4 bg_popup   = {0.227f,0.239f,0.267f,1.f}; // #3a3d44  popup / menu bg
    static constexpr ImVec4 bg_header  = {0.227f,0.239f,0.267f,1.f}; // #3a3d44  collapsing header
    static constexpr ImVec4 bg_row_alt = {1.00f, 1.00f, 1.00f,0.04f};// faint zebra stripe

    // Borders
    static constexpr ImVec4 border     = {0.290f,0.306f,0.337f,1.f}; // #4a4e56

    // Text
    static constexpr ImVec4 text       = {0.831f,0.839f,0.851f,1.f}; // #d4d6d9  body
    static constexpr ImVec4 text_dim   = {0.549f,0.561f,0.580f,1.f}; // #8c8f94  disabled

    // Accent — Revit selection/highlight sky-blue.
    static constexpr ImVec4 accent      = {0.239f,0.647f,0.851f,1.f}; // #3DA5D9
    static constexpr ImVec4 accent_dim  = {0.122f,0.290f,0.388f,1.f}; // #1F4A63  selection bg
    static constexpr ImVec4 accent_hot  = {0.357f,0.722f,0.910f,1.f}; // #5BB8E8  hover

    // Hover / active deltas applied to panel bg
    static constexpr ImVec4 hover   = {0.286f,0.302f,0.333f,1.f};    // #494d55
    static constexpr ImVec4 active  = {0.239f,0.647f,0.851f,1.f};    // == accent

    // Scrollbar
    static constexpr ImVec4 scroll_grab        = {0.322f,0.337f,0.365f,1.f};
    static constexpr ImVec4 scroll_grab_hover  = {0.384f,0.400f,0.427f,1.f};
    static constexpr ImVec4 scroll_grab_active = {0.447f,0.463f,0.490f,1.f};

    // Check / plot colour — reuse accent family (Revit has no separate teal)
    static constexpr ImVec4 teal = {0.357f,0.722f,0.910f,1.f};       // == accent_hot
}

void apply_editor_dark_theme() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ── Shape ──────────────────────────────────────────────────────────────
    s.WindowPadding      = {8.f,  8.f};
    s.FramePadding       = {6.f,  3.f};
    s.CellPadding        = {4.f,  2.f};
    s.ItemSpacing        = {8.f,  4.f};
    s.ItemInnerSpacing   = {4.f,  4.f};
    s.IndentSpacing      = 18.f;
    s.ScrollbarSize      = 12.f;
    s.GrabMinSize        = 8.f;

    s.WindowBorderSize   = 1.f;
    s.ChildBorderSize    = 1.f;
    s.PopupBorderSize    = 1.f;
    s.FrameBorderSize    = 0.f;
    s.TabBorderSize      = 0.f;

    s.WindowRounding     = 4.f;
    s.ChildRounding      = 4.f;
    s.FrameRounding      = 3.f;
    s.PopupRounding      = 4.f;
    s.ScrollbarRounding  = 3.f;
    s.GrabRounding       = 3.f;
    s.TabRounding        = 4.f;

    // ── Colours ────────────────────────────────────────────────────────────
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                  = theme::text;
    c[ImGuiCol_TextDisabled]          = theme::text_dim;
    c[ImGuiCol_WindowBg]              = theme::bg_panel;
    c[ImGuiCol_ChildBg]               = theme::bg_deep;
    c[ImGuiCol_PopupBg]               = theme::bg_popup;
    c[ImGuiCol_Border]                = theme::border;
    c[ImGuiCol_BorderShadow]          = {0,0,0,0};

    c[ImGuiCol_FrameBg]               = theme::bg_widget;
    c[ImGuiCol_FrameBgHovered]        = theme::hover;
    c[ImGuiCol_FrameBgActive]         = theme::accent_dim;

    c[ImGuiCol_TitleBg]               = theme::bg_deep;
    c[ImGuiCol_TitleBgActive]         = theme::bg_deep;
    c[ImGuiCol_TitleBgCollapsed]      = theme::bg_deep;

    c[ImGuiCol_MenuBarBg]             = theme::bg_popup;

    c[ImGuiCol_ScrollbarBg]           = theme::bg_deep;
    c[ImGuiCol_ScrollbarGrab]         = theme::scroll_grab;
    c[ImGuiCol_ScrollbarGrabHovered]  = theme::scroll_grab_hover;
    c[ImGuiCol_ScrollbarGrabActive]   = theme::scroll_grab_active;

    c[ImGuiCol_CheckMark]             = theme::teal;
    c[ImGuiCol_SliderGrab]            = theme::accent;
    c[ImGuiCol_SliderGrabActive]      = theme::accent_hot;

    c[ImGuiCol_Button]                = theme::bg_popup;
    c[ImGuiCol_ButtonHovered]         = theme::hover;
    c[ImGuiCol_ButtonActive]          = theme::accent;

    c[ImGuiCol_Header]                = theme::accent_dim;
    c[ImGuiCol_HeaderHovered]         = theme::accent_hot;
    c[ImGuiCol_HeaderActive]          = theme::accent;

    c[ImGuiCol_Separator]             = theme::border;
    c[ImGuiCol_SeparatorHovered]      = theme::accent;
    c[ImGuiCol_SeparatorActive]       = theme::accent;

    c[ImGuiCol_ResizeGrip]            = {0,0,0,0};
    c[ImGuiCol_ResizeGripHovered]     = theme::accent;
    c[ImGuiCol_ResizeGripActive]      = theme::accent_hot;

    // Tabs
    c[ImGuiCol_Tab]                   = theme::bg_deep;
    c[ImGuiCol_TabHovered]            = theme::hover;
    c[ImGuiCol_TabActive]             = theme::bg_panel;
    c[ImGuiCol_TabUnfocused]          = theme::bg_deep;
    c[ImGuiCol_TabUnfocusedActive]    = theme::bg_popup;

    // Docking
    c[ImGuiCol_DockingPreview]        = {theme::accent.x, theme::accent.y, theme::accent.z, 0.25f};
    c[ImGuiCol_DockingEmptyBg]        = theme::bg_deep;

    // Plot / misc
    c[ImGuiCol_PlotLines]             = theme::teal;
    c[ImGuiCol_PlotLinesHovered]      = theme::accent_hot;
    c[ImGuiCol_PlotHistogram]         = theme::teal;
    c[ImGuiCol_PlotHistogramHovered]  = theme::accent_hot;

    c[ImGuiCol_TableHeaderBg]         = theme::bg_popup;
    c[ImGuiCol_TableBorderStrong]     = theme::border;
    c[ImGuiCol_TableBorderLight]      = {0.18f,0.18f,0.18f,1.f};
    c[ImGuiCol_TableRowBg]            = {0,0,0,0};
    c[ImGuiCol_TableRowBgAlt]         = theme::bg_row_alt;

    c[ImGuiCol_TextSelectedBg]        = theme::accent_dim;
    c[ImGuiCol_DragDropTarget]        = {theme::accent.x, theme::accent.y, theme::accent.z, 0.9f};
    c[ImGuiCol_NavHighlight]          = theme::accent;
    c[ImGuiCol_NavWindowingHighlight] = theme::accent;
    c[ImGuiCol_NavWindowingDimBg]     = {0,0,0,0.4f};
    c[ImGuiCol_ModalWindowDimBg]      = {0,0,0,0.45f};
}
