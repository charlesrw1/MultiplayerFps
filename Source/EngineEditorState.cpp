#include "EngineEditorState.h"
#include "IEditorTool.h"
#include "Framework/Util.h"
#include "imgui.h"
#include "Assets/AssetDatabase.h"
#include "Render/Texture.h"
EditorState::EditorState()
{
	g_assets.find_async<Texture>("eng/editor/red_cross.png", [&](GenericAssetPtr ptr) {
		if (auto tex = ptr.cast_to<Texture>())
			this->redCrossIcon = tex.get();
		},true);
}

EditorState::~EditorState()
{
}

void EditorState::hide()
{
	if (curTool) {
		sys_print(Info, "EditorState: Closing tool\n");
		curTool.reset();
	}

}

bool EditorState::wants_scene_viewport_menu_bar() {
	if (curTool.get()) 
		return curTool->wants_scene_viewport_menu_bar();
	return false;
}

void EditorState::hook_pre_viewport()
{
	if (curTool)
		curTool->hook_pre_scene_viewport_draw();
}
void EditorState::hook_viewport()
{
	if (curTool)
		curTool->hook_scene_viewport_draw();
}

void EditorState::open_tool(uptr<CreateEditorAsync> creation, bool set_active, function<void(bool)> callback)
{
	assert(creation);
	sys_print(Debug, "EditorState::open_tool: calling creator\n");
	creation->execute([this, callback](uptr<IEditorTool> arg) {
		if (!arg)
			sys_print(Warning, "EditorState::open_tool: creation returned null\n");

		if (curTool&&arg) {
			sys_print(Debug, "EditorState::open_tool: replacing current tool\n");
		}
		this->curTool = std::move(arg);
		if(callback)
			callback(this->curTool != nullptr);
		});
}

const View_Setup* EditorState::get_vs()
{
	if (curTool)
		return curTool->get_vs();
	return nullptr;
}

void EditorState::tick(float dt)
{
	if (curTool)
		curTool->tick(dt);
}

void EditorState::imgui_hook_new_frame()
{
	if (curTool)
		curTool->hook_imgui_newframe();
}

void EditorState::imgui_draw()
{
	if (curTool)
		curTool->draw_imgui_public();
}

void EditorState::draw_tab_window()
{
	if (ImGui::Begin("EditorTabs")) {

		uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
		if (ImGui::BeginTable("Browser", 2, ent_list_flags))
		{
			ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
			ImGui::TableSetupColumn("X", ImGuiTableColumnFlags_WidthFixed, 15.f, 0);

			if (curTool) {

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				if (ImGui::Selectable("##selectednode", true,0, ImVec2(0, 0))) {
					
				}
				ImGui::SameLine();
				ImGui::SameLine();
				ImGui::Text(curTool->get_asset_type_info().classname);
				ImGui::TableNextColumn();
				if (redCrossIcon) {
					const int sz = redCrossIcon->width;
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5, 0.5, 0.5, 1));
					if(ImGui::ImageButton(ImTextureID(uintptr_t(redCrossIcon->gl_id)), ImVec2(sz, sz))) {

					}
					ImGui::PopStyleColor();
				}
			}
			ImGui::EndTable();
		}
	}
	ImGui::End();
}
