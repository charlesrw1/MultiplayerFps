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
#include "Types.h"
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
#include "IntegrationTest.h"
#include "EngineEditorState.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Assets/AssetBundle.h"
#include "Framework/StringUtils.h"
#include "Scripting/ScriptManager.h"

#include "EditorPopupTemplate.h"
#include "Animation/SkeletonData.h"
#include <mutex>

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
ConfigVar g_editor_cfg_folder("g_editor_cfg_folder", "Cfg", CVAR_DEV, "what folder to save .ini and other editor cfg to");
ConfigVar loglevel("loglevel", "4", CVAR_INTEGER, "(0=disable,4=all)", 0, 4);
ConfigVar colorLog("colorLog", "1", CVAR_BOOL, "");
ConfigVar g_application_class("g_application_class", "Application", CVAR_DEV, "");
ConfigVar with_threading("with_threading", "1", CVAR_BOOL | CVAR_DEV, "");
ConfigVar is_editor_app("is_editor_app", "0", CVAR_BOOL, "");
ConfigVar g_drawconsole("drawconsole", "0", CVAR_BOOL, "draw the console");
ConfigVar g_project_name("g_project_name", "CsRemakeEngine", CVAR_DEV, "the project name of the game, used for save file folders");
ConfigVar g_mousesens("g_mousesens", "0.005", CVAR_FLOAT, "", 0.0, 1.0);
ConfigVar g_fov("fov", "70.0", CVAR_FLOAT, "", 10.0, 110.0);
ConfigVar g_thirdperson("thirdperson", "70.0", CVAR_BOOL, "");
ConfigVar g_fakemovedebug("fakemovedebug", "0", CVAR_INTEGER, "", 0, 2);
ConfigVar g_drawimguidemo("g_drawimguidemo", "0", CVAR_BOOL, "");
ConfigVar g_debug_skeletons("g_debug_skeletons", "0", CVAR_BOOL, "draw skeletons of active animated renderables");
ConfigVar g_draw_grid("g_draw_grid", "0", CVAR_BOOL, "draw a debug grid around the origin");
ConfigVar g_grid_size("g_grid_size", "1", CVAR_FLOAT, "size of g_draw_grid", 0.01, 10);
// defualt sky material to use for editors like materials/models/etc.
ConfigVar ed_default_sky_material("ed_default_sky_material", "eng/hdriSky.mm", CVAR_DEV, "default sky material used for editors");
ConfigVar g_drawdebugmenu("g_drawdebugmenu", "0", CVAR_BOOL, "draw the debug menu");
ConfigVar g_window_w("vid.width", "1200", CVAR_INTEGER, "", 1, 4000);
ConfigVar g_window_h("vid.height", "800", CVAR_INTEGER, "", 1, 4000);
ConfigVar g_window_fullscreen("vid.fullscreen", "0", CVAR_BOOL, "");
ConfigVar g_host_port("net.hostport", "47000", CVAR_INTEGER | CVAR_READONLY, "", 0, UINT16_MAX);
ConfigVar g_dontsimphysics("stop_physics", "0", CVAR_BOOL | CVAR_DEV, "");
ConfigVar developer_mode("developer_mode", "1", CVAR_DEV | CVAR_BOOL, "enables dev mode features like compiling assets when loading");
ConfigVar g_slomo("slomo", "1.0", CVAR_FLOAT | CVAR_DEV, "multiplier of dt in update loop", 0.0001, 5.0);


double GetTime()
{
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart()
{
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


struct Debug_Shape
{
	enum type {
		sphere,
		line,
		box,
		transformed_box
	}type;
	glm::vec3 pos;
	glm::vec3 size;
	glm::mat4 transform;
	Color32 color;
	float lifetime = 0.0;
};
class DebugShapeCtx
{
public:
	static DebugShapeCtx& get() {
		static DebugShapeCtx inst;
		return inst;
	}

	void init() {
		MeshBuilder_Object obj;
		obj.visible = false;
		obj.depth_tested = false;
		obj.use_background_color = true;
		this->handle = idraw->get_scene()->register_meshbuilder();
		idraw->get_scene()->update_meshbuilder(handle, obj);
	}
	void update(float dt);
	void add(Debug_Shape shape, bool fixedupdate) {
		if (shape.lifetime <= 0.f && fixedupdate)
			one_frame_fixedupdate.push_back(shape);
		else
			shapes.push_back(shape);
	}
	void fixed_update_start();
private:
	std::vector<Debug_Shape> shapes;
	std::vector<Debug_Shape> one_frame_fixedupdate;
	handle<MeshBuilder_Object> handle;
	MeshBuilder mb;
};
//


static std::mutex printMutex;	// fixme

char* string_format(const char* fmt, ...) {
	std::lock_guard<std::mutex> printLock(printMutex);	// fixme

	va_list argptr;
	static int index = 0;
	static char string[4][512];
	char* buf;

	buf = string[index];
	index = (index + 1) & 3;

	va_start(argptr, fmt);
	vsprintf(buf, fmt, argptr);
	va_end(argptr);

	return buf;
}

void Quit()
{
	sys_print(Info, "Quiting... (runtime %f)\n", TimeSinceStart());
	eng_local.cleanup();
	exit(0);
}
Color32 get_color_of_print(LogType type) {

	const Color32 err = { 255, 105, 105 };
	const Color32 warn = { 252, 224, 121 };
	const Color32 info = COLOR_WHITE;
	const Color32 debug = { 136, 161, 252 };
	const Color32 consolePrint = { 136,23,152 };
	Color32 out = info;
	if (type == LogType::Error) out = err;
	else if (type == LogType::Warning)out = warn;
	else if (type == LogType::Debug) out = debug;
	else if (type == LogType::LtConsoleCommand) out = consolePrint;
	return out;
}

void sys_print(LogType type, const char* fmt, ...)
{
	if ((int(type)) > loglevel.get_integer())
		return;

	va_list args;
	va_start(args, fmt);
	
	bool print_end = false;
	if (colorLog.get_bool()) {
		print_end = true;
		if (type == LogType::Error) {
			printf("\033[91m");
		}
		else if (type == LogType::Warning) {
			printf("\033[33m");
		}
		else if (type == LogType::Debug) {
			printf("\033[32m");
		}
		else if (type == LogType::LtConsoleCommand) {
			printf("\033[35m");
			char buf[1024];
			vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
			buf[IM_ARRAYSIZE(buf) - 1] = 0;
			eng->log_to_fullscreen_gui(LtConsoleCommand, buf);
		}
		else
			print_end = false;
	}


	vprintf(fmt, args);
	if(Debug_Console::inst)
		Debug_Console::inst->print_args(get_color_of_print(type),fmt, args);
	va_end(args);

	if (print_end)
		printf("\033[0m");
}


void sys_vprint(const char* fmt, va_list args)
{
	std::lock_guard<std::mutex> printLock(printMutex);

	vprintf(fmt, args);
	Debug_Console::inst->print_args(get_color_of_print(Info),fmt, args);
}

void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	vprintf(format, list);
	va_end(list);
	fflush(stdout);
	exit(-1);
}

int imgui_std_string_resize(ImGuiInputTextCallbackData* data)
{
	std::string* user = (std::string*)data->UserData;
	assert(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		user->resize(data->BufSize);
		data->Buf = (char*)user->data();
	}

	return 0;
}
bool std_string_input_text(const char* label, std::string& str, int flags)
{
	return ImGui::InputText(label, (char*)str.c_str(), str.size() + 1, flags | ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &str);
}



void GameEngineLocal::log_to_fullscreen_gui(LogType type, const char* msg)
{
	gui_log.add_text(get_color_of_print(type), msg);
}

static void SDLError(const char* msg)
{
	printf(" %s: %s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}




glm::mat4 User_Camera::get_view_matrix() const {
	return glm::lookAt(position, position + front, up);
}
bool User_Camera::can_take_input() const {
	return orbit_mode || UiSystem::inst->is_game_capturing_mouse() || Input::last_recieved_input_from_con();
}


void User_Camera::set_orbit_target(glm::vec3 target, float object_size)
{
	orbit_target = target;
	position = orbit_target - front * object_size * 4.f;
}



void User_Camera::update_from_input(int width, int height, float aratio, float fov)
{
	//int xpos, ypos;
	//xpos = mouse_dx;
	//ypos = mouse_dy;

	auto deadzone = [](float in) -> float {
		const float dead_zone_val = 0.1;
		return glm::abs(in) > dead_zone_val ? in : 0.f;
	};

	auto mousedelta = Input::get_mouse_delta();

	const float controllerSens = 5.f;
	mousedelta.x += deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX)) * controllerSens;
	mousedelta.y += deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY)) * controllerSens;


	int xpos = mousedelta.x;
	int ypos = mousedelta.y;

	float x_off = xpos;
	float y_off = ypos;

	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;
	
	auto update_pitch_yaw = [&]() {
		yaw += x_off;
		pitch -= y_off;

		if (pitch > HALFPI - 0.01)
			pitch = HALFPI - 0.01;
		if (pitch < -HALFPI + 0.01)
			pitch = -HALFPI + 0.01;

		if (yaw > TWOPI) {
			yaw -= TWOPI;
		}
		if (yaw < 0) {
			yaw += TWOPI;
		}
	};

	if (orbit_mode)
	{

		//auto keystate = SDL_GetKeyboardState(nullptr);
		bool pan_in_orbit_model = Input::is_shift_down();// keystate[SDL_SCANCODE_LSHIFT];

		if (!pan_in_orbit_model) {
			update_pitch_yaw();
		}

		front = AnglesToVector(pitch, yaw);
		glm::vec3 right = normalize(cross(up, front));
		glm::vec3 real_up = glm::cross(right, front);
		distance = glm::length(orbit_target - position);

		// panning
		float x_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX))* distance *0.2;
		float y_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY)) * distance *0.2;

		if (pan_in_orbit_model&&!Input::last_recieved_input_from_con()) {	
			// scale by dist, not accurate, fixme	
			float x_s = tan(fov / 2) * distance * 0.5;
			float y_s = x_s * aratio;
			x_orb += x_s * x_off;
			y_orb += y_s * y_off;
		}
		orbit_target = orbit_target - real_up *  y_orb + right *  x_orb;



		position = orbit_target - front * distance;
	}
	else
	{
		update_pitch_yaw();

		front = AnglesToVector(pitch, yaw);
		glm::vec3 delta = glm::vec3(0.f);
		vec3 right = normalize(cross(up, front));
		if (Input::is_key_down(SDL_SCANCODE_W))
			delta += move_speed * front;
		if (Input::is_key_down(SDL_SCANCODE_S))
			delta -= move_speed * front;
		if (Input::is_key_down(SDL_SCANCODE_A))
			delta += right * move_speed;
		if (Input::is_key_down(SDL_SCANCODE_D))
			delta -= right * move_speed;
		if (Input::is_key_down(SDL_SCANCODE_Z) || Input::is_con_button_down(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
			delta += move_speed * up;
		if (Input::is_key_down(SDL_SCANCODE_X) || Input::is_con_button_down(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
			delta -= move_speed * up;

		delta -= move_speed * front * deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY));
		delta -= move_speed * right * deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX));

		position += delta;
	}

	{
		int scroll_amt = Input::get_mouse_scroll();

		if (orbit_mode) {
			float lookatpointdist = dot(position - orbit_target, front);
			glm::vec3 lookatpoint = position + front * lookatpointdist;
			lookatpointdist += (lookatpointdist * 0.25) * scroll_amt;
			if (abs(lookatpointdist) < 0.01)
				lookatpointdist = 0.01;
			position = (lookatpoint - front * lookatpointdist);
		}
		else {
			move_speed += (move_speed * 0.5) * scroll_amt;
			if (abs(move_speed) < 0.000001)
				move_speed = 0.0001;
		}
	}
}







vector<string>* GameEngineLocal::find_keybinds(SDL_Scancode code, uint16_t keymod) {

	auto mod_to_integer = [](uint16_t mod) -> uint16_t {
		if (mod & KMOD_CTRL)
			mod |= KMOD_CTRL;
		if (mod & KMOD_SHIFT)
			mod |= KMOD_SHIFT;
		if (mod & KMOD_ALT)
			mod |= KMOD_ALT;
		mod &= KMOD_CTRL | KMOD_SHIFT | KMOD_ALT;
		return mod;
	};


	uint32_t both = uint32_t(code) | ((uint32_t)mod_to_integer(keymod) << 16);

	auto find = keybinds.find(both);
	if (find == keybinds.end()) return nullptr;
	return &find->second;
}

void GameEngineLocal::set_keybind(SDL_Scancode code, uint16_t keymod, std::string bind) {
	auto mod_to_integer = [](uint16_t mod) -> uint16_t {
		if (mod & KMOD_CTRL)
			mod |= KMOD_CTRL;
		if (mod & KMOD_SHIFT)
			mod |= KMOD_SHIFT;
		if (mod & KMOD_ALT)
			mod |= KMOD_ALT;
		mod &= KMOD_CTRL | KMOD_SHIFT | KMOD_ALT;
		return mod;
	};
	uint32_t both = uint32_t(code) | ((uint32_t)mod_to_integer(keymod) << 16);
	keybinds[both].push_back(bind);
}

void bind_key(const Cmd_Args& args)
{
	if (args.size() < 2) return;
	SDL_Scancode scancode = SDL_GetScancodeFromName(args.at(1));
	if (scancode == SDL_SCANCODE_UNKNOWN) 
		return;
	if (args.size() <= 2)
		eng_local.set_keybind(scancode,0, "");
	else if (args.size() <= 3)
		eng_local.set_keybind(scancode,0, args.at(2));
	else {
		
		uint16_t modifiers = 0;
		for (int i = 2; i < args.size() - 1; i++) {
			const char* m = args.at(i);
			if (strcmp(m, "Ctrl")==0)
				modifiers |= KMOD_CTRL;
			else if (strcmp(m, "Alt")==0)
				modifiers |= KMOD_ALT;
			else if (strcmp(m, "Shift")==0)
				modifiers |= KMOD_SHIFT;
			else
				sys_print(Warning, "unknown modifier for 'bind': %s\n", m);
		}

		// bind M Ctrl Alt "mycommand"
		
		eng_local.set_keybind(scancode, modifiers, args.at(args.size() - 1));
	}
}


extern void init_log_gui();
using std::make_unique;
#include "LevelSerialization/SerializeNew.h"
bool GameEngineLocal::load_level(string mapname)
{
	if (level && level->get_is_in_update()) {
		sys_print(Warning, "GameEngineLocal::load_level: level is in update period, can't change level here.\n");
		return false;
	}
	const bool wants_empty = mapname == "<empty>";

	double start_time = GetTime();

	bool success = true;
	uptr<UnserializedSceneFile> file;
	if (!wants_empty) {
		auto val = load_level_asset(mapname);
		if (val)
			file = std::move(val);
		else
			success = false;
	}
	else {
		file = std::make_unique<UnserializedSceneFile>();
	}


	auto insert_this_map_as_level = [&](UnserializedSceneFile* loadedLevel, bool is_for_playing) {
		sys_print(Info, "Changing map: %s (for_playing=%s)\n", mapname.c_str(), print_get_bool_string(is_for_playing));

#ifdef EDITOR_BUILD
		if (editorState && editorState->has_tool()) {
			editorState->hide();
			assert(!editorState->get_tool());
		}
#endif
		if (level) {
			stop_game();
			assert(!level);
		}
	

		g_modelMgr.compact_memory();	// fixme, compacting memory here means newly loaded objs get moved twice, should be queuing uploads
		time = 0.0;
		set_tick_rate(60.f);
		level = make_unique<Level>(!is_for_playing);
		level->start(mapname,loadedLevel);	// scene will then get destroyed
		idraw->on_level_start();

		if (app) {
			app->on_map_changed();
		}
		sys_print(Info, "changed state to Engine_State::Game\n");
	};

	if (success || wants_empty) {
		insert_this_map_as_level(file.get(), !is_editor_state());
	}
	else {
		sys_print(Warning, "OpenMapCommand::execute(%s): failed to load\n", mapname.c_str());
		return false;
	}
	
	double now = GetTime();
	sys_print(Debug, "OpenMapCommand::execute: took %f\n", float(now - start_time));

	return true;
}




static void inc_or_dec_int_var(ConfigVar* var, bool decrement)
{
	int cur = var->get_integer();
	int max = std::round(var->get_max_val());
	int min = std::round(var->get_min_val());
	int span = max - min;
	int cur_start_min = cur - min;
	int next_start_min = (cur_start_min + 1) % span;
	if (decrement) {
		next_start_min = cur_start_min - 1;
		if (next_start_min < 0)
			next_start_min = span - 1;
	}
	int next = next_start_min + min;
	var->set_integer(next);
	sys_print(Info, "%s = %s\n", var->get_name(), var->get_string());
}

void dump_imgui_ini(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Info, "usage: dump_imgui_ini  ($g_editor_cfg_folder)/<file>");
		return;
	}

	std::string relative = g_editor_cfg_folder.get_string();
	relative += "/";
	relative += args.at(1);

	auto path = FileSys::get_full_path_from_relative(relative, FileSys::ENGINE_DIR);	// might change this to user dir

	ImGui::SaveIniSettingsToDisk(path.c_str());
}
void load_imgui_ini(const Cmd_Args& args)
{
	if (args.size() != 2) {
		sys_print(Info, "usage: load_imgui_ini ($g_editor_cfg_folder)/<file>");
		return;
	}

	std::string relative = g_editor_cfg_folder.get_string();
	relative += "/";
	relative += args.at(1);

	auto path = FileSys::get_full_path_from_relative(relative, FileSys::ENGINE_DIR);

	ImGui::LoadIniSettingsFromDisk(path.c_str());
}

// tech debt nonsense
extern void IMPORT_TEX_FOLDER(const Cmd_Args& args);
extern void IMPORT_TEX(const Cmd_Args& args);
extern void COMPILE_TEX(const Cmd_Args& args);

void GameEngineLocal::add_commands()
{
	commands = ConsoleCmdGroup::create("");
	commands->add("print_assets", [](const Cmd_Args&) { g_assets.print_usage(); });
#ifdef EDITOR_BUILD
	commands->add("import-tex-folder", IMPORT_TEX_FOLDER);
	commands->add("import-tex", IMPORT_TEX);
	commands->add("compile-tex", COMPILE_TEX);
	
#endif
	
	commands->add("stress-test", [](const Cmd_Args&) {
		int size = 20;
		for (int x = 0; x < size; x++) {
			for (int y = 0; y < size; y++) {
				for (int z = 0; z < size; z++) {
					Entity* e = eng->get_level()->spawn_entity();
					auto m = e->create_component<MeshComponent>();
					e->dont_serialize_or_edit = true;
					m->dont_serialize_or_edit = true;
					m->set_ignore_baking(true);
					m->set_model(Model::load("sphere_lods.cmdl"));
					e->set_ws_position(glm::vec3(x, y, z)*3.f);
				}
			}
		}

		});
	commands->add("save_baked_gi", [](const Cmd_Args&) {
		GameSceneGiUtil::save_to_disk();
		});
	commands->add("bake_probes", [](const Cmd_Args&) {GameSceneGiUtil::bake_all_cubemaps(); });
	//commands->add("close_ed", close_editor);
	commands->add("load_imgui_ini", load_imgui_ini);
	commands->add("dump_imgui_ini", dump_imgui_ini);
	commands->add("reload_shaders", [](const Cmd_Args&) { idraw->reload_shaders(); } );
	commands->add("dec", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: dec <int cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_INTEGER))
		{
			sys_print(Warning, "usage: dec <int cvar>");
			return;
		}
		inc_or_dec_int_var(var, true);
	});
	commands->add("inc", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: inc <int cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_INTEGER))
		{
			sys_print(Warning, "usage: inc <int cvar>");
			return;
		}
		inc_or_dec_int_var(var, false);
		});
	commands->add("toggle", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Warning, "usage: toggle <boolean cvar>");
			return;
		}
		auto var = VarMan::get()->find(args.at(1));
		if (!var || !(var->get_var_flags() & CVAR_BOOL))
		{
			sys_print(Warning, "usage: toggle <boolean cvar>");
			return;
		}
		var->set_bool(!var->get_bool());
		sys_print(Info, "%s = %s\n", var->get_name(), var->get_string());
		});
	commands->add("exec", [](const Cmd_Args& args) {
		if (args.size() < 2) {
			sys_print(Info, "usage: exec <exec filename>");
			return;
		}
		Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, args.at(1));
		});
	commands->add("quit", [](const Cmd_Args& args) { Quit(); });

	commands->add("create-map", [](const Cmd_Args& args) {
		auto existing = FileSys::open_read_game(args.at(1));
		if (!existing) {
			auto file = FileSys::open_write_game(args.at(1));
			string s = "!json\n{\"objs\":[]}";
			file->write(s.c_str(),s.size());
		}
		else {
			sys_print(Error, "cant make new map, already exists\n");
		}
		});
	commands->add("open-editor", [&](const Cmd_Args& args) {
		sys_print(Debug, "OpenEditorToolCommand::execute\n");

		if (!eng_local.editorState) {
			sys_print(Error, "OpenEditorToolCommand: didnt launch in editor mode, use 'is_editor_app 1' in cfg or command line\n");
			return;
		}
		string mapname = "<empty>";
		if (args.size() == 2)
			mapname = args.at(1);

		editorState->open_tool(mapname);

		});

	commands->add("bind", bind_key);
	
	commands->add("dump_bundle", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "usage: bundle name\n");
		}
		else {
			g_assets.dump_loaded_assets_to_disk(args.at(1));
		}
		});
	commands->add("reload_script",[](const Cmd_Args& args) {
		ScriptManager::inst->reload_all_scripts();
		});

	g_modelMgr.add_commands(*commands);
}

void test_integration_1(IntegrationTester& tester)
{
}

#include "Testheader.h"

void GameEngineLocal::set_tester(IntegrationTester* tester, bool headless_mode) {
#ifdef EDITOR_BUILD
	this->tester.reset(tester);
	this->headless_mode = headless_mode;
#endif
}


#include "Render/Frustum.h"
static void cullobjstest(Frustum& frustum, bool* visible_array, int visible_array_size,glm::vec4* bounding_spheres)
{
	for (int i = 0; i < visible_array_size; i++)
	{
		glm::vec4& sph = bounding_spheres[i];
		const glm::vec3& center = glm::vec3(sph);
		const float& radius = sph.w;

		int res = 0;
		res += (glm::dot(glm::vec3(frustum.top_plane), center) + frustum.top_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.bot_plane), center) + frustum.bot_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.left_plane), center) + frustum.left_plane.w >= -radius) ? 1 : 0;
		res += (glm::dot(glm::vec3(frustum.right_plane), center) + frustum.right_plane.w >= -radius) ? 1 : 0;

		visible_array[i] = res == 4;
	}
}
extern void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin);
#include "Framework/FreeList.h"
extern void texture_loading_benchmark();


// tests: iterate in a hot loop
// remove/add objects randomly
// access via ids

struct FattyRo : public Render_Object {
	glm::vec4 v{};
	glm::mat4 matrix{};
	glm::mat4 bruh{};
};
struct SmallRo {
	glm::mat4 transform{};
	int index = 0;
};

template<typename T>
class FreeList2 {
public:
	void insert(T* ptr) {
		int index = ptrs.size() - 1;
		ptr->index = index;
		ptrs.push_back(ptr);
	}
	void remove(T* ptr) {
		int index = ptr->index;
		ptrs.back()->index = index;
		ptrs.at(index) = ptrs.back();
		ptrs.pop_back();
	}
	std::vector<T*> ptrs;
};

int benchmark_free_list() {
	using Type = SmallRo;
	Random random(42341);
	const int NUM_OBJECTS = 10'000;
	const int NUM_REMOVE = 1000;
	double totalt = 0;
	double start = 0;
	double timestamps[3];
	auto add_time = [&](int idx) {
		double now = GetTime();
		timestamps[idx] = now - start;
		start = now;
	};
	auto print_times = [&](const char* str) {
		printf("times %s\n", str);
		printf("	* %f\n", float(timestamps[0] * 1000));
		printf("	* %f\n", float(timestamps[1] * 1000));
		printf("	* %f\n", float(timestamps[2] * 1000));
	};
	{

		// normal free list
		Free_List<Type> render_objects;
		for (int i = 0; i < NUM_OBJECTS; i++) {
			int id = render_objects.make_new();
			render_objects.get(id).transform[3] = glm::vec4(0, random.RandF(-1, 1), 0, 0);
		}

		start = GetTime();
		// remove/add randomly
		for (int i = 0; i < NUM_REMOVE; i++) {
			render_objects.free(100 + i);
		}
		add_time(0);

		// access via ids
		for (int i = 100 + NUM_REMOVE; i < NUM_OBJECTS; i += 3) {
			render_objects.get(i).transform[3].y += 0.1;
		}
		add_time(1);

		// iterate hot loop
		volatile float x = 0;
		for (int i = 0; i < render_objects.objects.size(); i++) {
			x += render_objects.objects[i].type_.transform[3].y;
		}
		add_time(2);
		print_times("struct");
		totalt += x;
	}

	{
		std::vector<Type*> ptrs;
		// normal free list
		FreeList2<Type> render_objects;
		for (int i = 0; i < NUM_OBJECTS; i++) {
			Type* o = new Type;
			o->transform[3] = glm::vec4(0, random.RandF(), 0, 0);
			render_objects.insert(o);
			ptrs.push_back(o);
		}

		start = GetTime();
		// remove/add randomly
		for (int i = 0; i < NUM_REMOVE; i++) {
			render_objects.remove(ptrs.at(100 + i));
		}
		add_time(0);

		// access via ids
		for (int i = 100 + NUM_REMOVE; i < NUM_OBJECTS; i += 3) {
			ptrs[i]->transform[3].y += 0.1;
		}
		add_time(1);

		// iterate hot loop
		volatile float x = 0;
		for (int i = 0; i < render_objects.ptrs.size(); i++) {
			x += render_objects.ptrs[i]->transform[3].y;
		}
		add_time(2);
		print_times("struct2");
		totalt += x;
	}

	{
		// pointer free list
		Free_List<Type*> render_objects;
		for (int i = 0; i < NUM_OBJECTS; i++) {
			Type* o = new Type;
			int id = render_objects.make_new();
			o->transform[3] = glm::vec4(0, random.RandF(), 0, 0);
			render_objects.get(id) = o;
		}
		start = GetTime();
		// remove/add randomly
		for (int i = 0; i < NUM_REMOVE; i++) {
			render_objects.free(100 + i);
		}
		add_time(0);
		// access via ids
		for (int i = 100 + NUM_REMOVE; i < NUM_OBJECTS; i += 3) {
			render_objects.get(i)->transform[3].y += 0.1;
		}
		add_time(1);
		// iterate hot loop
		volatile float x = 0;
		for (int i = 0; i < render_objects.objects.size(); i++) {
			x += render_objects.objects[i].type_->transform[3].y;
		}
		add_time(2);
		print_times("ptr");
		totalt += x;
	}

	{
		// pointer free list
		hash_map<Type*> render_objects;
		int64_t handle = 1;
		for (int i = 0; i < NUM_OBJECTS; i++) {
			Type* o = new Type;
			render_objects.insert(handle++, o);
			o->transform[3] = glm::vec4(0, random.RandF(), 0, 0);
		}
		start = GetTime();
		// remove/add randomly
		for (int i = 0; i < NUM_REMOVE; i++) {
			render_objects.remove(1 + i);
		}
		add_time(0);

		add_time(1);
		// iterate hot loop
		volatile float x = 0;
		for (auto o : render_objects) {
			x += o->transform[3].y;

		}
		add_time(2);
		print_times("hashmap");
		totalt += x;
	}

	printf("totalx %f", float(totalt));
	return 0;



	Frustum f;
	build_a_frustum_for_perspective(f, View_Setup(glm::vec3(0), glm::vec3(1, 0, 0), glm::radians(60.f), 0.1, 100, 100, 100));
	bool* visarray = new bool[NUM_OBJECTS];
	glm::vec4* bounding_spheres = new glm::vec4[NUM_OBJECTS];
	for (int i = 0; i < NUM_OBJECTS; i++) {
		glm::vec4 v;
		v.w = 1.0;
		v.x = random.RandF(-100, 100);
		v.y = random.RandF(-100, 100);
		v.z = random.RandF(-100, 100);
		bounding_spheres[i] = v;
	}
	start = GetTime();
	cullobjstest(f, visarray, NUM_OBJECTS, bounding_spheres);
	double end = GetTime();
	printf("diff=%f\n", float(end - start) * 1000.f);
	return 0;

}

int game_engine_main(int argc, char** argv)
{
	material_print_debug.set_bool(true);
	//developer_mode.set_bool(false);
	log_shader_compiles.set_bool(false);
	log_all_asset_loads.set_bool(true);
	eng_local.init(argc,argv);
	//developer_mode.set_bool(true);
	//log_all_asset_loads.set_bool(false);
	log_destroy_game_objects.set_bool(false);
	//loglevel.set_integer(1);
#if 0
	vector<IntTestCase> tests;
	tests.push_back({ test_integration_1, "myTest" });
	tests.push_back({ test_integration_2, "myTest2" });
	tests.push_back({ test_state, "test_state" });
	tests.push_back({ test_opening_prefab_as_map, "test_opening_prefab_as_map" });
	tests.push_back({ test_loading_prefab_without_component, "test_loading_prefab_without_component" });
	tests.push_back({ test_loading_invalid_prefab, "test_loading_invalid_prefab" });
	tests.push_back({ test_remove_and_undo_pfb, "test_remove_and_undo_pfb" });
	tests.push_back({ test_editor_entity_ptr, "test_editor_entity_ptr" });

	//Cmd_Manager::inst->append_cmd(uptr<OpenMapCommand>(new OpenMapCommand("top_down/map0.tmap", true)));

	//eng_local.set_tester(new IntegrationTester(true, tests), false);
	auto c = ClassBase::create_class<InterfaceClass>("TestClassI");
	//
#endif


	//c->buzzer();
	//int val = c->get_value("hello");
	//assert(val == 1);
	//assert(c->myStr == "hello");

	//texture_loading_benchmark();

	eng_local.loop();
	eng_local.cleanup();
	
	return 0;
}
ClassWithDelegate StaticClass::myClass;

// console vs text-gui mode
// text-gui mode: swap to using normal 2d renderer? maybe.
// multiple text-gui windows? inspector

void GameEngineLocal::set_tick_rate(float tick_rate)
{
//	if (state == Engine_State::Game) {
	//	sys_print(Warning, "Can't change tick rate while running\n");
	//	return;
//	}
	tick_interval = 1.0 / tick_rate;
}


void GameEngineLocal::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;

		if (!UiSystem::inst->is_vp_focused()&&Debug_Console::inst->get_is_console_focused()) {
			UiSystem::inst->set_focus_to_viewport();
		}
		else {
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
			for(string& k : *keybinds)
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, k.c_str());
		}
	}
}

void GameEngineLocal::cleanup()
{
#ifdef EDITOR_BUILD
	//assert(0);
	//if (get_current_tool())
	//	get_current_tool()->close();
	if (editorState)
		editorState.reset();
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
		//NetworkQuit();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}
}


class Debug_Interface_Impl : public Debug_Interface
{
public:
	void add_hook(const char* menu_name, void(*drawfunc)()) {
		int i = 0;
		for (; i < hooks.size(); i++) {
			if (strcmp(menu_name, hooks[i].menu_name) == 0) {
				Hook_Node* node = new Hook_Node;
				node->drawfunc = drawfunc;
				node->next = nullptr;
				hooks[i].tail->next = node;
				hooks[i].tail = node;
				break;
			}
		}
		if (i == hooks.size()) {
			hooks.push_back(Menu_Hook());
			auto& hook = hooks.back();
			hook.menu_name = menu_name;
			Hook_Node* node = new Hook_Node;
			node->drawfunc = drawfunc;
			node->next = nullptr;
			hook.tail = hook.head = node;
		}
	}

	void draw() {
		ImVec2 winsize = ImGui::GetMainViewport()->Size;
		int width = 275;
		ImGui::SetNextWindowPos(ImVec2(winsize.x - width - 10, 50));
		ImGui::SetNextWindowSize(ImVec2(width, 700));
		ImGui::SetNextWindowBgAlpha(0.3);
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
		if (ImGui::Begin("Debug",nullptr, flags)) {
			ImGui::PushItemWidth(140.f);
			for (int i = 0; i < hooks.size(); i++) {
				if (ImGui::CollapsingHeader(hooks[i].menu_name)) {
					Hook_Node* p = hooks[i].head;
					while (p) {
						p->drawfunc();
						p = p->next;
					}
				}
			}


		}
		ImGui::End();
	}


	struct Hook_Node {
		Hook_Node* next = nullptr;
		void(*drawfunc)();
	};

	struct Menu_Hook {
		const char* menu_name = "";
		Hook_Node* head = nullptr;
		Hook_Node* tail = nullptr;
	};
	std::vector<Menu_Hook> hooks;
};





Debug_Interface* Debug_Interface::get()
{
	static Debug_Interface_Impl inst;
	return &inst;
}


bool GameEngineLocal::is_drawing_to_window_viewport() const
{
	return !UiSystem::inst->is_drawing_to_screen();
}


static bool scene_hovered = false;

#include "Framework/MyImguiLib.h"
void GameEngineLocal::draw_any_imgui_interfaces()
{
	
}

void GameEngineLocal::get_draw_params(SceneDrawParamsEx& params, View_Setup& setup)
{
	params = SceneDrawParamsEx(time, frame_time);
	params.output_to_screen = UiSystem::inst->is_drawing_to_screen();
	// so the width/height parameters are valid
	View_Setup vs_for_gui;
	auto viewport = UiSystem::inst->get_vp_rect().get_size();// get_game_viewport_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;


#ifdef EDITOR_BUILD
	// draw general ui
	if (editorState&&editorState->has_tool()) {
		params.is_editor = true;	// draw to the id buffer for mouse picking
		auto vs = editorState->get_vs();

		// fixme

		if (!vs) {
			params.draw_world = false;
			vs = &vs_for_gui;
		}
		isound->set_listener_position(vs->origin, glm::normalize(glm::cross(vs->front, glm::vec3(0, 1, 0))));
		setup = *vs;
		//idraw->scene_draw(params, *vs, get_gui());
	}
	else
#endif
	{
		params=SceneDrawParamsEx(time, frame_time);
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
		}
		else {


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

glm::mat4 View_Setup::make_opengl_perspective_with_near_far() const
{
	return glm::perspectiveRH_NO(fov, width / (float)height, near, far);
}

View_Setup::View_Setup(glm::mat4 viewMat, float fov, float near, float far, int width, int height)
	: view(viewMat), fov(fov), near(near), far(far), width(width), height(height)
{
	auto inv = glm::inverse(viewMat);
	this->origin = inv[3];
	this->front = -inv[2];
	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);
	viewproj = proj * view;
}

View_Setup::View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height)
	: origin(origin),front(front),fov(fov),near(near),far(far),width(width),height(height)
{
	view = glm::lookAt(origin, origin + front, glm::vec3(0, 1.f, 0));
	//proj = glm::perspective(fov, width / (float)height, near, far);

	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);

	viewproj = proj * view;
}

#define TIMESTAMP(x) sys_print(Debug, "%s in %f\n",x,(float)GetTime()-start); start = GetTime();

GameEngineLocal::GameEngineLocal()
{

}


void GameEngineLocal::init_sdl_window()
{
	ASSERT(!window);

	sys_print(Info, "initializing window...\n");

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		sys_print(Error,"init sdl failed: %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	const char* title = g_project_name.get_string();
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		g_window_w.get_integer(), g_window_h.get_integer(), SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
	TracyGpuContext;
	TracyGpuContextName("a", 1);	//??

	SDL_GL_SetSwapInterval(0);
}


using std::make_unique;

ImFont* global_big_imgui_font = nullptr;


void GameEngineLocal::init(int argc, char** argv)
{
	this->argc = argc;
	this->argv = argv;

	sys_print(Info, "--------- Initializing Engine ---------\n");

	program_time_start = GetTime();
	double start = GetTime();
	auto print_time = [&](const char* msg) {
		double now = GetTime();
		//printf("-----TIME %s %f\n", msg, float(now - start));
		printf("init % s in % fs\n", msg, float(now-start));
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
	FileSys::init();
	print_time("file init");


	Cmd_Manager::inst->set_set_unknown_variables(true);
	Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, "vars.txt");
	print_time("execute vars file");

	g_assets.init();
	print_time("asset init");

	JobSystem::inst = new JobSystem();// spawns worker threads
	print_time("job sys init");

	ScriptManager::inst = new ScriptManager();
	ScriptManager::inst->load_script_files();
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
	DebugShapeCtx::get().init();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();
	print_time("imgui init");

	auto path = FileSys::get_full_path_from_game_path("eng/inconsolata_bold.ttf");
	ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), 14.0);
	global_big_imgui_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), 24.0);
	ImGui::GetIO().Fonts->Build();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	print_time("imgui font");

	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-x") == 0) {
			startx = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-y") == 0) {
			starty = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			g_window_w.set_integer(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-h") == 0) {
			g_window_h.set_integer(atoi(argv[++i]));
		}
		else if (strcmp(argv[i], "-VISUALSTUDIO") == 0) {
			const char* projName = g_project_name.get_string();
			SDL_SetWindowTitle(window, string_format("%s - VISUAL STUDIO\n", projName));
		}

		else if (argv[i][0] == '-') {
			string cmd;
			cmd += &argv[i++][1];
			while (i < argc && argv[i][0] != '-') {
				cmd += ' ';
				cmd += argv[i++];
			}

			Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, cmd.c_str());
		}
	}
	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());


	auto start_game = [&]() {
		sys_print(Info, "starting game...\n");
		Application* a = ClassBase::create_class<Application>(g_application_class.get_string());
		if (!a) {
			Fatalf("Engine::init: application class not found %s. set it in vars.txt\n", g_application_class.get_string());
		}
		app.reset(a);
		Input::on_con_status.add(a, [this](int i, bool b) {
			app->on_controller_status(i, b);
			});
		Model::on_model_loaded.add(this, [this](Model* m) {
			app->on_post_model_load(m);
			});
		MaterialInstance::on_material_loaded.add(this, [this](MaterialInstance* m) {
			try {
				app->on_post_material_load(m);
			}
			catch (LuaRuntimeError er) {
				sys_print(Error, "on_post_material_load %s\n",er.what());
			}
			});


		app->start();
	};
#ifdef EDITOR_BUILD
	AssetRegistrySystem::get().init();
	
	if (is_editor_app.get_bool()) {
		AssetBrowser::inst = new AssetBrowser();
		editorState = make_unique<EditorState>();
		ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		print_time("asset reg and browser init");
	}
	else {
		start_game();
	}
#else
	start_game();
#endif

	Cmd_Manager::inst->execute_file(Cmd_Execute_Mode::NOW, "init.txt");
	print_time("execute init");
	Cmd_Manager::inst->set_set_unknown_variables(false);

	// doesnt matter if it fails, just precache loading stuff
	//g_assets.find_sync<AssetBundle>("eng/default_bundle.bundle", true);
	print_time("load default bundle");

	sys_print(Info, "execute startup in %f\n", (float)TimeSinceStart());
}

void GameEngineLocal::game_update_tick()
{
	ZoneScopedN("game_update_tick");

	auto fixed_update = [&](double dt) {
		ZoneScopedN("fixed_update");

		DebugShapeCtx::get().fixed_update_start();
		g_physics.simulate_and_fetch(dt);
	};
	auto update = [&](double dt) {
		ZoneScopedN("update");
		CPUSCOPESTART(game_update_tick_update);
		if(level)
			level->update_level();
		if (app)
			app->update();
		GameAnimationMgr::inst->update_animating();
	};
	const double dt = frame_time;

	double secs_per_tick = tick_interval;
	frame_remainder += dt;
	int num_ticks = (int)floor(frame_remainder / secs_per_tick);
	frame_remainder -= num_ticks * secs_per_tick;

	float orig_ft = frame_time;
	float orig_ti = tick_interval;
	
	frame_time *= g_slomo.get_float();
	tick_interval *= g_slomo.get_float();

	// physics update

	if (app)
		app->pre_update();

	for (int i = 0; i < 1; i++) {
		fixed_update(tick_interval);
	}

	// call level update
	update(frame_time);

	time += frame_time;

	frame_time = orig_ft;
	tick_interval = orig_ti;
}


// unloads all game state
void GameEngineLocal::stop_game()
{
	if (!map_spawned())
		return;

	assert(level);
	string name = level->get_source_asset_name();
	sys_print(Info,"Clearing Map (%s)\n", name.c_str());
	on_leave_level.invoke();
	idraw->on_level_end();
	level->close_level();
	level.reset();



	// clear any debug shapes
	DebugShapeCtx::get().fixed_update_start();
}

bool GameEngineLocal::game_thread_update()
{

	return true;
}


enum class RpcId {
	ChangeMap,
};

class Networking {
public:
	// unreliable
	// netid 10bits, typeid 8bits, data, netid, typeid, data,...

	// rpcs (reliable+unreiable):
	// id,args,id,args

	const int net_id_bits = 10;
	const int max_net_ids = 1 << net_id_bits;
	const int net_type_id_bits = 7;
	const int max_net_types = 1 << net_type_id_bits;


	void tick();
	void get_state();

	// who connected to. peers? for server

	// snapshot management
	// id management

	// callbacks
	virtual void process_rpcs() {}
	virtual void process_snapshot() {}
};


struct GameUpdateOuput {
	bool drawOut = false;
	SceneDrawParamsEx paramsOut = SceneDrawParamsEx(0, 0);
	View_Setup vsOut;
};


struct NetObj {
	int net_id = 0;
	int net_type = 0;
};
class NetBuffer {
public:
};

class LLNetworkSystemCallbacks {
public:
	virtual void on_new_object(handle<NetObj> no) = 0;
	virtual void on_net_message(NetBuffer& buffer) = 0;
	virtual void on_conn_change() = 0;
};

class LlNetworkSystem {
public:
	handle<NetObj> register_net_obj();
	// connect to server
	//		with callback set
	// spawn server
	//		on client connect callback
	//			approve/deny
	//			send map info
	//				
	//			

};

// [Game]
// Game code
// Game net manager
// [Engine]
// LlNetworkSystem

// LlNetworkSystem not aware of Entities/Components. Just does replication, message sending, client/server connections.
// Game net manager is user provided (can provide a base implmentation).
// It maps the LlNetworkSystem to the gameplay. For example, "net_id" will likely map to a Component. "net_type" maps to a prefab type, for example.
// Game net manager also manages recieving net messages and determines if it has permissions.



void GameEngineLocal::loop()
{
	auto frame_start = [&]()
	{
		ZoneScopedN("frame_start");
		// update input
		Input::inst->pre_events();
		UiSystem::inst->pre_events();
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);
			Input::inst->handle_event(event);
			UiSystem::inst->handle_event(event);

			switch (event.type) {
			case SDL_QUIT:
				::Quit();
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
					int x, y;
					SDL_GetWindowSize(window, &x, &y);
					if (x % 2 == 1) x -= 1;
					if (y % 2 == 1) y -= 1;
					if (x != g_window_w.get_integer())
						g_window_w.set_integer(x);
					if (y != g_window_h.get_integer())
						g_window_h.set_integer(y);
				}
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
				key_event(event);
				break;
			default:
				break;
			}
			//if (!is_game_focused()) {
			//	if ((event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) && ImGui::GetIO().WantCaptureKeyboard)
			//		continue;
			//	if (!scene_hovered && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse)
			//		continue;
			//}
			//g_guiSystem->handle_event(event);
		}
		Input::inst->tick();
		UiSystem::inst->update();
		// Update the messsage queue! does level changing etc.
		Cmd_Manager::inst->execute_buffer();

		if (g_window_fullscreen.was_changed())
			Canvas::set_window_fullscreen(g_window_fullscreen.get_bool());
	};

	auto do_overlapped_update = [&](bool& shouldDrawNext, SceneDrawParamsEx& drawparamsNext, View_Setup& setupNext, const bool skip_rendering)
	{
		ZoneScopedN("OverlappedUpdate");
		CPUSCOPESTART(OverlappedUpdate);
		BooleanScope scope(b_is_in_overlapped_period);
		GameUpdateOuput out;

		// I reworked the asset system so have to disable this for now. issue is sync loading assets on game thread. otherwise everything else is threadsafe(tm).
		// 
		//JobCounter* gameupdatecounter{};
		//JobSystem::inst->add_job(game_update_job,uintptr_t(&out), gameupdatecounter);
		//game_update_job(uintptr_t(&out));


		{
			ZoneScopedN("GameThreadUpdate");
			CPUFUNCTIONSTART;

			//printf("abc\n");
			out.drawOut = true;
			try {
				game_update_tick();
#ifdef EDITOR_BUILD
				if (editorState)
					editorState->tick(frame_time);
#endif
			}
			catch (LuaRuntimeError luaErr) {
				sys_print(Error, "game_thread_update: caught LuaRuntimeError: %s\n", luaErr.what());
				string msg = string_format("LuaRuntimeError %s\n", luaErr.what());
				eng->log_to_fullscreen_gui(Error, msg.c_str());
			}

			isound->tick(frame_time);


			eng_local.get_draw_params(out.paramsOut, out.vsOut);
			//return;
			// update particles, doesnt draw, only builds meshes FIXME
			ParticleMgr::inst->draw(out.vsOut);
		}

		if (!skip_rendering) {
			if (!shouldDrawNext) {
				drawparamsNext.draw_world = drawparamsNext.draw_ui = false;
			}
			idraw->scene_draw(drawparamsNext, setupNext);
		}
		//JobSystem::inst->wait_and_free_counter(gameupdatecounter);// wait for game update to finish while render is on this thread

		shouldDrawNext = out.drawOut;
		drawparamsNext = out.paramsOut;
		setupNext = out.vsOut;
	};

	// This happens on main thread
	// I could double buffer draw data so ImGui can update on game thread and render simultaneously
	auto imgui_render = [&](const bool skip_rendering)
	{
		//ZoneScopedN("ImguiDraw");
		GPUSCOPESTART(imgui_update_scope);

		ZoneScopedN("ImGuiUpdate");

		gui_log.draw(UiSystem::inst->window);
		EditorState* s = nullptr;
#ifdef EDITOR_BUILD
		s = editorState.get();
#endif
		UiSystem::inst->draw_imgui_interfaces(s);

		ImGui::Render();
		if (!skip_rendering) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}
	};
	auto do_sync_update = [&]()
	{
		ZoneScopedN("SyncUpdate");
		DebugShapeCtx::get().update(frame_time);
#ifdef EDITOR_BUILD
		AssetRegistrySystem::get().update(); 		// update hot reloading
#endif
		ScriptManager::inst->update();

		if (get_level())
			get_level()->sync_level_render_data();
		UiSystem::inst->sync_to_renderer();
		g_physics.sync_render_data();		
		idraw->sync_update();
	};
	auto wait_for_swap = [&](const bool skip_rendering)
	{
		//ZoneScopedN("SwapWindow");
		GPUSCOPESTART(gl_swap_window_scope);
		if(!skip_rendering)
			SDL_GL_SwapWindow(window);
	};


	double last = GetTime() - 0.1;
	// these are from the last game frame
	SceneDrawParamsEx drawparamsNext(0, 0);
	View_Setup setupNext;
	bool shouldDrawNext = true;

	for (;;)
	{
		try {
			const bool skip_rendering = headless_mode;

			// update time
			const double now = GetTime();
			double dt = now - last;
			last = now;
			if (dt > 0.1)
				dt = 0.1;
			frame_time = dt;

#ifdef EDITOR_BUILD
			if (tester) {
				bool res = tester->tick(dt);
				if (res) {
					Quit();
				}
			}
#endif

			// update input, console cmd buffer, could change maps etc.
			frame_start();
			// overlapped update (game+render)
			do_overlapped_update(shouldDrawNext, drawparamsNext, setupNext, skip_rendering);

			// sync period
			imgui_render(skip_rendering);
			do_sync_update();
			wait_for_swap(skip_rendering);	// wait for swap last

			FrameMark;	// tracy profiling
			Profiler::end_frame_tick(frame_time);	// my crappy profilier
		}
		catch (LuaRuntimeError luaErr) {
			sys_print(Error, "loop: caught LuaRuntimeError: %s\n", luaErr.what());
			string msg = string_format("LuaRuntimeError %s\n", luaErr.what());
			eng->log_to_fullscreen_gui(Error, msg.c_str());
		}
	}
}

int debug_console_text_callback(ImGuiInputTextCallbackData* data)
{
	Debug_Console* console = (Debug_Console * )data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory) {
		if (data->EventKey == ImGuiKey_UpArrow) {
			if (console->history_index == -1) {
				console->history_index = console->history.size() -1 ;
			}
			else {
				console->history_index--;
				if (console->history_index < 0)
					console->history_index = 0;
			}
		}
		else if (data->EventKey == ImGuiKey_DownArrow) {
			if (console->history_index != -1) {
				console->history_index++;
				if (console->history_index >= console->history.size())
					console->history_index = console->history.size() - 1;
			}
		}
		console->scroll_to_bottom = true;
		if (console->history_index != -1) {
			auto& hist = console->history[console->history_index];
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, hist.c_str());
		}
	}
	else if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion) {
		string match = Cmd_Manager::get()->print_matches(console->input_buffer);
		console->scroll_to_bottom = true;
		if (!match.empty()) {
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, match.c_str());
		}
	}

	return 0;
}

void Debug_Console::draw()
{
	{
		std::lock_guard<std::mutex> printLock(printMutex);
		for (auto& l : bufferedLines)
			lines.push_back(std::move(l));
		bufferedLines.clear();
	}
	if (lines.size() > 1000)
		lines.clear();



	ImGui::SetNextWindowSize(ImVec2(520, 600), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("Console")) {
		ImGui::End();

		return;
	}
	
	ImGui::PushStyleColor(ImGuiCol_ChildBg, uint32_t(ImColor(5, 5, 5)));

	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false))
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)); // Tighten spacing
		for (int i = 0; i < lines.size(); i++)
		{
			Color32 color;
	
			auto& line = lines[i];
			auto& lineStr = line.line;
			auto lineColor = line.color;

			//if (!lines[i].empty() && lines[i][0] == '>') { 
			//	color = { 136,23,152 }; 
			//	has_color = true; 
			//}

			ImGui::PushStyleColor(ImGuiCol_Text, lineColor.to_uint());
			ImGui::TextUnformatted(lineStr.c_str());
			ImGui::PopStyleColor();
		}
		if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
			ImGui::SetScrollHereY(1.0f);
		scroll_to_bottom = false;

		ImGui::PopStyleVar();
	}
	ImGui::EndChild();

	ImGui::PopStyleColor();

	ImGui::Separator();

	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
		ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;

	if (ImGui::InputText("##Input", input_buffer, IM_ARRAYSIZE(input_buffer), input_text_flags, debug_console_text_callback, this))
	{
		char* s = input_buffer;
		if (s[0]) {
			// this will print it to the console
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, input_buffer);

			history.push_back(input_buffer);
			scroll_to_bottom = true;

			history_index = -1;
		}
		s[0] = 0;
		reclaim_focus = true;
	}
	is_console_focused = ImGui::IsItemFocused();

	ImGui::SetItemDefaultFocus();

	if (reclaim_focus||wants_toggle_set_focus) {
		ImGui::SetKeyboardFocusHere(-1);
		wants_toggle_set_focus = false;
	}
	ImGui::End();
}
void Debug_Console::print_args(Color32 color, const char* fmt, va_list args)
{
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	LineAndColor lc;
	lc.color = color;
	lc.line = buf;
	bufferedLines.push_back(lc);
}

void Debug_Console::print(Color32 color, const char* fmt, ...)
{
	std::lock_guard<std::mutex> printLock(printMutex);

	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	LineAndColor lc;
	lc.color = color;
	lc.line = buf;
	bufferedLines.push_back(lc);
}


void Debug::add_line(glm::vec3 f, glm::vec3 to, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::line;
	shape.pos = f;
	shape.size = to;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_box(glm::vec3 c, glm::vec3 size, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::box;
	shape.pos = c;
	shape.size = size;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_transformed_box(glm::mat4 c, glm::vec3 size, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::transformed_box;
	shape.transform = c;
	shape.size = size;
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}
void Debug::add_sphere(glm::vec3 c, float radius, Color32 color, float duration, bool fixedupdate)
{
	Debug_Shape shape;
	shape.type = Debug_Shape::sphere;
	shape.pos = c;
	shape.size = vec3(radius);
	shape.color = color;
	shape.lifetime = duration;
	DebugShapeCtx::get().add(shape, fixedupdate);
}

void DebugShapeCtx::update(float dt)
{

	auto& builder = mb;
	builder.Begin();

	vector<Debug_Shape>* shapearrays[2] = { &one_frame_fixedupdate,&shapes };
	for (int i = 0; i < 2; i++) {
		vector<Debug_Shape>& shapes = *shapearrays[i];
		for (int j = 0; j < shapes.size(); j++) {
			switch (shapes[j].type)
			{
			case Debug_Shape::line:
				builder.PushLine(shapes[j].pos, shapes[j].size, shapes[j].color);
				break;
			case Debug_Shape::box:
				builder.PushLineBox(shapes[j].pos - shapes[j].size * 0.5f, shapes[j].pos + shapes[j].size * 0.5f, shapes[j].color);
				break;
			case Debug_Shape::sphere:
				builder.AddSphere(shapes[j].pos, shapes[j].size.x, 8, 6, shapes[j].color);
				break;
			case Debug_Shape::transformed_box:
				builder.PushOrientedLineBox(glm::vec3(0.f), shapes[j].size, shapes[j].transform, shapes[j].color);
				break;
			}
		}
	}
	builder.End();
	MeshBuilder_Object mbo;
	mbo.transform = glm::mat4(1.f);
	mbo.owner = nullptr;
	mbo.meshbuilder = &mb;
	mbo.visible = true;
	mbo.depth_tested = false;
	mbo.use_background_color = true;

	idraw->get_scene()->update_meshbuilder(handle, mbo);


	for (int i = 0; i < shapes.size(); i++) {
		shapes[i].lifetime -= dt;
		if (shapes[i].lifetime <= 0.f) {
			shapes.erase(shapes.begin() + i);
			i--;
		}
	}
}
void DebugShapeCtx::fixed_update_start()
{
	one_frame_fixedupdate.clear();
}
