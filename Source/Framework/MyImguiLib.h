#pragma once
#include <vector>
#include <string>
#include <functional>
#include "imgui.h" // ImVec2/ImVec4/ImGuiID etc used below; must come before any use, not relied on from include order
#include "Framework/Util.h" // color32
void MyImSeperator(float x1, float x2, float width);

// Reusable modal matching AssetBrowser::draw_create_asset_popup's UX: a read-only "Folder: X" line
// plus a Name (no extension) field; the extension is fixed per-use and applied automatically (any
// extension the user types into the name field is stripped before it's re-appended). Used for
// "Duplicate...", "Make Prefab Using...", etc. open() triggers it; draw() must be called once per
// frame unconditionally -- it no-ops until open() has been called, and closes itself after confirm/cancel.
class FolderNamePopup {
public:
	// initial_name_no_ext may itself contain an extension; it's stripped. extension should include the dot (e.g. ".tprefab").
	void open(const std::string& title, const std::string& folder, const std::string& initial_name_no_ext,
			  const std::string& extension, std::function<void(const std::string& full_gamepath)> on_confirm);
	void draw();
private:
	bool want_open = false;
	std::string title;
	std::string folder;
	std::string extension;
	std::function<void(const std::string&)> on_confirm;
	static constexpr int BUF_SIZE = 128;
	char name_buf[BUF_SIZE]{};
};

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