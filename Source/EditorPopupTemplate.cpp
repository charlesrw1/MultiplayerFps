#include "EditorPopupTemplate.h"
#include "imgui.h"
void PopupTemplate::create_are_you_sure(EditorPopupManager* mgr, const string& desc, function<void()> continue_func)
{
	mgr->add_popup(
		"Are you sure?",
		[desc,continue_func]()->bool {
			ImGui::Text("%s",desc.c_str());
			ImGui::Spacing();
			bool ret = false;
			if (ImGui::Button("Continue")) {
				ret = true;
				continue_func();
			}
			ImGui::SameLine(0,20);
			if (ImGui::Button("Cancel")) {
				ret = true;
			}
			return ret;
		});
}

void PopupTemplate::create_basic_okay(EditorPopupManager* mgr, const std::string& title, const std::string& desc)
{
	mgr->add_popup(
		title,
		[desc]()->bool {
			ImGui::Text("%s", desc.c_str());
			ImGui::Spacing();
			return ImGui::Button("Okay");
		}
	);
}

class SaveAsState {
public:
	bool did_last_attempt_fail = false;
	string input_buffer;
	string extension;
	function<void(string)> callback;
	bool actually_wants_save = false;
	bool set_focus = true;
};
#include "Framework/Files.h"
extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);
void PopupTemplate::create_file_save_as(EditorPopupManager* mgr, function<void(string)> on_save, string extension)
{
	SaveAsState state;
	state.extension = extension;
	state.callback = on_save;
	mgr->add_popup(
		"Save file as",
		[mgr,state]() mutable -> bool {
			ImGui::Text("Path saved to asset dir");
			ImGui::Text("Extension = .%s\n",state.extension.c_str());
			ImGui::Separator();
			if (state.set_focus) {
				ImGui::SetKeyboardFocusHere();
				state.set_focus = false;
			}
			if (ImGui::InputText("##input", state.input_buffer.data(), state.input_buffer.size() + 1, ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize,&state.input_buffer)) {
				state.input_buffer = state.input_buffer.c_str();
			}
			if (ImGui::Button("Save")) {
				auto fullpath = state.input_buffer + "." + state.extension;
				auto file = FileSys::open_read_game(fullpath);
				if (file) {
					PopupTemplate::create_are_you_sure(mgr, "File \"" + fullpath + "\" already exists, overwrite?", [&state]() {
						state.actually_wants_save = true;
						});
				}
				else {
					state.actually_wants_save = true;
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				return true;
			}
			if (state.actually_wants_save) {
				auto fullpath = state.input_buffer + "." + state.extension;
				auto testOut = FileSys::open_write_game(fullpath);
				if (!testOut) {
					PopupTemplate::create_basic_okay(mgr, "Error", "Can't save there.");
					state.actually_wants_save = false;
				}
				else{
					testOut.reset();	// close the file handle

					state.callback(fullpath);
					return true;
				}
			}
			return false;
		}
	);
}
