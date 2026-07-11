#include "ScriptingStdLib.h"
#include "imgui.h"
#include <variant>
LuaCallbackShim* LuaCallbackShim::inst = nullptr;
std::pair<bool,bool> LuaSystem::im_checkbox(std::string name, bool b) {
	bool changed = ImGui::Checkbox(name.c_str(), &b);
	return {changed, b};
}
void LuaSystem::im_same_line() {
	ImGui::SameLine();
}

bool LuaSystem::im_button(std::string name) {
	return ImGui::Button(name.c_str());
}

std::pair<bool, float> LuaSystem::im_drag_float(std::string name, float f, float speed, float min, float max) {
	bool changed = ImGui::DragFloat(name.c_str(), &f, speed, min, max);
	return {changed, f};
}

std::pair<bool, int> LuaSystem::im_input_int(std::string name, int i) {
	bool changed = ImGui::InputInt(name.c_str(), &i);
	return {changed, i};
}

std::pair<bool, int> LuaSystem::im_slider_int(std::string name, int i, int min, int max) {
	bool changed = ImGui::SliderInt(name.c_str(), &i, min, max);
	return {changed, i};
}

void LuaSystem::im_separator() {
	ImGui::Separator();
}

void LuaSystem::im_text(std::string name) {
	ImGui::Text(name.c_str());
}

void LuaSystem::execute_command(int mode, std::string cmd) {
	Cmd_Execute_Mode m = (mode == 0) ? Cmd_Execute_Mode::NOW : Cmd_Execute_Mode::APPEND;
	Cmd_Manager::inst->execute(m, cmd.c_str());
}
