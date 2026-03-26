#pragma once
#include "Framework/ClassBase.h"
#include <vector>
#include <string>
#include <unordered_set>

#include "Framework/Util.h"
#include "GameEnginePublic.h"
// engine callback shim
// console cmd callbacks
// imgui menu callbacks
// has a lua impl, which sets inst
#include "Framework/ConsoleCmdGroup.h"
#include "Framework/Config.h"
class LuaCallbackShim : public ClassBase
{
public:
	CLASS_BODY(LuaCallbackShim, scriptable);

	static LuaCallbackShim* inst;

	REF static void set_inst(LuaCallbackShim* i) {
		ASSERT(!inst);
		inst = i;
	}
	LuaCallbackShim() { cmds_groups = ConsoleCmdGroup::create(""); }

	// cursed
	REF void _add_command_name(std::string command_name) {
		cmds_groups->add(command_name, [this](const Cmd_Args& args) {
			std::vector<std::string> args2;
			for (int i = 1; i < args.size(); i++) {
				args2.push_back(args.at(i));
			}
			_call_command(args.at(0), args2);
		});
	}
	REF void _add_imgui_menu_name(std::string imgui_menu) {
		some_bs.push_back(std::make_unique<std::string>(imgui_menu));
		Debug_Interface::get()->add_hook_w_name(some_bs.back()->c_str(), debug_interface_function);
	}

	// overridden by lua
	REF virtual void _call_command(std::string name, std::vector<std::string> args) {}
	REF virtual void _call_imgui_menu(std::string name) {}

private:
	static void debug_interface_function(const char* name) {
		auto self = LuaCallbackShim::inst;
		ASSERT(self);
		self->_call_imgui_menu(name);
	}

	std::vector<uptr<std::string>> some_bs;
	uptr<ConsoleCmdGroup> cmds_groups;
};

class LuaSystem : public ClassBase
{
public:
	CLASS_BODY(LuaSystem);
	REF static bool im_checkbox(std::string name, bool b);
	REF static bool im_button(std::string name);
	REF static bool im_drag_float(std::string name, float f, float speed, float min, float max);
	REF static bool im_input_int(std::string name, int i);
	REF static bool im_slider_int(std::string name, int i, int min, int max);
	REF static void im_separator();
	REF static void im_text(std::string name);
	REF static float im_get_last_float();
	REF static bool im_get_last_bool();
	REF static int im_get_last_int();

	// 0=append,1=now
	REF static void execute_command(int mode, std::string command);
	// 0=er,1=warn,2=info,3=debug
	REF static void sys_print(int type, std::string str) { ::sys_print(LogType(type), "%s", str.c_str()); }
	REF static float get_time_since_start() { return ::TimeSinceStart(); }
	REF static void log_fullscreen(int type, std::string str) {
		eng->log_to_fullscreen_gui(LogType(type), str.c_str());
	}
};