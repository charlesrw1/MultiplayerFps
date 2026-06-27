#pragma once
#include "imgui.h"

// Apply a VS Code / Unreal 5 inspired dark theme.
// Call once after ImGui::CreateContext(), before the first frame.
void apply_editor_dark_theme();

// Font pointers set once after ImGui font build.
extern ImFont* g_prop_bold_font;    // inconsolata bold 14pt  — group/array headers
extern ImFont* g_prop_regular_font; // inter regular 14pt     — property name labels
