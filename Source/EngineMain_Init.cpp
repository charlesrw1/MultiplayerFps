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
#include "Navigation/RuntimeNavManager.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"
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
extern void add_events_test(Model* model);
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

void GameEngineLocal::open_tool(string mapname) {
	ASSERT(!mapname.empty());

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
		if (gfx_is_initialized())
			gfx().imgui_shutdown();
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

	// Loaded before init_sdl_window() so g_render_backend (vars.txt) is known
	// before window/context creation (DX11 needs no SDL_WINDOW_OPENGL flag or
	// GL context attribs; OpenGL needs both set up pre-window-creation).
	Cmd_Manager::inst->set_set_unknown_variables(true);
	if (!options.vars_section.empty())
		Cmd_Manager::inst->execute_file_section(Cmd_Execute_Mode::NOW, options.vars_file.c_str(),
											   options.vars_section.c_str());
	else
		Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, options.vars_file.c_str());
	print_time("execute vars file");

	init_sdl_window();
	print_time("init sdl window");

	Profiler::init();

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

	g_assets.init();
	print_time("asset init");

	JobSystem::inst = new JobSystem(); // spawns worker threads
	print_time("job sys init");

	ScriptManager::inst = new ScriptManager();
	ScriptManager::inst->load_script_files();
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

	Model::on_model_loaded.add(this, [](Model* mod) { add_events_test(mod); });
	print_time("init mods,sounds");

	imgui_context = ImGui::CreateContext();
	init_debug_shape_ctx();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	gfx().imgui_init();
	print_time("imgui init");

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
		ImGui::GetIO().Fonts->Build();
	};
	build_imgui_fonts();
	apply_editor_dark_theme();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	print_time("imgui font");

	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());

	auto start_game = [&]() {
		sys_print(Info, "starting game...\n");
		Application* a = ClassBase::create_class<Application>(g_application_class.get_string());
		if (!a) {
			Fatalf("Engine::init: application class not found %s. set it in vars.txt\n",
				   g_application_class.get_string());
		}
		app.reset(a);
		Input::on_con_status.add(a, [this](int i, bool b) { app->on_controller_status(i, b); });
		Model::on_model_loaded.add(this, [this](Model* m) { app->on_post_model_load(m); });
		MaterialInstance::on_material_loaded.add(this, [this](MaterialInstance* m) {
			try {
				app->on_post_material_load(m);
			}
			catch (LuaRuntimeError er) {
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
