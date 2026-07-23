#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL3/SDL.h>
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include "GameEngineLocal.h"
#include "Level.h"
#include "AgentBridge/AgentBridge.h"
#include "IEditorTool.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Framework/ClassBase.h"
#include "Framework/MeshBuilder.h"
#include "Framework/Files.h"
#include "Render/DrawPublic.h"
#include "Render/IGraphicsDevice.h"
#include "Render/Texture.h"
#include "Render/MaterialPublic.h"
#include "Game/Entities/Player.h"
#include "Game/Entity.h"
#include "Game/LevelAssets.h"
#include "Game/Components/CameraComponent.h"
#include "Physics/Physics2.h"
#include "Assets/AssetBrowser.h"
#include "AssetTools/AssetDiagnostics.h"
#include "AssetTools/AssetCompiler.h"
#include "Sound/SoundPublic.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "Framework/EditorTheme.h"
#include "UI/UILoader.h"
#include "UI/BaseGUI.h"
#include "UI/OnScreenLogGui.h"
#include "UI/GUISystemPublic.h"
#include "UI/RmlUi/RmlUiSystem.h"
#include "Assets/AssetDatabase.h"
#include "Input/InputSystem.h"
#include "Render/RenderObj.h"
#include "Render/ModelManager.h"
#include "Framework/SysPrint.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Navigation/RuntimeNavManager.h"
#include "Framework/Jobs.h"
#include "EditorPopups.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"
#ifdef EDITOR_BUILD
#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/EditorRecents.h"
#endif
#include "Framework/StringUtils.h"
#include "EngineMain.h"
#include "EditorPopupTemplate.h"
#include "Animation/SkeletonData.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/TestRunner.h"
#include "IntegrationTests/StateDump.h"
#include "Framework/Util.h"
#include <mutex>

#include "Logging.h"

// Declared in EngineMain.cpp
extern double program_time_start;
extern ConfigVar g_window_w;
extern ConfigVar g_window_h;
extern ConfigVar g_project_name;
extern ConfigVar g_application_class;
extern ConfigVar developer_mode;
extern ConfigVar g_startup_project;
extern ConfigVar g_editor_cfg_folder;
double GetTime();
double TimeSinceStart();

// Forward declaration for init_debug_shape_ctx defined in EngineMain_Debug.cpp
extern void init_debug_shape_ctx();

#define TIMESTAMP(x)                                                                                                   \
	sys_print(Debug, "%s in %f\n", x, (float)GetTime() - start);                                                       \
	start = GetTime();

// ---------------------------------------------------------------------------
// Window size helpers
// ---------------------------------------------------------------------------

glm::ivec2 get_app_window_size() {
	ASSERT(g_window_w.get_integer() > 0 && g_window_h.get_integer() > 0);
	return {g_window_w.get_integer(), g_window_h.get_integer()};
}

void set_app_window_size(glm::ivec2 i) {
	ASSERT(i.x > 0 && i.y > 0);
	sys_print(Debug, "set window size %d %d\n", i.x, i.y);
	g_window_w.set_integer(i.x);
	g_window_h.set_integer(i.y);
}

// ---------------------------------------------------------------------------
// SDL window creation
// ---------------------------------------------------------------------------


void GameEngineLocal::init_sdl_window() {
	ASSERT(!window);

	sys_print(Info, "initializing window...\n");

	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO)) {
		sys_print(Error, "init sdl failed: %s\n", SDL_GetError());
		exit(-1);
	}

	const bool use_dx11 = !strcmp(g_render_backend.get_string(), "dx11");

	SDL_WindowFlags window_flags = SDL_WINDOW_RESIZABLE;
	if (!use_dx11) {
		// GL backend sets the GL context attribs (major/minor/depth/double-buffer)
		// before the window is created — see gfx_opengl_pre_window_setup docs.
		gfx_opengl_pre_window_setup();
		window_flags |= SDL_WINDOW_OPENGL;
	}

	const char* title = g_project_name.get_string();
	window = SDL_CreateWindow(title, g_window_w.get_integer(), g_window_h.get_integer(), window_flags);
	if (!window) {
		sys_print(Error, "create sdl window failed: %s\n", SDL_GetError());
		exit(-1);
	}

	if (use_dx11)
		gfx_init_dx11(window);
	else
		// Backend creates the GL context, loads glad, takes over swap-interval.
		gfx_init_opengl(window);

	// Diagnostic: log the refresh rate SDL/the driver actually negotiated for
	// this window's display, since it can silently differ from the monitor's
	// rated max (cable bandwidth, GPU control panel override, scaled res, etc).
	const SDL_DisplayID display_id = SDL_GetDisplayForWindow(window);
	const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display_id);
	if (mode)
		sys_print(Info, "display refresh rate: %.3f hz (%dx%d)\n", mode->refresh_rate, mode->w, mode->h);
	else
		sys_print(Error, "could not query display mode: %s\n", SDL_GetError());
}

// ---------------------------------------------------------------------------
// Internal init helpers
// ---------------------------------------------------------------------------

using std::make_unique;

static bool has_arg(int argc, char** argv, const char* flag) {
	ASSERT(argv);
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == flag)
			return true;
	return false;
}

static std::string current_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream oss;
	oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
		<< ms.count();
	return oss.str();
}

static std::string append_strings(int c, char** str) {
	ASSERT(str || c == 0);
	std::string out;
	for (int i = 0; i < c; i++) {
		out += str[i];
		out += " ";
	}
	return out;
}

// ---------------------------------------------------------------------------
// Editor tool open
// ---------------------------------------------------------------------------

#ifdef EDITOR_BUILD
namespace {
// Wraps a lambda so it can be handed to Cmd_Manager::append_cmd. The switch has to happen
// via the deferred systemCommands queue (drained in execute_buffer() at frame start), not
// synchronously from inside the popup's imgui callback: at that point in the frame other
// code (dock_over_viewport) has already cached the raw IEditorTool* for this frame, and
// destroying/replacing editor_tool out from under it is a use-after-free.
class LambdaSystemCommand : public SystemCommand
{
public:
	explicit LambdaSystemCommand(std::function<void()> f) : func(std::move(f)) {}
	void execute() override { func(); }
	string to_string() override { return "<deferred open_tool switch>"; }

private:
	std::function<void()> func;
};
} // namespace
#endif

void GameEngineLocal::open_tool(string mapname, const CameraSnapshot* restore_cam) {
	ASSERT(!mapname.empty());

#ifdef EDITOR_BUILD
	// Don't silently discard unsaved work: if the outgoing doc is dirty, prompt to
	// save/discard/cancel and only actually switch once that's resolved. Skip during
	// tests — they manage their own doc lifecycle and there's no user to answer a popup.
	if (!is_test_mode() && this->editor_tool && this->editor_tool->get_has_editor_changes()) {
		// Copy by value: restore_cam may point at a recents-list entry that could be
		// invalidated before the deferred switch actually runs.
		const bool has_cam = restore_cam != nullptr;
		const CameraSnapshot cam_copy = has_cam ? *restore_cam : CameraSnapshot();
		PopupTemplate::create_unsaved_changes_prompt(EditorPopupManager::inst, this->editor_tool.get(),
													 [this, mapname, cam_copy, has_cam]() {
														 Cmd_Manager::inst->append_cmd(
															 std::make_unique<LambdaSystemCommand>(
																 [this, mapname, cam_copy, has_cam]() {
																	 do_open_tool(mapname,
																				 has_cam ? &cam_copy : nullptr);
																 }));
													 });
		return;
	}
#endif

	do_open_tool(mapname, restore_cam);
}

void GameEngineLocal::do_open_tool(string mapname, const CameraSnapshot* restore_cam) {
#ifdef EDITOR_BUILD
	// Snapshot the doc we are about to leave BEFORE load_level runs — load_level
	// internally calls editor_tool.reset(), so by the time we get back here the
	// outgoing doc is already destroyed.
	std::string outgoing_path;
	CameraSnapshot outgoing_cam;
	if (auto* old = dynamic_cast<EditorDoc*>(this->editor_tool.get())) {
		outgoing_path = old->get_asset_path();
		if (!outgoing_path.empty() && outgoing_path != "<empty>" && outgoing_path != mapname)
			outgoing_cam = old->ed_cam.snapshot();
		else
			outgoing_path.clear();
	}
#endif

	const bool good = load_level(mapname);
	if (good) {
		if (this->editor_tool)
			sys_print(Debug, "EditorState::open_tool: replacing current tool\n");
		this->editor_tool.reset(IEditorTool::create(mapname));
#ifdef EDITOR_BUILD
		// Only commit to recents after the new doc actually loaded — failed
		// load_level leaves the old doc destroyed but we shouldn't pollute the
		// list with a switch that didn't really complete. Skip recording while
		// a test runner is active so integration tests don't churn the list.
		if (!outgoing_path.empty() && !is_test_mode())
			g_editor_recents.record(outgoing_path, outgoing_cam);
		if (restore_cam) {
			if (auto* new_doc = dynamic_cast<EditorDoc*>(this->editor_tool.get()))
				new_doc->ed_cam.apply_snapshot(*restore_cam);
		}
#endif
	} else {
		// Keep the current editor_tool intact so a failed open doesn't destroy
		// the user's in-progress document. load_level returns false WITHOUT
		// running insert_this_map_as_level when the asset is missing, so the
		// existing `level` is also untouched.
		sys_print(Error, "EditorState::open_tool: failed to load '%s', keeping current document\n", mapname.c_str());
	}
}

// ---------------------------------------------------------------------------
// Cleanup
// ---------------------------------------------------------------------------

void GameEngineLocal::cleanup() {
	ASSERT(true); // always valid to call cleanup
	agent_bridge_shutdown();
#ifdef EDITOR_BUILD
	// Snapshot the current doc into recents before tearing it down — open_tool
	// only records on doc-switch, so a user who opens one map and quits would
	// otherwise lose that map from the recents list.
	if (!is_test_mode()) {
		if (auto* doc = dynamic_cast<EditorDoc*>(editor_tool.get())) {
			const std::string path = doc->get_asset_path();
			if (!path.empty() && path != "<empty>")
				g_editor_recents.record(path, doc->ed_cam.snapshot());
		}
	}
	if (editor_tool)
		editor_tool.reset();
#endif
	if (level) {
		stop_game();
	}

	if (app) {
		app->stop();
		app.reset();
	}

	isound->cleanup();

	// could get fatal error before initializing this stuff
	if (window) {
		if (gfx_is_initialized()) {
			if (RmlUiSystem::inst) {
				RmlUiSystem::inst->shutdown();
				delete RmlUiSystem::inst;
				RmlUiSystem::inst = nullptr;
			}
			gfx().rmlui_shutdown();
			gfx().imgui_shutdown();
		}
		gfx_shutdown();
		SDL_DestroyWindow(window);
	}
}

// ---------------------------------------------------------------------------
// Engine init — subsystem startup
// ---------------------------------------------------------------------------

void GameEngineLocal::init(MainConfigurationOptions& options, int argc, char** argv) {
	ASSERT(argc >= 1 && argv);

	this->argc = argc;
	this->argv = argv;
	this->m_is_editor_app = options.editor_mode;

	FileSys::init();

	// log_file/dumps live under Logs/ (see EngineMain.h) — must exist before the Logger opens
	// its file below.
	FileSys::create_directory("Logs", FileSys::ENGINE_DIR);

	Logger::inst = Logger::make_logger(options.log_file, options.no_console_print);

	sys_print(Debug, "%s\n", append_strings(argc, argv).c_str());
	sys_print(Debug, "%s\n", current_timestamp().c_str());

	sys_print(Info, "--------- Initializing Engine ---------\n");

	program_time_start = GetTime();
	double start = GetTime();
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		sys_print(Debug, "init % s in % fs\n", msg, float(now - start));
		start = now;
	};

	Debug_Console::inst = new Debug_Console;
	Cmd_Manager::inst = Cmd_Manager::create();
	add_commands();
	print_time("init commands");

	// must come first
	ClassBase::init_classes_startup();
	print_time("init class system");

	level = nullptr;
	tick_interval = 1.0 / 60.0;
	is_hosting_game = false;

	// Loaded before init_sdl_window() so g_render_backend (EngineVars.ini) is known
	// before window/context creation (DX11 needs no SDL_WINDOW_OPENGL flag or
	// GL context attribs; OpenGL needs both set up pre-window-creation).
	Cmd_Manager::inst->set_set_unknown_variables(true);
	if (!options.vars_section.empty())
		Cmd_Manager::inst->execute_file_section(Cmd_Execute_Mode::NOW, options.vars_file.c_str(),
											   options.vars_section.c_str());
	else
		Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, options.vars_file.c_str());
	print_time("execute vars file");

	// Project .ini is always applied on top of vars_file, never for --tests runs (those stay
	// self-contained in the game_test/editor_test sections). --project on the CLI wins over the
	// `startup_project` cvar that vars_file just set.
	if (!options.pending_test_runnner) {
		const std::string project_file = resolve_project_ini(
			!options.project_file.empty() ? options.project_file : g_startup_project.get_string());
		if (!project_file.empty())
			Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, project_file.c_str());
		print_time("execute project file");
	}

	init_sdl_window();
	print_time("init sdl window");

	prof::Profiler::init();

	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;
	int i = 1;
	auto needs_value = [&](const char* flag) {
		if (i + 1 >= argc) {
			sys_print(Error, "missing value for %s\n", flag);
			return false;
		}
		return true;
	};
	for (; i < argc; i++) {
		if (strcmp(argv[i], "-x") == 0) {
			if (needs_value("-x")) startx = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-y") == 0) {
			if (needs_value("-y")) starty = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-w") == 0) {
			if (needs_value("-w")) g_window_w.set_integer(atoi(argv[++i]));
		} else if (strcmp(argv[i], "-h") == 0) {
			if (needs_value("-h")) g_window_h.set_integer(atoi(argv[++i]));
		} else if (strcmp(argv[i], "-VISUALSTUDIO") == 0) {
			const char* projName = g_project_name.get_string();
			SDL_SetWindowTitle(window, string_format("%s - VISUAL STUDIO\n", projName));
		} else if (argv[i][0] == '-') {
			string cmd;
			const int start_i = i;
			cmd += &argv[i++][1];
			while (i < argc && argv[i][0] != '-') {
				cmd += ' ';
				cmd += argv[i++];
			}
			i = glm::max(start_i, i - 1);
			Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, cmd.c_str());
		}
	}

	// after argv-driven cvar overrides (e.g. -agentbridge.enabled 0) have already applied
	agent_bridge_init();

	g_assets.init();
	print_time("asset init");

	JobSystem::inst = new JobSystem(); // spawns worker threads
	print_time("job sys init");

	ScriptManager::inst = new ScriptManager();
	const bool lua_debug_wait = has_arg(argc, argv, "--lua_debug_wait");
	ScriptManager::inst->load_script_files(has_arg(argc, argv, "--lua_debug") || lua_debug_wait, lua_debug_wait);
	if (options.pending_test_runnner)
		ScriptManager::inst->load_test_scripts();
	print_time("script init");

	// renderer init
	idraw->init();
	print_time("draw init");

	SchemaManager::get().init();
	Input::inst = new Input();
	UiSystem::inst = new UiSystem();
	EditorPopupManager::inst = new EditorPopupManager();

	g_physics.init();
	print_time("physics init");

	imaterials->init();
	print_time("mat init");

	isound->init();
	g_modelMgr.init();
	GameAnimationMgr::inst = new GameAnimationMgr;
	ParticleMgr::inst = new ParticleMgr;
	RuntimeNavManager::inst = new RuntimeNavManager;

	print_time("init mods,sounds");

	imgui_context = ImGui::CreateContext();
	init_debug_shape_ctx();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	// io.IniFilename is stored as a raw pointer (not copied), so this needs to outlive the
	// context; static is fine since there's exactly one imgui context for the app's lifetime.
	static const std::string imgui_ini_path =
		FileSys::get_full_path_from_relative(std::string(g_editor_cfg_folder.get_string()) + "/imgui.ini",
											 FileSys::ENGINE_DIR);
	ImGui::GetIO().IniFilename = imgui_ini_path.c_str();
	gfx().imgui_init();
	print_time("imgui init");

	gfx().rmlui_init();
	RmlUiSystem::inst = new RmlUiSystem();
	RmlUiSystem::inst->init();
	print_time("rmlui init");

#ifdef EDITOR_BUILD
	g_editor_recents.load();
#endif

	auto build_imgui_fonts = []() {
		auto add_font_to_imgui = [&](std::string data_path, std::vector<float> sizes) -> void {
			auto path = FileSys::get_full_path_from_game_path(data_path.c_str());
			if (FileSys::does_file_exist(path.c_str(), FileSys::FULL_SYSTEM)) {
				for (auto s : sizes)
					ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), s);
			} else {
				sys_print(Error, ".ttf for imgui not found: %s\n", path.c_str());
			}
		};
		add_font_to_imgui("eng/inconsolata_bold.ttf", {14.0, 24.0});
		add_font_to_imgui("inter_regular.ttf", {14.0, 18.0});
		// Consolas 12pt — lighter monospace for property name labels
		ImGui::GetIO().Fonts->AddFontFromFileTTF("C:/Windows/Fonts/consola.ttf", 12.0f);
		ImGui::GetIO().Fonts->Build();
	};
	build_imgui_fonts();
	{
		auto& flist = *ImGui::GetIO().Fonts;
		g_prop_bold_font    = flist.Fonts.Size > 0 ? flist.Fonts[0] : nullptr; // inconsolata bold 14pt — group headers
		g_prop_regular_font = flist.Fonts.Size > 4 ? flist.Fonts[4] : nullptr; // consolas 12pt — property labels
	}
	apply_editor_dark_theme();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	// Ctrl+Tab is repurposed as the editor's recent-doc switcher (EditorDoc::check_recent_switcher_input);
	// free it from imgui's built-in window-windowing-overlay switcher so the two don't fight over input
	// and the overlay doesn't render on top of our popup.
	ImGui::GetCurrentContext()->ConfigNavWindowingKeyNext = ImGuiKey_None;
	ImGui::GetCurrentContext()->ConfigNavWindowingKeyPrev = ImGuiKey_None;
	print_time("imgui font");

	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());

	auto start_game = [&]() {
		sys_print(Info, "starting game...\n");
		Application* a = ClassBase::create_class<Application>(g_application_class.get_string());
		if (!a) {
			Fatalf("Engine::init: application class not found %s. set it in EngineVars.ini or the project ini\n",
				   g_application_class.get_string());
		}
		app.reset(a);
		Input::on_con_status.add(a, [this](int i, bool b) { app->on_controller_status(i, b); });
		Model::on_model_loaded.add(this, [this](Model* m) { app->on_post_model_load(m); });
		MaterialInstance::on_material_loaded.add(this, [this](MaterialInstance* m) {
			try {
				app->on_post_material_load(m);
			}
			catch (LuaRuntimeError& er) {
				sys_print(Error, "on_post_material_load %s\n", er.what());
			}
		});

		app->start();
	};
#ifdef EDITOR_BUILD
	AssetRegistrySystem::get().init();

	if (m_is_editor_app) {
		AssetBrowser::inst = new AssetBrowser();
		loaded_in_tool_mode = true;
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		AssetDiagnostics::get().load();
		AssetDiagnostics::get().scan_all();
		AssetCompiler::register_console_commands();
		print_time("asset reg and browser init");
	} else {
		start_game();
	}
#else
	start_game();
#endif

	// inject pending test runner
	if (options.pending_test_runnner.get()) {
		set_runner(options.pending_test_runnner.release(), options.skip_swap);
		if (get_app()) {
			ASSERT(!m_is_editor_app);
		}
	}

	Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, options.init_file.c_str());
	print_time("execute init");
	Cmd_Manager::inst->set_set_unknown_variables(false);

	print_time("load default bundle");

	sys_print(Info, "execute startup in %f\n", (float)TimeSinceStart());

}
