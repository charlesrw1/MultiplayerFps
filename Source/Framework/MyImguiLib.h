#pragma once
#include <vector>
#include "Framework/Util.h" // color32
void MyImSeperator(float x1, float x2, float width);

// Draws a 2D blend-space plot: `verts`/`indicies` are the triangulated sample grid (indicies
// grouped in triples), `vert_names` labels each vertex's hover tooltip. `current_pos` is the
// current blend input -- drawn as a marker, and draggable: returns true (and writes the new
// value into it) the frame the user drags it to a new position.
bool MyImDrawBlendSpace(const char* label, const std::vector<ImVec2>& verts, const std::vector<int>& indicies,
						const std::vector<const char*>& vert_names, ImVec2 minbound, ImVec2 maxbound,
						ImVec2* current_pos);

inline ImVec4 color32_to_imvec4(Color32 color) {
	return ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f);
}
inline unsigned int color32_to_int(Color32 color) {
	return *(unsigned int*)&color;
}
namespace ImGui {
int Curve(const char* label, const ImVec2& size, const int maxpoints, ImVec2* points, int* selection,
		  const ImVec2& rangeMin = ImVec2(0, 0), const ImVec2& rangeMax = ImVec2(1, 1));
};

class Texture;
bool my_imgui_image_button(const Texture* t, int size);
void my_imgui_image(const Texture* t, int size);
bool my_imgui_icon_text_button(const Texture* icon, const char* label, float icon_size = 13.f);

class IEditorTool;
ImGuiID dock_over_viewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags dockspace_flags, IEditorTool* tool,
						   const ImGuiWindowClass* window_class = nullptr);