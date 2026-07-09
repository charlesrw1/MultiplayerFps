#define IMGUI_DEFINE_MATH_OPERATORS

#include <SDL3/SDL.h>
#include "glad/glad.h"
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "GameEngineLocal.h"
#include "Level.h"
#include "IEditorTool.h"
#include "User_Camera.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Framework/ClassBase.h"
#include "Framework/MeshBuilder.h"
#include "Framework/Files.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Game/Entities/Player.h"
#include "Game/Entity.h"
#include "Game/LevelAssets.h"
#include "Game/Components/CameraComponent.h"
#include "Physics/Physics2.h"
#include "Assets/AssetBrowser.h"
#include "Sound/SoundPublic.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl3.h"
#include "Framework/EditorTheme.h"
#include "UI/UILoader.h"
#include "UI/Widgets/Layouts.h"
#include "UI/OnScreenLogGui.h"
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Input/InputSystem.h"
#include "Render/RenderObj.h"
#include "Render/ModelManager.h"
#include "Framework/SysPrint.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Framework/Jobs.h"
#include "EditorPopups.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/StringUtils.h"
#include "EngineMain.h"
#include "EditorPopupTemplate.h"
#include "Animation/SkeletonData.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/TestRunner.h"
#include "IntegrationTests/StateDump.h"
#include "Framework/Util.h"
#include <mutex>

// ---------------------------------------------------------------------------
// Global singletons
// ---------------------------------------------------------------------------


Debug_Console* Debug_Console::inst = nullptr;

GameEngineLocal eng_local;
GameEnginePublic* eng = &eng_local;

std::string get_cli_arg(const std::string& name, const std::string& default_value) {
	const int argc = eng_local.argc;
	char** argv = eng_local.argv;
	if (!argv) return default_value;
	for (int i = 1; i < argc; i++) {
		if (argv[i][0] != '-') continue;
		if (name == argv[i] + 1) {
			if (i + 1 >= argc) return default_value;
			if (argv[i + 1][0] == '-') return default_value;
			return argv[i + 1];
		}
	}
	return default_value;
}

double program_time_start;

// ---------------------------------------------------------------------------
// Config vars
// ---------------------------------------------------------------------------

extern ConfigVar log_destroy_game_objects;
extern ConfigVar log_all_asset_loads;
extern ConfigVar developer_mode;
extern ConfigVar log_shader_compiles;
extern ConfigVar material_print_debug;
extern ConfigVar g_project_name;
ConfigVar g_editor_cfg_folder("g_editor_cfg_folder", "Cfg", CVAR_DEV,
							  "what folder to save .ini and other editor cfg to");
ConfigVar loglevel("loglevel", "4", CVAR_INTEGER, "(0=disable,4=all)", 0, 4);
ConfigVar colorLog("colorLog", "1", CVAR_BOOL, "");
ConfigVar g_application_class("g_application_class", "Application", CVAR_DEV, "");
ConfigVar with_threading("with_threading", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar g_drawconsole("drawconsole", "0", CVAR_BOOL, "draw the console");
ConfigVar g_project_name("g_project_name", "CsRemakeEngine", CVAR_DEV,
						 "the project name of the game, used for save file folders");
ConfigVar g_mousesens("g_mousesens", "0.005", CVAR_FLOAT, "", 0.0, 1.0);
ConfigVar g_fov("fov", "70.0", CVAR_FLOAT, "", 10.0, 110.0);
ConfigVar g_thirdperson("thirdperson", "70.0", CVAR_BOOL, "");
ConfigVar g_fakemovedebug("fakemovedebug", "0", CVAR_INTEGER, "", 0, 2);
ConfigVar g_drawimguidemo("g_drawimguidemo", "0", CVAR_BOOL, "");
ConfigVar g_debug_skeletons("g_debug_skeletons", "0", CVAR_BOOL, "draw skeletons of active animated renderables");
ConfigVar g_draw_grid("g_draw_grid", "0", CVAR_BOOL, "draw a debug grid around the origin");
ConfigVar g_grid_size("g_grid_size", "1", CVAR_FLOAT, "size of g_draw_grid", 0.01, 10);
ConfigVar ed_default_sky_material("ed_default_sky_material", "eng/hdriSky.mm", CVAR_DEV,
								  "default sky material used for editors");
ConfigVar g_drawdebugmenu("g_drawdebugmenu", "0", CVAR_BOOL, "draw the debug menu");
ConfigVar g_window_w("vid.width", "1200", CVAR_INTEGER, "", 1, 4000);
ConfigVar g_window_h("vid.height", "800", CVAR_INTEGER, "", 1, 4000);
ConfigVar g_window_fullscreen("vid.fullscreen", "0", CVAR_BOOL, "");
ConfigVar g_host_port("net.hostport", "47000", CVAR_INTEGER | CVAR_READONLY, "", 0, UINT16_MAX);
ConfigVar g_dontsimphysics("stop_physics", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar developer_mode("developer_mode", "1", CVAR_DEV | CVAR_BOOL,
						 "enables dev mode features like compiling assets when loading");
ConfigVar g_slomo("slomo", "1.0", CVAR_FLOAT | CVAR_DEV, "multiplier of dt in update loop", 0.0001, 5.0);
ConfigVar g_engine_ticks_per_frame("engine.ticks_per_frame", "1", CVAR_INTEGER,
    "game ticks to simulate per render frame; render+present are skipped for all but the last", 1, 10000);
ConfigVar ui_disable_screen_log("ui.disable_screen_log", "0", CVAR_BOOL, "");
ConfigVar stat_fps("stat.fps", "0", CVAR_BOOL, "show fps/frametime counter overlay in the top-right corner");
ConfigVar disable_render_time_tick("disable_render_time_tick", "0", CVAR_BOOL, "");

// ---------------------------------------------------------------------------
// Time utilities
// ---------------------------------------------------------------------------

double GetTime() {
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart() {
	ASSERT(program_time_start > 0.0);
	return GetTime() - program_time_start;
}

// ---------------------------------------------------------------------------
// Model event hooks
// ---------------------------------------------------------------------------



// ---------------------------------------------------------------------------
// Quit
// ---------------------------------------------------------------------------

void Quit() {
	// blah blah
	sys_print(Info, "Quiting... (runtime %f)\n", TimeSinceStart());
	eng_local.cleanup();
	exit(0);

	//asdfasdf

	printf("");
}

// ---------------------------------------------------------------------------
// Imgui helpers
// ---------------------------------------------------------------------------

int imgui_std_string_resize(ImGuiInputTextCallbackData* data) {
	ASSERT(data);
	std::string* user = (std::string*)data->UserData;
	assert(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		user->resize(data->BufSize);
		data->Buf = (char*)user->data();
	}

	return 0;
}

bool std_string_input_text(const char* label, std::string& str, int flags) {
	ASSERT(label);
	return ImGui::InputText(label, (char*)str.c_str(), str.size() + 1, flags | ImGuiInputTextFlags_CallbackResize,
							imgui_std_string_resize, &str);
}

// ---------------------------------------------------------------------------
// Engine log / SDL error
// ---------------------------------------------------------------------------

void GameEngineLocal::log_to_fullscreen_gui(LogType type, const char* msg) {
	ASSERT(msg);
	gui_log.add_text(get_color_of_print(type), msg);
}

static void SDLError(const char* msg) {
	ASSERT(msg);
	printf(" %s: %s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}

// ---------------------------------------------------------------------------
// Test runner injection
// ---------------------------------------------------------------------------

void GameEngineLocal::set_runner(ITestRunner* runner, bool skip_swap_) {
	ASSERT(runner);
	test_runner = runner;
	skip_swap = skip_swap_;
}

// ---------------------------------------------------------------------------
// Test-mode argument parsing
// ---------------------------------------------------------------------------

// --tests <mode> [pattern...]   pattern may be a glob or "@file" reading newline-separated globs.
struct TestModeArgs {
	bool present = false;
	std::string mode; // "game" or "editor"
	std::vector<std::string> patterns;
	bool promote = false;
	bool interactive = false;
	bool timing_assert = false;
	bool editor = false;              // --editor (launch editor app)
	bool wait_for_debugger = false;   // --wait-for-debugger
};

static void read_pattern_file(const char* path, std::vector<std::string>& out) {
	ASSERT(path);
	std::ifstream in(path);
	if (!in) {
		fprintf(stderr, "[--tests] could not open pattern file: %s\n", path);
		return;
	}
	std::string line;
	while (std::getline(in, line)) {
		// strip CR + leading/trailing whitespace
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
			line.pop_back();
		size_t start = 0;
		while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
			++start;
		if (start)
			line.erase(0, start);
		if (line.empty() || line[0] == '#')
			continue;
		out.push_back(line);
	}
}

static TestModeArgs parse_test_mode_args(int argc, char** argv) {
	ASSERT(argc >= 1 && argv);
	TestModeArgs out;
	for (int i = 1; i < argc; ++i) {
		std::string a = argv[i];
		if (a == "--tests") {
			out.present = true;
			if (i + 1 < argc)
				out.mode = argv[++i];
			// consume positional patterns until next "--flag"
			while (i + 1 < argc) {
				std::string p = argv[i + 1];
				if (p.size() >= 2 && p[0] == '-' && p[1] == '-')
					break;
				++i;
				if (!p.empty() && p[0] == '@')
					read_pattern_file(p.c_str() + 1, out.patterns);
				else
					out.patterns.push_back(p);
			}
		} else if (a == "--promote") {
			out.promote = true;
		} else if (a == "--interactive") {
			out.interactive = true;
		} else if (a == "--timing-assert") {
			out.timing_assert = true;
		} else if (a == "--editor") {
			out.editor = true;
		} else if (a == "--wait-for-debugger") {
			out.wait_for_debugger = true;
		}
	}
	return out;
}

// ---------------------------------------------------------------------------
// Top-level entry point
// ---------------------------------------------------------------------------
extern void wait_for_debugger_windows();
int game_engine_main(MainConfigurationOptions& options, int argc, char** argv) {
	ASSERT(argc >= 1 && argv);
	const TestModeArgs test_args = parse_test_mode_args(argc, argv);

	if (test_args.wait_for_debugger) {
		wait_for_debugger_windows();
	}
	install_crash_handler();

	options.editor_mode = test_args.editor;
	if (test_args.present) {
		eng_local.m_is_test_mode = true;
		if (test_args.mode != "game" && test_args.mode != "editor") {
			fprintf(stderr, "Usage: App.exe --tests <game|editor> [pattern...]   (got mode '%s')\n",
					test_args.mode.c_str());
			return 1;
		}
		options.vars_section = (test_args.mode == "editor") ? "editor_test" : "game_test";
		options.log_file = std::string("test_") + test_args.mode + "_output.log";
		// Static so the C-string outlives this scope — set_assert_log_path stores
		// the pointer.
		static std::string s_log_file = options.log_file;
		set_assert_log_path(s_log_file.c_str());
		if (test_args.mode == "editor") {
			options.init_file = "";
			options.editor_mode = true;
		}



		set_assert_hook([](const char* cond) {
			fprintf(stderr, "\n[ASSERT FAILED] %s\n", cond);
			print_stack_trace();
		});

		const TestMode mode = (test_args.mode == "editor") ? TestMode::Editor : TestMode::Game;
		auto tests = TestRegistry::get_filtered(mode, test_args.patterns);

		TestRunnerConfig cfg;
		cfg.promote = test_args.promote;
		cfg.interactive = test_args.interactive;
		cfg.timing_assert = test_args.timing_assert;
		auto runner = std::make_unique<TestRunner>(mode, std::move(tests), cfg);
		runner->set_lua_patterns(test_args.patterns);
		options.pending_test_runnner = std::move(runner);
	}

	eng_local.init(options, argc, argv);
	eng_local.loop();
	eng_local.cleanup();
	return 0;
}

// ---------------------------------------------------------------------------
// Engine tick rate
// ---------------------------------------------------------------------------

void GameEngineLocal::set_tick_rate(float tick_rate) {
	ASSERT(tick_rate > 0.f);
	tick_interval = 1.0 / tick_rate;
}

// ---------------------------------------------------------------------------
// Key event handling
// ---------------------------------------------------------------------------

void GameEngineLocal::key_event(SDL_Event event) {
	ASSERT(UiSystem::inst && Debug_Console::inst && EditorPopupManager::inst);

	if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;

		if (!UiSystem::inst->is_vp_focused() && Debug_Console::inst->get_is_console_focused()) {
			UiSystem::inst->set_focus_to_viewport();
		} else {
			Debug_Console::inst->toggle_set_focus();
		}
	}

	if ((event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

	if (EditorPopupManager::inst->has_popup_open())
		return;

	if (event.type == SDL_EVENT_KEY_DOWN) {
		SDL_Scancode scancode = event.key.scancode;
		vector<string>* keybinds = find_keybinds(scancode, event.key.mod);
		if (keybinds != nullptr) {
			for (string& k : *keybinds)
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, k.c_str());
		}
	}
}

// ---------------------------------------------------------------------------
// GameEngineLocal constructor
// ---------------------------------------------------------------------------

GameEngineLocal::GameEngineLocal() {}

// ---------------------------------------------------------------------------
// Application::get_app
// ---------------------------------------------------------------------------

Application* Application::get_app() {
	ASSERT(eng);
	return eng->get_app();
}


