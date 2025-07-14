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
#include <SDL2/SDL.h>

#include "Assets/AssetBrowser.h"
#include "Assets/AssetRegistry.h"

void IEditorTool::set_has_editor_changes()
{
	if (!this->has_editor_changes) {
		this->has_editor_changes = true;
		set_window_title();
	}
}
void IEditorTool::set_window_title()
{
	auto metadata = AssetRegistrySystem::get().find_for_classtype(&get_asset_type_info());
	assert(metadata);
	const char* name = string_format("%s Editor: %s%s\n", metadata->get_type_name().c_str(), get_doc_name().c_str(), has_editor_changes?"*":"");
	SDL_SetWindowTitle(eng->get_os_window(), name);
}
#if 0
bool IEditorTool::open(const char* name, const char* arg) {

	if (!is_initialized) {
		init();
		is_initialized = true;
	}

	close();

	this->name = name;
	bool good = open_document_internal(name, arg);

	if (!good)
		return false;

	is_open = true;
	has_editor_changes = false;

	set_window_title();

	return true;
}
#endif
extern ConfigVar g_project_name;

#include "EditorPopups.h"
void IEditorTool::try_save(std::function<void()> callback)
{
	EditorPopupManager::inst->add_popup(
		"Save new document.",
		[this,callback]()->bool {
			if (ImGui::Button("Save?")) {
				save();
				callback();
				return true;
			}
			if (ImGui::Button("Cancel.")) {
				return true;
			}
			return false;
		}
	);
}

void IEditorTool::try_close(std::function<void()> callback)
{
	bool is_done_with_save = false;
	EditorPopupManager::inst->add_popup(
		"Are you sure?",
		[callback, is_done_with_save,this]()mutable->bool {
			ImGui::Text("Editor is open, continue without saving?");

			if (ImGui::Button("Continue")) {
				callback();
				return true;
			}
			if (ImGui::Button("Save and continue")) {
				try_save([&is_done_with_save]()mutable {
					is_done_with_save = true;
					});
				return false;
			}
			if (ImGui::Button("Cancel")) {

				return true;
			}
			if (is_done_with_save) {
				callback();
				return true;
			}

			return false;
		}
	);
}
#if 0

void IEditorTool::close()
{
	if (!is_open)
		return;
	// fixme: prompt to save or not save?
	save();
	open_open_popup = open_save_popup = false;
	close_internal();
	name = "";
	is_open = false;
	has_editor_changes = false;

	SDL_SetWindowTitle(eng->get_os_window(), g_project_name.get_string());
}
#endif

bool IEditorTool::save()
{

	return save_document_internal();
}

template<typename FUNCTOR>
static void open_or_save_file_dialog(FUNCTOR&& callback, const std::string& path_prefix, const bool is_save_dialog, IEditorTool* tool)
{
	static bool alread_exists = false;
	static bool cant_open_path = false;
	static char buffer[256];
	static bool init = true;
	if (init) {
		buffer[0] = 0;
		alread_exists = false;
		cant_open_path = false;
		init = false;
	}
	bool write_out = false;

	bool returned_true = false;
	if (!alread_exists || !is_save_dialog) {
		ImGui::Text("Enter path (.%s): ", tool->get_save_file_extension());
		ImGui::SameLine();
		returned_true = ImGui::InputText("##pathinput", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue);
	}

	if (returned_true) {
		const char* full_path = string_format("%s/%s.%s", path_prefix.c_str(), buffer, tool->get_save_file_extension());
		bool file_already_exists = FileSys::does_file_exist(buffer,FileSys::GAME_DIR);
		cant_open_path = false;
		alread_exists = false;

		if (is_save_dialog) {

			if (file_already_exists)
				alread_exists = true;
			else {
				std::ofstream test_open(full_path);
				if (!test_open)
					cant_open_path = true;
			}
			if (!alread_exists && !cant_open_path) {
				write_out = true;
			}
		}
		else {

			if (file_already_exists)
				write_out = true;
			else
				cant_open_path = true;

		}
	}
	if (alread_exists) {

		if (is_save_dialog) {

			ImGui::Text("File already exists. Overwrite?");
			if (ImGui::Button("Yes")) {
				write_out = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("No")) {
				alread_exists = false;
			}
		}
		else {
			// open file dialog
			write_out = true;
		}
	}
	else if (cant_open_path) {
		ImGui::Text("Cant open path\n");
	}
	ImGui::Separator();
	if (ImGui::Button("Cancel")) {
		init = true;
		ImGui::CloseCurrentPopup();
	}

	if (write_out) {
init = true;
ImGui::CloseCurrentPopup();
callback(buffer);
	}

	ImGui::EndPopup();
}

static void draw_popups_for_editor(bool& open_open_popup, bool& open_save_popup, std::string& name, IEditorTool* tool, const std::string& prefix)
{
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
		open_or_save_file_dialog([&](const char* buf) {
			name = buf + std::string(".") + tool->get_save_file_extension();
			tool->save();
			}, prefix.c_str(), true, tool);
	}

	if (ImGui::BeginPopupModal("Open file dialog")) {
		assert(0);
		//ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Searched for in %s", prefix.c_str());
		//open_or_save_file_dialog([&](const char* buf) {
		//	tool->open( (buf +std::string( "." ) + tool->get_save_file_extension()).c_str() );
		//	}, prefix.c_str(), false, tool);
	}
}


void IEditorTool::draw_imgui_public()
{
	imgui_draw();
}
#include "EngineSystemCommands.h"
#include "Game/LevelAssets.h"
void IEditorTool::draw_menu_bar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				save();
			}
			if (ImGui::MenuItem("New map")) {
				Cmd_Manager::inst->append_cmd(std::make_unique<OpenEditorToolCommand>(SceneAsset::StaticType, std::nullopt, true));
			}
			if (ImGui::MenuItem("New prefab")) {
				Cmd_Manager::inst->append_cmd(std::make_unique<OpenEditorToolCommand>(PrefabAsset::StaticType, std::nullopt, true));
			}

			hook_menu_bar_file_menu();

			ImGui::EndMenu();
		}

		hook_menu_bar();

		ImGui::EndMenuBar();
	}
}
#endif