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


bool IEditorTool::open(const char* name, const char* arg) {
	//assert(get_focus_state() != editor_focus_state::Closed);	// must have opened

	//if (get_focus_state() == editor_focus_state::Background) {
	//	sys_print("!!! cant open %s while game is running. Close the level then try again.\n", get_editor_name());
	//	return false;
	//}
	// close currently open document
	close();
	assert(!has_document_open());

	open_document_internal(name, arg);

	{
		const char* window_name = "unnamed";
		if (current_document_has_path())
			window_name = this->name.c_str();
		SDL_SetWindowTitle(eng->get_os_window(), string_format("%s: %s",get_editor_name(), window_name));
	}

	return true;
}

void IEditorTool::close()
{
	if (!has_document_open())
		return;
	// fixme: prompt to save?
	save();
	open_open_popup = open_save_popup = false;
	close_internal();
	name = "";

	SDL_SetWindowTitle(eng->get_os_window(),"CsRemake");

	ASSERT(!has_document_open());
}
bool IEditorTool::save()
{
	if (!can_save_document()) {
		sys_print("!!! cant save graph while playing\n");
		return false;
	}
	if (!current_document_has_path()) {
		open_save_popup = true;
		return false;
	}

	{
		const char* window_name = "unnamed";
		if (current_document_has_path())
			window_name = this->name.c_str();
		SDL_SetWindowTitle(eng->get_os_window(), string_format("%s: %s", get_editor_name(), window_name));
	}

	return save_document_internal();
}

/* "./Data/Animations/Graphs/%s" */
template<typename FUNCTOR>
static void open_or_save_file_dialog(FUNCTOR&& callback, const std::string& path_prefix, const bool is_save_dialog)
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
		ImGui::Text("Enter path: ");
		ImGui::SameLine();
		returned_true = ImGui::InputText("##pathinput", buffer, 256, ImGuiInputTextFlags_EnterReturnsTrue);
	}

	if (returned_true) {
		const char* full_path = string_format("%s%s", path_prefix.c_str(), buffer);
		bool file_already_exists = FileSys::does_os_file_exist(full_path);
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
			name = buf;
			tool->save();
			}, prefix.c_str(), true);
	}

	if (ImGui::BeginPopupModal("Open file dialog")) {
		ImGui::TextColored(ImVec4(0.5, 0.5, 0.5, 1), "Searched for in %s", prefix.c_str());
		open_or_save_file_dialog([&](const char* buf) {
			tool->open_document(buf);
			}, prefix.c_str(), false);
	}
}
/* "./Data/Animations/Graphs/"*/

void IEditorTool::imgui_draw()
{
	draw_popups_for_editor(open_open_popup, open_save_popup, name, this, get_save_root_dir());
}
