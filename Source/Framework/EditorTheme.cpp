#include "EditorTheme.h"
#include "imgui.h"

// Colour palette — edit these to retune the whole UI at once
namespace theme {
    // Bases
    static constexpr ImVec4 bg_deep    = {0.10f, 0.10f, 0.10f, 1.f}; // #1a1a1a  deepest bg
    static constexpr ImVec4 bg_panel   = {0.145f,0.145f,0.148f,1.f}; // #252526  panel / window
    static constexpr ImVec4 bg_widget  = {0.12f, 0.12f, 0.12f, 1.f}; // #1e1e1e  input / frame bg
    static constexpr ImVec4 bg_popup   = {0.176f,0.176f,0.188f,1.f}; // #2d2d30  popup / menu bg
    static constexpr ImVec4 bg_header  = {0.176f,0.176f,0.188f,1.f}; // #2d2d30  collapsing header
    static constexpr ImVec4 bg_row_alt = {0.00f, 0.00f, 0.00f,0.10f};// faint zebra stripe

    // Borders
    static constexpr ImVec4 border     = {0.24f, 0.24f, 0.24f, 1.f}; // #3d3d3d

    // Text
    static constexpr ImVec4 text       = {0.784f,0.784f,0.784f,1.f}; // #c8c8c8  body
    static constexpr ImVec4 text_dim   = {0.502f,0.502f,0.502f,1.f}; // #808080  disabled

    // Accent — VS Code blue.  Change to {0.957f,0.639f,0.137f,1.f} for Unreal orange.
    static constexpr ImVec4 accent      = {0.000f,0.478f,0.800f,1.f}; // #007ACC
    static constexpr ImVec4 accent_dim  = {0.149f,0.290f,0.467f,1.f}; // #264F78  selection bg
    static constexpr ImVec4 accent_hot  = {0.196f,0.478f,0.824f,1.f}; // #3279D2  hover

    // Hover / active deltas applied to panel bg
    static constexpr ImVec4 hover   = {0.243f,0.243f,0.259f,1.f};    // #3e3e42
    static constexpr ImVec4 active  = {0.000f,0.478f,0.800f,1.f};    // == accent

    // Scrollbar
    static constexpr ImVec4 scroll_grab        = {0.259f,0.259f,0.259f,1.f};
    static constexpr ImVec4 scroll_grab_hover  = {0.322f,0.322f,0.322f,1.f};
    static constexpr ImVec4 scroll_grab_active = {0.388f,0.388f,0.388f,1.f};

    // Teal check / plot colour
    static constexpr ImVec4 teal = {0.306f,0.788f,0.690f,1.f};       // #4EC9B0
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
