#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Framework/Util.h"
#include <string>
#include <fstream>
#include "imgui.h"
#include "Framework/Files.h"
#include "Framework/Util.h"
#include "GameEnginePublic.h"
#include <cassert>
#include <SDL3/SDL.h>

#include "Assets/AssetBrowser.h"
#include "Assets/AssetRegistry.h"

void IEditorTool::set_has_editor_changes() {
	if (!this->has_editor_changes) {
		this->has_editor_changes = true;
		set_window_title();
	}
}
void IEditorTool::set_window_title() {
	const char* name = string_format("LevelEditor: %s%s\n", get_doc_name().c_str(), has_editor_changes ? "*" : "");
	SDL_SetWindowTitle(eng->get_os_window(), name);
}

extern ConfigVar g_project_name;

#include "EditorPopups.h"

bool IEditorTool::save() {

	return save_document_internal();
}

static void draw_popups_for_editor(bool& open_open_popup, bool& open_save_popup, std::string& name, IEditorTool* tool,
								   const std::string& prefix) {
	if (open_open_popup) {
		ImGui::OpenPopup("Open file dialog");
		open_open_popup = false;
	}
	if (open_save_popup) {
		ImGui::OpenPopup("Save file dialog");
		open_save_popup = false;
	}

	if (ImGui::BeginPopupModal("Save file dialog")) {
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Will be saved under %s", prefix.c_str());

		tool->save();
	}

	if (ImGui::BeginPopupModal("Open file dialog")) {
		assert(0);
		// ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Searched for in %s", prefix.c_str());
		// open_or_save_file_dialog([&](const char* buf) {
		//	tool->open( (buf +std::string( "." ) + tool->get_save_file_extension()).c_str() );
		//	}, prefix.c_str(), false, tool);
	}
}

void IEditorTool::draw_imgui_public() {
	imgui_draw();
}
#include "Game/LevelAssets.h"

bool begin_main_menu_bar_menu(const char* label) {
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.06f, 0.06f, 0.06f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.90f, 0.90f, 0.90f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.79f, 0.87f, 0.95f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.67f, 0.81f, 0.93f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.94f, 0.94f, 0.94f, 1.f));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.68f, 0.68f, 0.68f, 1.f));
	const bool open = ImGui::BeginMenu(label);
	if (!open)
		ImGui::PopStyleColor(6);
	return open;
}

void end_main_menu_bar_menu() {
	ImGui::EndMenu();
	ImGui::PopStyleColor(6);
}

void IEditorTool::draw_menu_bar() {
	if (ImGui::BeginMenuBar()) {
		if (begin_main_menu_bar_menu("File")) {
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
			}
			// if (ImGui::MenuItem("New map")) {
			//	Cmd_Manager::inst->append_cmd(std::make_unique<OpenEditorToolCommand>(SceneAsset::StaticType,
			// std::nullopt, true));
			//}
			// if (ImGui::MenuItem("New prefab")) {
			//	Cmd_Manager::inst->append_cmd(std::make_unique<OpenEditorToolCommand>(PrefabAsset::StaticType,
			// std::nullopt, true));
			//}

			hook_menu_bar_file_menu();

			end_main_menu_bar_menu();
		}

		hook_menu_bar();

		if (begin_main_menu_bar_menu("Layout")) {
			if (ImGui::MenuItem("Save Layout")) {
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, "dump_imgui_ini layout.ini");
			}
			if (ImGui::MenuItem("Load Layout")) {
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini layout.ini");
			}
			end_main_menu_bar_menu();
		}

		ImGui::EndMenuBar();
	}
}
#endif
