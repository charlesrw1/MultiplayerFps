#define IMGUI_DEFINE_MATH_OPERATORS
#include <SDL2/SDL.h>
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
#include "glm/gtc/matrix_transform.hpp"
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
#include "imgui_impl_sdl2.h"
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
#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"
#include "Framework/Jobs.h"
#include "EditorPopups.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Framework/StringUtils.h"
#include "Scripting/ScriptManager.h"
#include "EngineMain.h"
#include "EditorPopupTemplate.h"
#include "Animation/SkeletonData.h"
#include "IntegrationTests/TestRegistry.h"
#include "IntegrationTests/TestRunner.h"
#include "IntegrationTests/StateDump.h"
#include "Framework/Util.h"
#include <mutex>
#include <fstream>

#include "Logging.h"

Debug_Console* Debug_Console::inst = nullptr;

GameEngineLocal eng_local;
GameEnginePublic* eng = &eng_local;

static double program_time_start;

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
ConfigVar is_editor_app("is_editor_app", "0", CVAR_BOOL, "");
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
// defualt sky material to use for editors like materials/models/etc.
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

double GetTime() {
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart() {
	return GetTime() - program_time_start;
}

void add_player_model_events(MSkeleton* skel) {
	if (auto runClip = skel->find_clip("RUN")) {
		runClip->directplayopt.slotname = StringName("TheSlot");
	}
}

void add_events_test(Model* model) {
	if (model->get_name() == "SWAT_model.cmdl") {
		auto skel = model->get_skel();
		if (skel) {
			add_player_model_events(skel);
		}
	}
}

void Quit() {
	sys_print(Info, "Quiting... (runtime %f)\n", TimeSinceStart());
	eng_local.cleanup();
	exit(0);
}

int imgui_std_string_resize(ImGuiInputTextCallbackData* data) {
	std::string* user = (std::string*)data->UserData;
	assert(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		user->resize(data->BufSize);
		data->Buf = (char*)user->data();
	}

	return 0;
}
bool std_string_input_text(const char* label, std::string& str, int flags) {
	return ImGui::InputText(label, (char*)str.c_str(), str.size() + 1, flags | ImGuiInputTextFlags_CallbackResize,
							imgui_std_string_resize, &str);
}

void GameEngineLocal::log_to_fullscreen_gui(LogType type, const char* msg) {
	gui_log.add_text(get_color_of_print(type), msg);
}

static void SDLError(const char* msg) {
	printf(" %s: %s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}

void GameEngineLocal::set_runner(ITestRunner* runner, bool skip_swap_) {
	test_runner = runner;
	skip_swap = skip_swap_;
}

// --tests <mode> [pattern...]   pattern may be a glob or "@file" reading newline-separated globs.
struct TestModeArgs {
	bool present = false;
	std::string mode; // "game" or "editor"
	std::vector<std::string> patterns;
	bool promote = false;
	bool interactive = false;
	bool timing_assert = false;
};

static void read_pattern_file(const char* path, std::vector<std::string>& out) {
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
		}
	}
	return out;
}

int game_engine_main(MainConfigurationOptions& options, int argc, char** argv) {
	const TestModeArgs test_args = parse_test_mode_args(argc, argv);
	if (test_args.present) {
		if (test_args.mode != "game" && test_args.mode != "editor") {
			fprintf(stderr, "Usage: App.exe --tests <game|editor> [pattern...]   (got mode '%s')\n",
					test_args.mode.c_str());
			return 1;
		}
		options.vars_section = (test_args.mode == "editor") ? "editor_test" : "game_test";
		options.log_file = std::string("test_") + test_args.mode + "_output.log";
		// editor mode loaded its tool harness without an init.txt previously
		if (test_args.mode == "editor")
			options.init_file = "";

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

void GameEngineLocal::set_tick_rate(float tick_rate) {
	//	if (state == Engine_State::Game) {
	//	sys_print(Warning, "Can't change tick rate while running\n");
	//	return;
	//	}
	tick_interval = 1.0 / tick_rate;
}

void GameEngineLocal::key_event(SDL_Event event) {
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;

		if (!UiSystem::inst->is_vp_focused() && Debug_Console::inst->get_is_console_focused()) {
			UiSystem::inst->set_focus_to_viewport();
		} else {
			Debug_Console::inst->toggle_set_focus();
		}
	}

	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

	if (EditorPopupManager::inst->has_popup_open())
		return;

	if (event.type == SDL_KEYDOWN) {
		// check keybind activation
		SDL_Scancode scancode = event.key.keysym.scancode;
		vector<string>* keybinds = find_keybinds(scancode, event.key.keysym.mod);
		if (keybinds != nullptr) {
			for (string& k : *keybinds)
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, k.c_str());
		}
	}
}

void GameEngineLocal::cleanup() {
#ifdef EDITOR_BUILD
	// assert(0);
	// if (get_current_tool())
	//	get_current_tool()->close();
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

	g_assets.quit();

	// could get fatal error before initializing this stuff
	if (gl_context && window) {
		// NetworkQuit();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}
}

bool GameEngineLocal::is_drawing_to_window_viewport() const {
	return !UiSystem::inst->is_drawing_to_screen();
}

glm::ivec2 get_app_window_size() {
	return {g_window_w.get_integer(), g_window_h.get_integer()};
}
void set_app_window_size(glm::ivec2 i) {
	sys_print(Debug, "set window size %d %d\n", i.x, i.y);
	g_window_w.set_integer(i.x);
	g_window_h.set_integer(i.y);
}
static bool scene_hovered = false;

#include "Framework/MyImguiLib.h"
void GameEngineLocal::draw_any_imgui_interfaces() {}
ConfigVar disable_render_time_tick("disable_render_time_tick", "0", CVAR_BOOL, "");
void GameEngineLocal::get_draw_params(SceneDrawParamsEx& params, View_Setup& setup) {

	const float time_to_use = (disable_render_time_tick.get_bool()) ? 0 : time;

	params = SceneDrawParamsEx(time_to_use, frame_time);
	params.output_to_screen = UiSystem::inst->is_drawing_to_screen();
	// so the width/height parameters are valid
	View_Setup vs_for_gui;
	auto viewport = UiSystem::inst->get_vp_rect().get_size(); // get_game_viewport_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;

#ifdef EDITOR_BUILD
	// draw general ui
	if (editor_tool) {
		params.is_editor = true; // draw to the id buffer for mouse picking
		auto vs = editor_tool->get_vs();

		// fixme

		if (!vs) {
			params.draw_world = false;
			vs = &vs_for_gui;
		}
		isound->set_listener_position(vs->origin, glm::normalize(glm::cross(vs->front, glm::vec3(0, 1, 0))));
		setup = *vs;
		// idraw->scene_draw(params, *vs, get_gui());
	} else
#endif
	{
		params = SceneDrawParamsEx(time_to_use, frame_time);
		params.output_to_screen = UiSystem::inst->is_drawing_to_screen();
		View_Setup vs_for_gui;
		auto viewport = UiSystem::inst->get_vp_rect().get_size();
		vs_for_gui.width = viewport.x;
		vs_for_gui.height = viewport.y;

		CameraComponent* scene_camera = CameraComponent::get_scene_camera();

		// no camera
		if (!scene_camera) {
			params.draw_world = false;
			params.draw_ui = true;
			setup = vs_for_gui;
		} else {

			glm::mat4 view;
			float fov = 60.f;
			scene_camera->get_view(view, fov);

			glm::mat4 in = glm::inverse(view);
			auto pos = in[3];
			auto front = -in[2];
			View_Setup vs = View_Setup(view, glm::radians(fov), 0.01, 100.0, viewport.x, viewport.y);
			scene_camera->last_vs = vs;
			setup = vs;

			// fixme
			isound->set_listener_position(vs.origin, glm::normalize(glm::cross(vs.front, glm::vec3(0, 1, 0))));
		}
	}
}

// RH, reverse Z, infinite far plane perspective matrix

glm::mat4 View_Setup::make_opengl_perspective_with_near_far() const {
	return glm::perspectiveRH_NO(fov, width / (float)height, near, far);
}

View_Setup::View_Setup(glm::mat4 viewMat, float fov, float near, float far, int width, int height)
	: view(viewMat), fov(fov), near(near), far(far), width(width), height(height) {
	auto inv = glm::inverse(viewMat);
	this->origin = inv[3];
	this->front = -inv[2];
	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);
	viewproj = proj * view;
}

View_Setup::View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height)
	: origin(origin), front(front), fov(fov), near(near), far(far), width(width), height(height) {
	view = glm::lookAt(origin, origin + front, glm::vec3(0, 1.f, 0));
	// proj = glm::perspective(fov, width / (float)height, near, far);

	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);

	viewproj = proj * view;
}

#define TIMESTAMP(x)                                                                                                   \
	sys_print(Debug, "%s in %f\n", x, (float)GetTime() - start);                                                       \
	start = GetTime();

GameEngineLocal::GameEngineLocal() {}

#include <tracy/public/tracy/TracyOpenGL.hpp>
void GameEngineLocal::init_sdl_window() {
	ASSERT(!window);

	sys_print(Info, "initializing window...\n");

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		sys_print(Error, "init sdl failed: %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	const char* title = g_project_name.get_string();
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, g_window_w.get_integer(),
							  g_window_h.get_integer(), SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		sys_print(Error, "create sdl window failed: %s\n", SDL_GetError());
		exit(-1);
	}

	gl_context = SDL_GL_CreateContext(window);

	sys_print(Debug, "OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	sys_print(Debug, "Vendor: %s\n", glGetString(GL_VENDOR));
	sys_print(Debug, "Renderer: %s\n", glGetString(GL_RENDERER));
	sys_print(Debug, "Version: %s\n\n", glGetString(GL_VERSION));

	// init tracy profiling for opengl
	// TracyGpuContext;
	// TracyGpuZone("Test");
	// glClear(GL_COLOR_BUFFER_BIT);
	// TracyGpuCollect;

	SDL_GL_SetSwapInterval(0);
}

using std::make_unique;

static bool has_arg(int argc, char** argv, const char* flag) {
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == flag)
			return true;
	return false;
}
static std::string current_timestamp() {
	auto now = std::chrono::system_clock::now();
	auto time_t_now = std::chrono::system_clock::to_time_t(now);

	// For milliseconds
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

	std::ostringstream oss;
	// Format: YYYY-MM-DD HH:MM:SS.mmm
	oss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3)
		<< ms.count();

	return oss.str();
}
static std::string append_strings(int c, char** str) {
	std::string out;
	for (int i = 0; i < c; i++) {
		out += str[i];
		out += " ";
	}
	return out;
}
void GameEngineLocal::open_tool(string mapname) {

	const bool good = load_level(mapname);
	if (good) {
		if (this->editor_tool)
			sys_print(Debug, "EditorState::open_tool: replacing current tool\n");
		this->editor_tool.reset(IEditorTool::create(mapname));
	}
	else {
		// sys_print(Warning, "CreateLevelEditorAync::execute: failed to load map (%s)\n",
		// assetPath.value_or("<unnamed>").c_str());
		sys_print(Warning, "EditorState::open_tool: creation returned null\n");
		this->editor_tool = nullptr;
	}
}

// Forward declaration for init_debug_shape_ctx defined in EngineMain_Debug.cpp
extern void init_debug_shape_ctx();

void GameEngineLocal::init(MainConfigurationOptions& options, int argc, char** argv) {
	this->argc = argc;
	this->argv = argv;

	FileSys::init();

	Logger::inst = Logger::make_logger(options.log_file, options.no_console_print);

	sys_print(Debug, "%s\n", append_strings(argc, argv).c_str());
	sys_print(Debug, "%s\n", current_timestamp().c_str());

	sys_print(Info, "--------- Initializing Engine ---------\n");

	program_time_start = GetTime();
	double start = GetTime();
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		// printf("-----TIME %s %f\n", msg, float(now - start));
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

	init_sdl_window();
	print_time("init sdl window");

	Profiler::init();

	Cmd_Manager::inst->set_set_unknown_variables(true);
	if (!options.vars_section.empty())
		Cmd_Manager::inst->execute_file_section(Cmd_Execute_Mode::NOW, options.vars_file.c_str(),
											   options.vars_section.c_str());
	else
		Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, options.vars_file.c_str());
	print_time("execute vars file");
	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;
	for (int i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-x") == 0) {
			startx = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-y") == 0) {
			starty = atoi(argv[++i]);
		} else if (strcmp(argv[i], "-w") == 0) {
			g_window_w.set_integer(atoi(argv[++i]));
		} else if (strcmp(argv[i], "-h") == 0) {
			g_window_h.set_integer(atoi(argv[++i]));
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

	Model::on_model_loaded.add(this, [](Model* mod) { add_events_test(mod); });
	print_time("init mods,sounds");

	imgui_context = ImGui::CreateContext();
	init_debug_shape_ctx();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();
	print_time("imgui init");

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

	if (is_editor_app.get_bool()) {
		AssetBrowser::inst = new AssetBrowser();
		loaded_in_tool_mode = true;
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
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
			ASSERT(!is_editor_app.get_bool());
		}
	}

	Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, options.init_file.c_str());
	print_time("execute init");
	Cmd_Manager::inst->set_set_unknown_variables(false);

	// doesnt matter if it fails, just precache loading stuff
	// g_assets.find_sync<AssetBundle>("eng/default_bundle.bundle", true);
	print_time("load default bundle");

	sys_print(Info, "execute startup in %f\n", (float)TimeSinceStart());
}

Application* Application::get_app() {
	return eng->get_app();
}
