#include "ScriptingStdLib.h"
#include "imgui.h"
#include <variant>
static std::variant<int, float, bool> last_lua_imgui_value;
LuaCallbackShim* LuaCallbackShim::inst = nullptr;
bool LuaSystem::im_checkbox(std::string name, bool b) {
	bool res = ImGui::Checkbox(name.c_str(), &b);
	last_lua_imgui_value = b;
	return res;
}

bool LuaSystem::im_button(std::string name) {
	return ImGui::Button(name.c_str());
}

bool LuaSystem::im_drag_float(std::string name, float f, float speed, float min, float max) {
	bool res = ImGui::DragFloat(name.c_str(), &f, speed, min, max);
	last_lua_imgui_value = f;
	return res;
}

bool LuaSystem::im_input_int(std::string name, int i) {
	bool res = ImGui::InputInt(name.c_str(), &i);
	last_lua_imgui_value = i;
	return res;
}

bool LuaSystem::im_slider_int(std::string name, int i, int min, int max) {
	bool res = ImGui::SliderInt(name.c_str(), &i, min, max);
	last_lua_imgui_value = i;
	return res;
}

void LuaSystem::im_separator() {
	ImGui::Separator();
}

void LuaSystem::im_text(std::string name) {
	ImGui::Text(name.c_str());
}

float LuaSystem::im_get_last_float() {
	return std::get<float>(last_lua_imgui_value);
}

bool LuaSystem::im_get_last_bool() {
	return std::get<bool>(last_lua_imgui_value);
}
int LuaSystem::im_get_last_int() {
	return std::get<int>(last_lua_imgui_value);
}
void LuaSystem::execute_command(int mode, std::string cmd) {
	Cmd_Execute_Mode m = (mode == 0) ? Cmd_Execute_Mode::NOW : Cmd_Execute_Mode::APPEND;
	Cmd_Manager::inst->execute(m, cmd.c_str());
}
