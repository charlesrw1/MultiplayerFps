#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "imgui.h"
#include "MyImguiLib.h"
#include <cstring>

// Strips a trailing "name.ext" extension, if any, leaving just the base name.
static std::string strip_ext(const std::string& name) {
	auto dot = name.rfind('.');
	return dot == std::string::npos ? name : name.substr(0, dot);
}

void FolderNamePopup::open(const std::string& title_, const std::string& folder_, const std::string& initial_name_no_ext,
							const std::string& extension_, std::function<void(const std::string&)> on_confirm_) {
	want_open = true;
	title = title_;
	folder = folder_;
	extension = extension_;
	on_confirm = std::move(on_confirm_);
	std::string base = strip_ext(initial_name_no_ext);
	strncpy(name_buf, base.c_str(), BUF_SIZE - 1);
	name_buf[BUF_SIZE - 1] = '\0';
}

void FolderNamePopup::draw() {
	if (want_open) {
		ImGui::OpenPopup(title.c_str());
		want_open = false;
	}
	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextDisabled("Folder: %s", folder.empty() ? "(root)" : folder.c_str());
	ImGui::Text("Name (no extension):");
	bool enter_pressed = ImGui::InputText("##folder_name_popup", name_buf, BUF_SIZE, ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::Separator();
	bool do_confirm = enter_pressed || ImGui::Button("Confirm", ImVec2(120, 0));
	ImGui::SameLine();
	bool do_cancel = ImGui::Button("Cancel", ImVec2(120, 0));

	if (do_confirm && name_buf[0] != '\0') {
		std::string base = strip_ext(name_buf);
		std::string full_path = (folder.empty() ? base : folder + "/" + base) + extension;
		if (on_confirm)
			on_confirm(full_path);
		ImGui::CloseCurrentPopup();
	} else if (do_cancel) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
}

void MyImSeperator(float x1, float x2, float width) {
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
		const float thickness_for_layout =
			(thickness == 1.0f) ? 0.0f : thickness; // FIXME: See 1.70/1.71 Separator() change: makes legacy 1-px
													// separator not affect layout yet. Should change.
		const ImRect bb(ImVec2(x1 + 5.0, window->DC.CursorPos.y), ImVec2(x2 - 5.0, window->DC.CursorPos.y + width));
		ItemSize(ImVec2(0.0f, thickness_for_layout));

		const bool item_visible = ItemAdd(bb, 0);
		if (item_visible) {
			// Draw
			window->DrawList->AddRectFilled(bb.Min, bb.Max, GetColorU32(ImGuiCol_Separator));
		}
	}
}

ImVec2 get_screen_pos(ImRect frame, ImVec2 param, ImVec2 min, ImVec2 max) {
	ImVec2 width = max - min;
	ImVec2 normalized = (param - min) / width;
	ImVec2 screen = (frame.Max - frame.Min) * normalized + frame.Min;
	return screen;
}

// current_pos is both an in param (draws the current blend-input marker at that position)
// and an out param (dragging inside the plot writes the new position back into it). Returns
// true the frame the user drags the marker to a new position.
bool MyImDrawBlendSpace(const char* label, const std::vector<ImVec2>& verts, const std::vector<int>& indicies,
						const std::vector<const char*>& vert_names, ImVec2 minbound, ImVec2 maxbound,
						ImVec2* current_pos) {
	assert(indicies.size() % 3 == 0);

	using namespace ImGui;

	bool changed = false;

	ImVec2 size = ImVec2(maxbound.x - minbound.x, maxbound.y - minbound.y);
	float height_scale = size.y / size.x;

	ImGuiWindow* window = GetCurrentWindow();
	auto& style = GetStyle();
	auto id = window->GetID(label);

	PushItemWidth(-FLT_MIN);

	float width = CalcItemWidth();
	float height = width * height_scale;
	const ImVec2 frame_size = ImVec2(width, height); // Arbitrary default of 8 lines high for multi-line

	const ImRect frame_bb(window->DC.CursorPos, window->DC.CursorPos + frame_size);

	ImGui::PushStyleColor(ImGuiCol_ChildBg, color32_to_imvec4({128, 128, 128}));
	ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, style.FrameRounding);
	ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, style.FrameBorderSize);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
						ImVec2(0, 0)); // Ensure no clip rect so mouse hover can reach FramePadding edges
	bool child_visible = ImGui::BeginChildEx(label, id, frame_bb.GetSize(), true,
											 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoMouseInputs);

	window = GetCurrentWindow();

	float border_width = 5.0;

	const ImRect sub_bb(window->Pos + ImVec2(border_width, border_width),
						window->Pos + window->Size - ImVec2(border_width, border_width));

	PopStyleVar(3);
	PopStyleColor();
	if (!child_visible) {
		EndChild();
		return false;
	}
	ImGui::EndChild();

	// Whole-plot drag catcher, added first so the smaller per-sample buttons below (added
	// after, so they win hover/hit-test priority on their own circles) can still show
	// tooltips on top of it. Click/drag anywhere in the plot to move the current blend input.
	SetCursorScreenPos(sub_bb.Min);
	ImGui::PushID(label);
	InvisibleButton("##blendspace_drag", sub_bb.GetSize());
	ImGui::PopID();
	if (current_pos && IsItemActive()) {
		ImVec2 mouse = GetIO().MousePos;
		ImVec2 t = (mouse - sub_bb.Min) / sub_bb.GetSize();
		t.x = ImClamp(t.x, 0.f, 1.f);
		t.y = ImClamp(t.y, 0.f, 1.f);
		*current_pos = ImVec2(minbound.x + t.x * size.x, minbound.y + t.y * size.y);
		changed = true;
	}

	for (int i = 0; i < indicies.size(); i += 3) {
		ImVec2 p1 = get_screen_pos(sub_bb, verts[indicies[i]], minbound, maxbound);
		ImVec2 p2 = get_screen_pos(sub_bb, verts[indicies[i + 1]], minbound, maxbound);
		ImVec2 p3 = get_screen_pos(sub_bb, verts[indicies[i + 2]], minbound, maxbound);

		window->DrawList->AddLine(p1, p2, color32_to_int({200, 200, 200}));
		window->DrawList->AddLine(p2, p3, color32_to_int({200, 200, 200}));
		window->DrawList->AddLine(p3, p1, color32_to_int({200, 200, 200}));
	}

	auto restore_pos = GetCursorScreenPos();
	for (int i = 0; i < verts.size(); i++) {

		const char* name = vert_names[i];
		ImVec2 p = get_screen_pos(sub_bb, verts[i], minbound, maxbound);

		window->DrawList->AddCircleFilled(p, 10, color32_to_int({5, 226, 255, 160}), 8);

		SetCursorScreenPos(p - ImVec2(10, 10));
		ImGui::PushID(i);
		InvisibleButton("##blendbutton", ImVec2(20, 20));
		if (IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlapped)) {
			if (ImGui::BeginTooltip()) {
				ImGui::Text("%s\n( %.3f, %.3f )\n", name, verts[i].x, verts[i].y);
				EndTooltip();
			}
		}
		ImGui::PopID();
	}

	// Current blend-input marker: a crosshair over a filled dot, drawn last so it's on top.
	if (current_pos) {
		ImVec2 p = get_screen_pos(sub_bb, *current_pos, minbound, maxbound);
		window->DrawList->AddCircleFilled(p, 6, color32_to_int({255, 60, 60, 255}), 12);
		window->DrawList->AddCircle(p, 10, color32_to_int({255, 255, 255, 255}), 12, 2.f);
		window->DrawList->AddLine(p - ImVec2(14, 0), p + ImVec2(14, 0), color32_to_int({255, 60, 60, 255}));
		window->DrawList->AddLine(p - ImVec2(0, 14), p + ImVec2(0, 14), color32_to_int({255, 60, 60, 255}));
	}

	SetCursorScreenPos(restore_pos);

	return changed;
}

#include "IEditorTool.h"
#include "Render/Texture.h"
#ifdef EDITOR_BUILD
bool my_imgui_image_button(const Texture* t, int size) {
	ImVec2 sz_to_use(size, size);
	if (size == -1) {
		auto s = t->get_size();
		sz_to_use = ImVec2(s.x, s.y);
	}

	return (ImGui::ImageButton(ImTextureID(uint64_t(t->get_internal_render_handle())), sz_to_use));
}
void my_imgui_image(const Texture* t, int size) {
	ImVec2 sz_to_use(size, size);
	if (size == -1) {
		auto s = t->get_size();
		sz_to_use = ImVec2(s.x, s.y);
	}
	ImGui::Image(ImTextureID(uint64_t(t->get_internal_render_handle())), sz_to_use);
}

bool my_imgui_icon_text_button(const Texture* icon, const char* label, float icon_size) {
	const float fh = ImGui::GetFrameHeight();
	const float pad = ImGui::GetStyle().FramePadding.x;
	const float gap = 4.f;
	const float text_w = ImGui::CalcTextSize(label).x;
	const float total_w = pad + icon_size + gap + text_w + pad;
	ImVec2 pos = ImGui::GetCursorScreenPos();
	bool clicked = ImGui::InvisibleButton(label, ImVec2(total_w, fh));
	bool hov = ImGui::IsItemHovered();
	bool act = ImGui::IsItemActive();
	ImU32 bg = act ? ImGui::GetColorU32(ImGuiCol_ButtonActive)
	         : hov ? ImGui::GetColorU32(ImGuiCol_ButtonHovered)
	               : ImGui::GetColorU32(ImGuiCol_Button);
	auto* dl = ImGui::GetWindowDrawList();
	float rounding = ImGui::GetStyle().FrameRounding;
	dl->AddRectFilled(pos, ImVec2(pos.x + total_w, pos.y + fh), bg, rounding);
	dl->AddRect(pos, ImVec2(pos.x + total_w, pos.y + fh), ImGui::GetColorU32(ImGuiCol_Border), rounding);
	if (icon) {
		float yo = (fh - icon_size) * 0.5f;
		dl->AddImage(ImTextureID(uint64_t(icon->get_internal_render_handle())),
			ImVec2(pos.x + pad, pos.y + yo), ImVec2(pos.x + pad + icon_size, pos.y + yo + icon_size),
			ImVec2(0, 0), ImVec2(1, 1));
	}
	ImVec2 text_pos(pos.x + pad + icon_size + gap, pos.y + (fh - ImGui::GetTextLineHeight()) * 0.5f);
	dl->AddText(text_pos, ImGui::GetColorU32(ImGuiCol_Text), label);
	return clicked;
}
ImGuiID dock_over_viewport(const ImGuiViewport* viewport, ImGuiDockNodeFlags dockspace_flags, IEditorTool* tool,
						   const ImGuiWindowClass* window_class) {
	using namespace ImGui;

	if (viewport == NULL)
		viewport = GetMainViewport();

	SetNextWindowPos(viewport->WorkPos);
	SetNextWindowSize(viewport->WorkSize);
	SetNextWindowViewport(viewport->ID);

	ImGuiWindowFlags host_window_flags = 0;
	host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
						 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
	host_window_flags |=
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
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