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
#include "LevelSerialization/SerializationAPI.h"
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
#include "EngineSystemCommands.h"
#include "DebugConsole.h"
#include "Scripting/ScriptManager.h"
#include "Scripting/ScriptFunctionCodegen.h"

Debug_Console* Debug_Console::inst = nullptr;

GameEngineLocal eng_local;
GameEnginePublic* eng = &eng_local;

static double program_time_start;
ConfigVar g_editor_cfg_folder("g_editor_cfg_folder", "Cfg", CVAR_DEV, "what folder to save .ini and other editor cfg to");

double GetTime()
{
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart()
{
	return GetTime() - program_time_start;
}

#include "Animation/SkeletonData.h"
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
		box
	}type;
	glm::vec3 pos;
	glm::vec3 size;
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

#include <mutex>
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
inline Color32 get_color_of_print(LogType type) {

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

ConfigVar loglevel("loglevel", "4", CVAR_INTEGER, "(0=disable,4=all)", 0, 4);
ConfigVar colorLog("colorLog", "1", CVAR_BOOL, "");
extern ConfigVar log_destroy_game_objects;
extern ConfigVar log_all_asset_loads;
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


extern ConfigVar g_project_name;

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
		float dist = glm::length(orbit_target - position);

		// panning
		float x_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX))*dist*0.2;
		float y_orb = -deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY)) *dist*0.2;

		if (pan_in_orbit_model&&!Input::last_recieved_input_from_con()) {	
			// scale by dist, not accurate, fixme	
			float x_s = tan(fov / 2) * dist * 0.5;
			float y_s = x_s * aratio;
			x_orb += x_s * x_off;
			y_orb += y_s * y_off;
		}
		orbit_target = orbit_target - real_up *  y_orb + right *  x_orb;



		position = orbit_target - front * dist;
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

#include "EditorPopupTemplate.h"

#ifdef EDITOR_BUILD
//TOGGLE_PLAY_EDIT_MAP
#if 0
void toggle_play_edit_map(const Cmd_Args& args)
{
	if (!eng->is_editor_level()) {
		auto level = eng->get_level();
		if(level)
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, string_format("start_ed Map %s", level->get_source_asset()->get_name().c_str()));
	}
	else {
		auto tool = eng->get_current_tool();
		auto level = eng->get_level();
		if (!level)
			return;
		if (!tool->get_asset_type_info().is_a(SceneAsset::StaticType)) {
			PopupTemplate::create_basic_okay(
				EditorPopupManager::inst,
				"Error",
				"Can only play Scene levels."
			);

			sys_print(Error, "can only play Scene levels\n");
			return;
		}

		auto source = level->get_source_asset();
		if (source) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, string_format("map %s", source->get_name().c_str()));
		}
		else 
			sys_print(Error, "no valid map");
	}
}
#endif

#if 0
DECLARE_ENGINE_CMD(EDITOR_BACK_ONE_PAGE)
{
	if (eng_local.engine_map_state_history.size() <= 1) {
		sys_print(Warning, "history empty\n");
		return;
	}
	eng_local.engine_map_state_future.push_back(eng_local.engine_map_state_history.back());
	eng_local.engine_map_state_history.pop_back();
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, eng_local.engine_map_state_history.back().c_str());
	eng_local.engine_map_state_history.pop_back();	// will get appended again
}
DECLARE_ENGINE_CMD(EDITOR_FORWARD_ONE_PAGE)
{
	if (eng_local.engine_map_state_future.empty()) {
		sys_print(Warning, "future empty\n");
		return;
	}
	eng_local.engine_map_state_history.push_back(eng_local.engine_map_state_future.back());
	eng_local.engine_map_state_future.pop_back();
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, eng_local.engine_map_state_history.back().c_str());
	eng_local.engine_map_state_history.pop_back();	// will get appended again
}
#endif
#if 0

void close_editor(const Cmd_Args& args)
{
	{
		eng_local.engine_map_state_history.push_back(
			"close_ed"
		);
	}
	eng_local.change_editor_state(nullptr,"");
}
#endif

void OpenEditorToolCommand::execute()
{
	sys_print(Debug, "OpenEditorToolCommand::execute\n");

	if (!eng_local.editorState) {
		sys_print(Error, "OpenEditorToolCommand: didnt launch in editor mode, use 'is_editor_app 1' in cfg or command line\n");
		return;
	}


	const AssetMetadata* metadata = AssetRegistrySystem::get().find_for_classtype(&assetType);
	if (metadata) {
		auto creationTool = metadata->create_create_tool_to_edit(assetName);
		if (creationTool) {
			eng_local.editorState->open_tool(std::move(creationTool), true, callback);
			return;
		}
		else {
			sys_print(Warning, "OpenEditorToolCommand::execute: no creation tool\n");
		}
	}
	else {
		sys_print(Warning, "OpenEditorToolCommand::execute: couldnt find asset metadata\n");
	}
	if(callback)
		callback(false);

}

void start_editor(const Cmd_Args& args)
{
	if (!eng_local.editorState) {
		sys_print(Error, "start_ed: didnt launch in editor mode, use 'is_editor_app 1' in cfg or command line\n");
		return;
	}

	static const char* usage_str = "Usage: starteditor [Map,AnimGraph,Model,<asset metadata typename>] <file>\n";
	if (args.size() != 2 && args.size()!=3) {
		sys_print(Info, usage_str);
		return;
	}

	//if (eng_local.is_wa) {
	//	sys_print(Error, "start_ed but waiting on an async map load call (%s), skipping.\n",eng_local.queued_mapname.c_str());
	//	return;
	//}

	std::string edit_type = args.at(1);
	const AssetMetadata* metadata = AssetRegistrySystem::get().find_type(edit_type.c_str());
	if (metadata) {
		opt<string> asset;
		if (args.size() >= 3)
			asset = args.at(2);

		auto creationTool = metadata->create_create_tool_to_edit(asset);
		if (creationTool) {
			eng_local.editorState->open_tool(std::move(creationTool), true, [](bool b) {});
		}
	}

	return;
#if 0
	if (metadata && metadata->tool_to_edit_me()) {

		const char* file_to_open = "";	// make a new map
		if (args.size() == 3)
			file_to_open = args.at(2);
		
		{
			std::string cmd = args.at(0);
			cmd += " ";
			cmd += args.at(1);
			if (args.size() == 3) {
				cmd += " ";
				cmd += args.at(2);
			}
			eng_local.engine_map_state_history.push_back(
				cmd
			);
		}

		if (eng_local.get_current_tool()) {
			std::string file_to_open_s = file_to_open;
			eng_local.get_current_tool()->try_close(
				[metadata, file_to_open_s]() {
					eng_local.change_editor_state(metadata->tool_to_edit_me(), metadata->get_arg_for_editortool(), file_to_open_s.c_str());
				}
			);
		}
		else {
			eng_local.change_editor_state(metadata->tool_to_edit_me(), metadata->get_arg_for_editortool(), file_to_open);
		}
	}
	else {
		sys_print(Error, "unknown editor\n");
		sys_print(Info, usage_str);
	}
#endif
}

#endif



#ifdef EDITOR_BUILD
#if 0
void GameEngineLocal::change_editor_state(IEditorTool* next_tool,const char* arg, const char* file)
{
	const char* str = string_format("%s:%s -> %s:%s",
		(active_tool) ? active_tool->get_asset_type_info().classname : "<none>",
		(active_tool) ? active_tool->get_doc_name().c_str() : "",
		(next_tool) ? next_tool->get_asset_type_info().classname : "<none>",
		(next_tool) ? file : ""
		);
	sys_print(Info, "--------- Change Editor State (%s) ---------\n",str);
	if (active_tool != next_tool && active_tool != nullptr) {
		// this should call close internally
		get_current_tool()->close();
		on_leave_editor.invoke();
	}
	active_tool = next_tool;
	if (active_tool != nullptr) {
		leave_level();
		enable_imgui_docking();
		bool could_open = get_current_tool()->open(file, arg);

		if (!could_open) {
			active_tool = nullptr;
		}
		else
			on_enter_editor.invoke(get_current_tool());
	}

	if (!active_tool)
		disable_imgui_docking();
}
#endif
#endif




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
	//keybinds.insert({ both,bind });
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
void GameEngineLocal::insert_this_map_as_level(SceneAsset*& loadedLevel, bool is_for_playing) {
	assert(is_waiting_on_map_load);
	is_waiting_on_map_load = false;

	string mapname = loadedLevel ? loadedLevel->get_name() : "<new level>";
	sys_print(Info, "Changing map: %s (for_playing=%s)\n", mapname.c_str(), print_get_bool_string(is_for_playing));

	if (editorState && editorState->has_tool()) {
		editorState->hide();
		assert(!editorState->get_tool());
	}
	if (level) {
		stop_game();
		assert(!level);
	}
	uptr<SceneAsset> unique_scene(loadedLevel);	// now its a unique ptr
	loadedLevel = nullptr;	// set caller to null since this deletes the sceneasset :)

	g_modelMgr.compact_memory();	// fixme, compacting memory here means newly loaded objs get moved twice, should be queuing uploads
	time = 0.0;
	set_tick_rate(60.f);
	level = make_unique<Level>(!is_for_playing);
	level->start(unique_scene.get());	// scene will then get destroyed
	idraw->on_level_start();

	if (app) {
		app->on_map_changed();
	}
	sys_print(Info, "changed state to Engine_State::Game\n");
}
void OpenMapCommand::execute()
{
	sys_print(Debug, "OpenMapCommand::execute\n");
	if (eng_local.is_waiting_on_map_load) {
		sys_print(Warning,"OpenMapCommand::execute(%s): already waiting on another OpenMapCommand\n", map_name.value_or("<empty>").c_str());
		if(callback)
			callback(OpenMapReturnCode::AlreadyLoadingMap);
	}
	else if (map_name.has_value()) {
		function<void(OpenMapReturnCode)> callback = this->callback;
		bool is_for_playing = this->is_for_playing;
		string mapname = map_name.value_or("<unnamed>");
		eng_local.is_waiting_on_map_load = true;
		double start_time = GetTime();

		uptr<SceneAsset> scene = std::make_unique<SceneAsset>();
		scene->editor_set_newly_made_path(mapname);
		bool success = scene->load_asset(g_assets.loader);
		if (success) {
			try {
				scene->post_load();
			}
			catch (...) {
				success = false;
			}
		}

		if (success) {
			auto scenePtr = scene.release();
			eng_local.insert_this_map_as_level(scenePtr, is_for_playing);
		}
		else {
			assert(eng_local.is_waiting_on_map_load);
			sys_print(Warning, "OpenMapCommand::execute(%s): failed to load\n", mapname.c_str());
			eng_local.is_waiting_on_map_load = false;
		}
		assert(!eng_local.is_waiting_on_map_load);
		auto code = success ? OpenMapReturnCode::Success : OpenMapReturnCode::FailedToLoad;
		double now = GetTime();
		sys_print(Debug, "OpenMapCommand::execute: took %f\n", float(now - start_time));

		if (callback)
			callback(code);
	}
	else {
		eng_local.is_waiting_on_map_load = true;
		SceneAsset* asset = nullptr;	
		eng_local.insert_this_map_as_level(asset, this->is_for_playing);
		assert(!eng_local.is_waiting_on_map_load);
		callback(OpenMapReturnCode::Success);
	}
}

#include "EditorPopupTemplate.h"

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
#include "LevelEditor/TagManager.h"

// tech debt nonsense
extern void IMPORT_TEX_FOLDER(const Cmd_Args& args);
extern void IMPORT_TEX(const Cmd_Args& args);
extern void COMPILE_TEX(const Cmd_Args& args);

void GameEngineLocal::add_commands()
{
	commands = ConsoleCmdGroup::create("");
	commands->add("IMPORT_TEX_FOLDER", IMPORT_TEX_FOLDER);
	commands->add("IMPORT_TEX", IMPORT_TEX);
	commands->add("COMPILE_TEX", COMPILE_TEX);
	commands->add("print_assets", [](const Cmd_Args&) { g_assets.print_usage(); });
	commands->add("TOGGLE_PLAY_EDIT_MAP", [this](const Cmd_Args&) {
		if (!get_level()) 
			return;
		if (get_level()->is_editor_level()) {
			Cmd_Manager::inst->append_cmd(std::make_unique<OpenMapCommand>(get_level()->get_source_asset_name(), true));
		}
		else {
			Cmd_Manager::inst->append_cmd(std::make_unique<OpenEditorToolCommand>(SceneAsset::StaticType,get_level()->get_source_asset_name(), true));
		}
		});
	commands->add("start_ed", start_editor);
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

	commands->add("bind", bind_key);
	commands->add("REG_GAME_TAG", [](const Cmd_Args& args) {
		if (args.size() != 2) {
			sys_print(Error, "REG_GAME_TAG <tag>");
			return;
		}
		GameTagManager::get().add_tag(args.at(1));
		});

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
#if 0
#include <variant>
using std::variant;
template<typename T>
class FutureExpc
{
public:
	FutureExpc(T data) : value(data) {}
	FutureExpc(std::exception_ptr except) : value(except) {}
	T get() {
		return std::visit(ValueOrThrow{}, value);
	}
private:
	struct ValueOrThrow {
		T operator()(const T& val) const { return val; }
		T operator()(const std::exception_ptr& ex) const { std::rethrow_exception(ex); }
	};
	variant<std::exception_ptr, T> value;
};


class AssetLoadError : public std::runtime_error {
public:
	AssetLoadError() : std::runtime_error("Asset load error.") {}
};
using AsyncAssetLoadResult = FutureExpc<IAsset*>;

void load_asset_new(AsyncAssetLoadResult result) {
	try {
		IAsset* asset = result.get();
	}
	catch (AssetLoadError er) {
		printf("%s", er.what());
	}
}


class ChangeMapCommand {
public:
	ChangeMapCommand(const string& map, bool force_change, function<void(FutureExpc<Level*>)> callback);
};
class StartPlayMapCommand {
public:
	StartPlayMapCommand(const string& map, bool force_change, function<void(FutureExpc<Level*>)> callback);
};
// start_ed Tool Asset

// for level editor to be valid:
// must have the level loaded
// 
// loading a prefab, load the level and the prefab in the callback
// then inserts them into the level

// change level command while editor is open
// issues a close editor command

class StartEditorCommand {
public:
	StartEditorCommand(const string& map, bool force_change, function<void(FutureExpc<IEditorTool*>)> callback);
};
class OpenEmptyMapCommand {
public:
};


// EditorTools have a method to create a CreateEditorCommand to put them back in the right state
// CreateCommands can have multiple constructors
// For example, one to create with a new map, one for existing map, one for putting back into old state
// These commands set up required state like loading maps etc. for the editors
class CreateEditorCommand {
public:
	virtual ~CreateEditorCommand() {}
	using Callback = function<void(FutureExpc<uptr<IEditorTool>>)>;
	virtual void execute(Callback callback) = 0;
};
class CreateLevelEditorCommand : public CreateEditorCommand {
public:
	CreateLevelEditorCommand();	// create empty map
	CreateLevelEditorCommand(const string& mapname);	// create from map
	CreateLevelEditorCommand(IEditorTool& existing);	// creates from existing state	
	void execute(Callback callback) {
		// step 1, load the map
		auto cmd = new ChangeMapCommand(mapname, true, [callback](FutureExpc<Level*> res) {
			uptr<IEditorTool> tool;
			try {
				Level* l = res.get();
				assert(l);
				// create editor tool
			}
			catch (...) {

			}
			// execute callback, could be a null tool
			callback(FutureExpc(std::move(tool)));
		});
		// add cmd to system
	}
	string mapname;
};
#endif

// multiple map workflow:
//		root_map,level0,(map0|map1|map2)

// map "map" -> PlayMap()
//		starts an async load,
//		then issues callback, then closes level
//		
//	 closes down cur level and editor state
//		
// 
// open_map "map" -> OpenMap()
//		all it does is open a map, unloads existing
// 
// open_empty_map -> OpenMap()
//		opens empty map
//
// close_map -> CloseMap()
//		all it does is close the map
//
// start_ed -> StartEditorCommand()
//		opens an editor in the editor state
//
// close_ed_tab -> CloseEditorTab()
//		closes the current editor tab
//
// save_ed -> SaveEditorTab()
// close_all_ed_tabs -> Close

#include "LevelEditor/EditorDocLocal.h"
#include "LevelEditor/Commands.h"
MulticastDelegate<> tempMD;


class EditorIntTesterUtil {
public:
	static void open_map_state(IntegrationTester& tester, opt<string> mapname) {
		double start = GetTime();
		auto cmd = make_unique<OpenMapCommand>(mapname, true);
		bool finished = false;
		cmd->callback = [start,&finished](OpenMapReturnCode code) {
			double now = GetTime();
			finished = true;
			printf("---TIME %f\n", float(now - start));
			tempMD.invoke();
		};
		Cmd_Manager::inst->append_cmd(std::move(cmd));
		tempMD.remove(&tester);
		if (!finished) {
			tester.wait_delegate(tempMD);
			tester.wait_ticks(1);
		}
	}
	static void open_editor_state(IntegrationTester& tester, const ClassTypeInfo& type, opt<string> map) {
		open_editor_state_shared(tester, type, map, true);
	}
	static void open_editor_state_dont_wait(IntegrationTester& tester, const ClassTypeInfo& type, opt<string> map) {
		open_editor_state_shared(tester, type, map, false);
	}
	static void open_editor_state_shared(IntegrationTester& tester, const ClassTypeInfo& type, opt<string> map, bool wait) {
		bool should_fail = false;
		double start = GetTime();
		auto edCmd = make_unique<OpenEditorToolCommand>(type, map, true);
		bool finished = false;
		edCmd->callback = [start, &tester, &finished,should_fail](bool b) {
			double now = GetTime();
			sys_print(Debug, "open_editor_state_shared: wait delegate calling\n");

			tester.checkTrue(!should_fail == b, "expected ed test wrong");
			finished = true;
			tempMD.invoke();
		};
		Cmd_Manager::inst->append_cmd(std::move(edCmd));
		tempMD.remove(&tester);
		sys_print(Debug, "adding wait delegate\n");
		if (!finished && wait) {
			tester.wait_delegate(tempMD);
			tester.wait_ticks(1);
		}
	}
	static EditorDoc& open_map_editor(IntegrationTester& tester, const ClassTypeInfo& type, opt<string> map) {
		EditorDoc* document = nullptr;
		EditorDoc::on_creation.add(&tester, [&](EditorDoc* ptr) {
			document = ptr;
			EditorDoc::on_creation.remove(&tester);
			});
		open_editor_state(tester, type, map);
		tester.checkTrue(document, "");
		tester.wait_ticks(1);
		return *document;
	}
	static void run_command(IntegrationTester& tester, EditorDoc& doc,Command* ptr, bool want_success = true) {
		opt<bool> res;
		doc.command_mgr->add_command_with_execute_callback(ptr, [&res](bool b) {
			res = b;
			});
		tester.wait_ticks(1);
		tester.checkTrue(res.has_value(), "command wasnt executed?");
		tester.checkTrue(res.value() == want_success, "mismatch command expected sucess");
	}
	static void remove_file(string path);
	static void undo_cmd(EditorDoc& doc) {
		doc.command_mgr->undo();
	}
	static Entity* find_with_name(string s) {
		auto l = eng->get_level();
		if (!l) return nullptr;
		for (auto e : l->get_all_objects()) {
			if (auto as_ent = e->cast_to<Entity>()) {
				if (as_ent->get_editor_name() == s)
					return as_ent;
			}
		}
		return nullptr;
	}
};

#include "Game/Components/PhysicsComponents.h"

void test_opening_prefab_as_map(IntegrationTester& tester)
{
	const string prefabPath = "EditorTestValidPfb.pfb";
	EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, prefabPath);
}

void test_remove_and_undo_pfb(IntegrationTester& tester)
{
	const string prefabPath = "EditorTestValidPfb.pfb";
	EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, std::nullopt);
	auto spawnPfb = new CreatePrefabCommand(doc, prefabPath, {});
	EditorIntTesterUtil::run_command(tester, doc, spawnPfb);
	auto preHandle = spawnPfb->handle;
	tester.checkTrue(preHandle.get(), "");
	auto removeCmd = new RemoveEntitiesCommand(doc, { preHandle });
	EditorIntTesterUtil::run_command(tester, doc, removeCmd);
	tester.checkTrue(!preHandle.get(), "");
	EditorIntTesterUtil::undo_cmd(doc);
	tester.checkTrue(preHandle.get(), "");
	//auto instPfb = new InstantiatePrefabCommand(doc, preHandle.get());
	//EditorIntTesterUtil::run_command(tester, doc, instPfb);
	tester.checkTrue(preHandle.get() && preHandle->get_object_prefab_spawn_type() == EntityPrefabSpawnType::None, "");
	EditorIntTesterUtil::undo_cmd(doc);
	tester.checkTrue(preHandle.get() && preHandle->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab, "");

	auto dupPfb = new DuplicateEntitiesCommand(doc, {preHandle});
	EditorIntTesterUtil::run_command(tester, doc, dupPfb);
	tester.checkTrue(dupPfb->handles.size() == 1, "");
	EntityPtr dupEntHandle = dupPfb->handles.at(0);
	tester.checkTrue(dupEntHandle && dupEntHandle->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab, "");

	auto parentCmd = new ParentToCommand(doc, { dupEntHandle.get() }, preHandle.get(), false, false);
	EditorIntTesterUtil::run_command(tester, doc, parentCmd);
	auto delCmd2 = new RemoveEntitiesCommand(doc, { preHandle });
	EditorIntTesterUtil::run_command(tester, doc, delCmd2);
	tester.checkTrue(!dupEntHandle.get(), "");
	EditorIntTesterUtil::undo_cmd(doc);
	tester.checkTrue(dupEntHandle.get(), "");
	tester.checkTrue(dupEntHandle && dupEntHandle->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab, "");
	auto delCmd3 = new RemoveEntitiesCommand(doc, { dupEntHandle });
	EditorIntTesterUtil::run_command(tester, doc, delCmd3);
	EditorIntTesterUtil::undo_cmd(doc);
	tester.checkTrue(dupEntHandle.get(), "");
	tester.checkTrue(dupEntHandle->get_parent() == preHandle.get(), "");
}

void test_editor_entity_ptr(IntegrationTester& tester)
{
	EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, std::nullopt);
	auto createJoint1 = new CreateCppClassCommand(doc, AdvancedJointComponent::StaticType.classname, {}, {},true);
	EditorIntTesterUtil::run_command(tester, doc, createJoint1);
	auto createObj = new CreateCppClassCommand(doc, "Entity", {}, {}, false);
	EditorIntTesterUtil::run_command(tester, doc, createObj);

	tester.checkTrue(createJoint1->handle, "");
	tester.checkTrue(createObj->handle, "");
	auto joint1 = createJoint1->handle->get_component<AdvancedJointComponent>();
	tester.checkTrue(joint1, "");
	joint1->set_target(createObj->handle.get());
	EntityPtr j1Owner = joint1->get_owner()->get_self_ptr();

	auto dup = new DuplicateEntitiesCommand(doc, { j1Owner,createObj->handle });
	EditorIntTesterUtil::run_command(tester, doc, dup);
	tester.checkTrue(dup->handles.size() == 2, "");
	auto j = dup->handles.at(0)->get_component<AdvancedJointComponent>();
	tester.checkTrue(j, "");
	tester.checkTrue(j->get_target() && j->get_target() == dup->handles.at(1).get(), "");
}

void test_loading_prefab_without_component(IntegrationTester& tester)
{
	const string prefabPath = "EditorTestInvalidPfbComponent.pfb";
	EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, PrefabAsset::StaticType, prefabPath);
	//Entity* root = doc.get_prefab_root_entity();
	//tester.checkTrue(root, "");
}

void test_state(IntegrationTester& tester)
{
	const string prefabPath = "EditorTestInvalidPfbComponent.pfb";
	EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, PrefabAsset::StaticType, std::nullopt);
	//Entity* root = doc.get_prefab_root_entity();
	//tester.checkTrue(root, "");
	EditorIntTesterUtil::run_command(tester, doc, new CreateStaticMeshCommand(doc, "SWAT_model.cmdl", {}));

	EditorIntTesterUtil::open_map_state(tester, "top_down/map0.tmap");

	tester.wait_time(200.0);
}

// What this does: creates a prefab object. creates a map with the prefab. deletes prefab from disk. tries loading the map again.
// checks that map load fails gracefully.
// put prefab back to disk. 
// load map again.
// check validity.
void test_loading_invalid_prefab(IntegrationTester& tester)
{
	const string prefabPath = "EditorTestInvalidPfb.pfb";
	const string mapPath = "EditorTestInvalidPfb.tmap";

	auto create_pfb_cmds = [&]() {
		EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, PrefabAsset::StaticType, std::nullopt);
		//Entity* root = doc.get_prefab_root_entity();
		//tester.checkTrue(root, "");
		auto cmd = new CreateStaticMeshCommand(doc, "eng/cube.cmdl", {});
		EditorIntTesterUtil::run_command(tester, doc, cmd);
		//tester.checkTrue(root->get_children().size() == 1, "");
		//.checkTrue(cmd->handle.get() && cmd->handle.get()->get_parent() == root, "");
		//doc.set_document_path(prefabPath);
		//tester.checkTrue(doc.save(), "");
		//PrefabAsset* pfb = g_assets.find_sync<PrefabAsset>(prefabPath).get();
		//tester.checkTrue(pfb && pfb->sceneFile->all_obj_vec.size() == 3, "");
	};

	// create prefab
	create_pfb_cmds();
	// create map
	{
		EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, std::nullopt);
		auto cmd = new CreatePrefabCommand(doc, prefabPath, {});
		EditorIntTesterUtil::run_command(tester, doc, cmd);
		tester.checkTrue(cmd->handle.get() && cmd->handle->get_object_prefab().get_name() == prefabPath, "");
		cmd->handle->set_editor_name("OBJECT");
		doc.set_document_path(mapPath);
		tester.checkTrue(doc.save(), "");

		// test duplicating and instantiating
		auto dupCmd = new DuplicateEntitiesCommand(doc, { cmd->handle });
		EditorIntTesterUtil::run_command(tester, doc, dupCmd);
		//auto isntCmd = new InstantiatePrefabCommand(doc, dupCmd->handles.at(0).get());
		//EditorIntTesterUtil::run_command(tester, doc, isntCmd);
		tester.checkTrue(doc.save(), "");	// try saving it, tests instantiating prefab works
		EditorIntTesterUtil::undo_cmd(doc);
		tester.checkTrue(doc.save(), "");
		EditorIntTesterUtil::undo_cmd(doc);
		tester.checkTrue(doc.save(), "");
	}
	{
		// close editor
		EditorIntTesterUtil::open_map_state(tester, std::nullopt);
		tester.checkTrue(eng->get_level(), "");
		tester.checkTrue(!eng_local.editorState->get_tool(), "");

	}
	{
		// remove from disk
		tester.checkTrue(FileSys::delete_game_file(prefabPath), "failed to delete");
		tester.wait_ticks(2);
		tester.checkTrue(!g_assets.is_asset_loaded(prefabPath),"failed to unload pfb");
	}
	{
		// load the map again
		EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, mapPath);
		Entity* obj = EditorIntTesterUtil::find_with_name("OBJECT");
		tester.checkTrue(obj, "");
		tester.checkTrue(obj->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab, "");
		tester.checkTrue(obj->get_object_prefab().get_name()==prefabPath, "");
		tester.checkTrue(obj->get_children().size() == 0, "");
		// try some operations

		auto dupCmd = new DuplicateEntitiesCommand(doc, { EntityPtr(obj->get_instance_id()) });
		EditorIntTesterUtil::run_command(tester, doc, dupCmd);
		tester.checkTrue(dupCmd->handles.size()==1&&dupCmd->handles.at(0).get(), "");
		auto dupObj = dupCmd->handles.at(0).get();
		tester.checkTrue(dupObj->get_object_prefab_spawn_type()==EntityPrefabSpawnType::RootOfPrefab, "");
		tester.checkTrue(dupObj->get_object_prefab().get_name()==prefabPath, "");
	//	auto instCmd = new InstantiatePrefabCommand(doc, dupObj);
	//	EditorIntTesterUtil::run_command(tester, doc, instCmd, false);	// want failure


		doc.save();
	}
	{
		// load the map again 2, after saving it
		EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, mapPath);
		Entity* obj = EditorIntTesterUtil::find_with_name("OBJECT");
		tester.checkTrue(obj, "");
	}
	// create the pfb again...
	create_pfb_cmds();
	// load the map x3
	{
		PrefabAsset* pfb = g_assets.find_sync<PrefabAsset>(prefabPath).get();
		tester.checkTrue(pfb,"");

		EditorDoc& doc = EditorIntTesterUtil::open_map_editor(tester, SceneAsset::StaticType, mapPath);
		Entity* obj = EditorIntTesterUtil::find_with_name("OBJECT");
		tester.checkTrue(obj, "");
		tester.checkTrue(obj->get_children().size() == 1, "");
	}
}

void test_integration_2(IntegrationTester& tester)
{
	
	sys_print(Info, "HELLO WORLD\n");
	auto open_map_state = [&](string name) {
		double start = GetTime();

		auto cmd = make_unique<OpenMapCommand>(name, true);
		cmd->callback = [start](OpenMapReturnCode code) {
			double now = GetTime();
			printf("---TIME %f\n", float(now - start));
			tempMD.invoke();
		};
		Cmd_Manager::inst->append_cmd(std::move(cmd));
		tester.wait_delegate(tempMD);
	};

	auto get_rand_ticks = [&]() -> int {
		float f = tester.get_rand().RandF(0, 1);
		if (f < 0.2) return 0;
		if (f < 0.5) return 1;
		return tester.get_rand().RandI(2, 50);
	};

	//open_map_state("top_down/map0.tmap");
	//tester.checkTrue(eng->get_level(),"");
	//tester.wait_time(2.0);
	//
	auto open_editor_state = [&](const ClassTypeInfo& type, opt<string> map, bool wait = true, bool should_fail=false) {
		double start = GetTime();
		auto edCmd = make_unique<OpenEditorToolCommand>(type, map, true);
		edCmd->callback = [start,&tester,should_fail](bool b) {
			double now = GetTime();
			printf("---TIME %f\n", float(now - start));
			tester.checkTrue(!should_fail == b,"expected ed test wrong");
			tempMD.invoke();
		};
		Cmd_Manager::inst->append_cmd(std::move(edCmd));
		if(wait)
			tester.wait_delegate(tempMD);
	};
	EditorDoc* document = nullptr;
	EditorDoc::on_creation.add(&tester, [&](EditorDoc* ptr) {
		document = ptr;
		EditorDoc::on_creation.remove(&tester);
		});
	open_editor_state(SceneAsset::StaticType, "top_down/map0.tmap",true,false);
	tester.checkTrue(document,"");
	tester.wait_ticks(1);

	auto run_command_and_get_good = [&](Command* ptr) -> bool {
		opt<bool> res;
		document->command_mgr->add_command_with_execute_callback(ptr, [&res](bool b) {
				res = b;
			});
		tester.wait_ticks(1);
		tester.checkTrue(res.has_value(), "command wasnt executed?");
		return res.value();
	};


	CreatePrefabCommand* cmd = new CreatePrefabCommand(*document, "top_down/player.pfb", glm::mat4(1.f));
	tester.checkTrue(run_command_and_get_good(cmd),"");
	DuplicateEntitiesCommand* dup = new DuplicateEntitiesCommand(*document, { cmd->handle });
	tester.checkTrue(run_command_and_get_good(dup), "");
	RemoveEntitiesCommand* rem = new RemoveEntitiesCommand(*document, dup->handles);
	tester.checkTrue(rem->handles.size() == 1, "has 1 handle");
	EntityPtr handle = rem->handles.at(0);
	tester.checkTrue(run_command_and_get_good(rem), "");
	tester.checkTrue(!handle.get(),"removed correctly");
	document->command_mgr->undo();
	tester.checkTrue(handle.get(), "undo removed correctly");



	tester.wait_time(200.0);

	//tester.checkTrue(eng_local.editorState->has_tool(),"");
	//tester.wait_ticks(get_rand_ticks());
	//tester.wait_time(0.2);
	open_editor_state(PrefabAsset::StaticType, std::nullopt,false,true);
	tester.wait_time(2.0);
	//tester.checkTrue(eng_local.editorState->has_tool(), "");
	tester.wait_ticks(get_rand_ticks());
	open_map_state("top_down/map0.tmap");
	tester.wait_ticks(get_rand_ticks());
	open_map_state("car/testmap.tmap");
	tester.wait_ticks(get_rand_ticks());
	open_editor_state(SceneAsset::StaticType, std::nullopt,false);
	//tester.checkTrue(eng_local.editorState->has_tool(), "");
	//tester.wait_ticks(get_rand_ticks());

	tester.wait_time(2.0);
	return;

//	Cmd_Manager::inst->append_cmd(EngineCommandBuilder::create_change_map("top_down/map0.tmap", true));// Cmd_Execute_Mode::APPEND, "map top_down/map0.tmap");
	//bool changeSuccesful = tester.wait_delegate(eng_local.on_map_load_return);
	//tester.checkTrue(changeSuccesful, "map change worked");

	g_assets.find_sync<PrefabAsset>("myprefabblah.pfb");// [](GenericAssetPtr) {
		//tempMD.invoke();
		//});
	//tester.wait_delegate(tempMD);
	tester.wait_ticks(1);
	tester.checkTrue(!EditorPopupManager::inst->has_popup_open(), "no popup after changing map");
	tester.checkTrue(!eng->is_editor_level(), "not editor level");
	//tester.checkTrue(!eng_local.is_in_an_editor_state(), "not editor state");
	tester.wait_time(0.1);
//	eng_local.leave_level();
	tester.wait_ticks(1);
	tester.checkTrue(!eng->get_level(), "");
	tester.wait_time(0.1);
	Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, "start_ed Map top_down/map0.tmap");
//	bool res = tester.wait_delegate(eng_local.on_map_load_return);
//	tester.checkTrue(res, "Must have opened editor");
	Level* l = eng->get_level();
	const bool conditions = l && l->is_editor_level() && l->get_source_asset_name() == "top_down/map0.tmap";
	tester.checkTrue(conditions, "level opened properly");
//	tester.checkTrue(eng_local.get_current_tool(), "editor opened properly");
	tester.wait_time(0.1);

	//load_asset_new(std::make_exception_ptr(AssetLoadError()));
}
#include "Testheader.h"

void GameEngineLocal::set_tester(IntegrationTester* tester, bool headless_mode) {
	this->tester.reset(tester);
	this->headless_mode = headless_mode;
}
extern ConfigVar developer_mode;
extern ConfigVar log_shader_compiles;
extern ConfigVar material_print_debug;
int game_engine_main(int argc, char** argv)
{

	material_print_debug.set_bool(true);
	developer_mode.set_bool(true);
	log_shader_compiles.set_bool(false);
	log_all_asset_loads.set_bool(true);
	loglevel.set_integer(4);
	eng_local.init(argc,argv);
	developer_mode.set_bool(true);
	//log_all_asset_loads.set_bool(false);
	log_destroy_game_objects.set_bool(false);
	//loglevel.set_integer(1);

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

	
	//c->buzzer();
	//int val = c->get_value("hello");
	//assert(val == 1);
	//assert(c->myStr == "hello");

	eng_local.loop();
	eng_local.cleanup();
	
	return 0;
}
ClassWithDelegate StaticClass::myClass;

ConfigVar g_project_name("g_project_name", "CsRemakeEngine", CVAR_DEV, "the project name of the game, used for save file folders");

ConfigVar g_mousesens("g_mousesens", "0.005", CVAR_FLOAT, "", 0.0, 1.0);
ConfigVar g_fov("fov", "70.0", CVAR_FLOAT, "", 10.0, 110.0);
ConfigVar g_thirdperson("thirdperson", "70.0", CVAR_BOOL,"");
ConfigVar g_fakemovedebug("fakemovedebug", "0", CVAR_INTEGER,"", 0, 2);
ConfigVar g_drawimguidemo("g_drawimguidemo", "0", CVAR_BOOL,"");
ConfigVar g_debug_skeletons("g_debug_skeletons", "0", CVAR_BOOL,"draw skeletons of active animated renderables");
ConfigVar g_draw_grid("g_draw_grid", "0", CVAR_BOOL,"draw a debug grid around the origin");
ConfigVar g_grid_size("g_grid_size", "1", CVAR_FLOAT, "size of g_draw_grid", 0.01,10);

// defualt sky material to use for editors like materials/models/etc.
ConfigVar ed_default_sky_material("ed_default_sky_material", "eng/hdriSky.mm", CVAR_DEV, "default sky material used for editors");

ConfigVar g_drawdebugmenu("g_drawdebugmenu","0",CVAR_BOOL, "draw the debug menu");

ConfigVar g_window_w("vid.width","1200",CVAR_INTEGER,"",1,4000);
ConfigVar g_window_h("vid.height", "800", CVAR_INTEGER, "",1, 4000);
ConfigVar g_window_fullscreen("vid.fullscreen", "0",CVAR_BOOL,"");
ConfigVar g_host_port("net.hostport","47000",CVAR_INTEGER|CVAR_READONLY,"",0,UINT16_MAX);
ConfigVar g_dontsimphysics("stop_physics", "0", CVAR_BOOL | CVAR_DEV,"");

ConfigVar developer_mode("developer_mode", "1", CVAR_DEV | CVAR_BOOL, "enables dev mode features like compiling assets when loading");

ConfigVar g_slomo("slomo", "1.0", CVAR_FLOAT | CVAR_DEV, "multiplier of dt in update loop", 0.0001, 5.0);






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
#endif
	if (editorState)
		editorState.reset();
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

ConfigVar g_drawconsole("drawconsole", "0", CVAR_BOOL, "draw the console");
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
		isound->set_listener_position(vs->origin, glm::normalize(glm::cross(vs->front, glm::vec3(0, 1, 0))));

		if (!vs) {
			params.draw_world = false;
			vs = &vs_for_gui;
		}
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
#include "Assets/AssetBundle.h"
#include "Framework/StringUtils.h"
#include "Scripting/ScriptManager.h"
using std::make_unique;

ImFont* global_big_imgui_font = nullptr;

ConfigVar is_editor_app("is_editor_app", "0", CVAR_BOOL, "");
extern ConfigVar g_application_class;

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
		sys_print(Debug, "init %s in %fs\n", msg, float(now-start));
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

	// doesnt matter if it fails, just precache loading stuff
	g_assets.find_sync<AssetBundle>("eng/default_bundle.bundle", true);
	print_time("load default bundle");

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


	// not in editor and no queued map, load the entry point

	
#ifdef EDITOR_BUILD
	AssetRegistrySystem::get().init();
	
	auto start_game = [&]() {
		sys_print(Info, "starting game...\n");
		Application* a = ClassBase::create_class<Application>(g_application_class.get_string());
		if (!a) {
			Fatalf("Engine::init: application class not found %s\n", g_application_class.get_string());
		}
		app.reset(a);
		Input::on_con_status.add(a, [this](int i, bool b) {
			app->on_controller_status(i, b);
			});

		app->start();
	};
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

	sys_print(Info, "execute startup in %f\n", (float)TimeSinceStart());
}
ConfigVar g_application_class("g_application_class", "Application", CVAR_DEV, "");
ConfigVar with_threading("with_threading", "1", CVAR_BOOL | CVAR_DEV, "");

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
	try {
		if (level)
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

	return true;
}

struct GameUpdateOuput {
	bool drawOut = false;
	SceneDrawParamsEx paramsOut = SceneDrawParamsEx(0, 0);
	View_Setup vsOut;
};
void game_update_job(uintptr_t user)
{
	ZoneScopedN("GameThreadUpdate");
	CPUFUNCTIONSTART;

	//printf("abc\n");
	auto out = (GameUpdateOuput*)user;
	out->drawOut = eng_local.game_thread_update();
	eng_local.get_draw_params(out->paramsOut, out->vsOut);
	//return;
	// update particles, doesnt draw, only builds meshes FIXME
	ParticleMgr::inst->draw(out->vsOut);
}

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
		game_update_job(uintptr_t(&out));
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
		ZoneScopedN("ImguiDraw");

		ImGui::Render();
		if (!skip_rendering) {
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		}
	};
	auto imgui_update = [&]()
	{
		ZoneScopedN("ImGuiUpdate");
		gui_log.draw(UiSystem::inst->window);
		UiSystem::inst->draw_imgui_interfaces(editorState.get());
	};

	auto do_sync_update = [&]()
	{
		ZoneScopedN("SyncUpdate");
		DebugShapeCtx::get().update(frame_time);
#ifdef EDITOR_BUILD
		AssetRegistrySystem::get().update(); 		// update hot reloading
		ScriptManager::inst->check_for_reload();	// do this here? this does script reloading stuff
#endif
		if (get_level())
			get_level()->sync_level_render_data();
		UiSystem::inst->sync_to_renderer();
		g_physics.sync_render_data();		
		idraw->sync_update();
	};
	auto wait_for_swap = [&](const bool skip_rendering)
	{
		ZoneScopedN("SwapWindow");
		CPUSCOPESTART(SwapWindow);
		if(!skip_rendering)
			SDL_GL_SwapWindow(window);
	};

	auto pre_update = [&]() {
		
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

			if (tester) {
				bool res = tester->tick(dt);
				if (res) {
					Quit();
				}
			}

			// update input, console cmd buffer, could change maps etc.
			frame_start();

			pre_update();

			// overlapped update (game+render)
			do_overlapped_update(shouldDrawNext, drawparamsNext, setupNext, skip_rendering);

			// sync period
			imgui_update();	// fixme
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
		const char* match = Cmd_Manager::get()->print_matches(console->input_buffer);
		console->scroll_to_bottom = true;
		if (match) {
			data->DeleteChars(0, data->BufTextLen);
			data->InsertChars(0, match);
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
	const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
	if (ImGui::BeginChild("ScrollingRegion", ImVec2(0, -footer_height_to_reserve), false, ImGuiWindowFlags_HorizontalScrollbar))
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


// SDL_CONTROLLER_AXIS_LEFTX = 0
// SDL_CONTROLLER_AXIS_LEFTY = 1

void update_char() {
	SDL_GameControllerAxis moveAxisX = SDL_CONTROLLER_AXIS_LEFTX;
	SDL_GameControllerAxis moveAxisY = SDL_CONTROLLER_AXIS_LEFTY;
	if (Input::is_shift_down() && Input::is_alt_down() && Input::is_mouse_down(0)) {
		
	}
	bool send_nav_next = false;

	static float next_time = 0.0;
	if (Input::is_any_con_active()) {
		const bool down = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
		if (!down) {
			next_time = 0.0;
		}
		else if (down && GetTime() > next_time) {
			send_nav_next = true;
			next_time = GetTime() + 0.4;
		}
	}

	vec2 move{};
	move.x = Input::get_con_axis(moveAxisX);
	move.y = Input::get_con_axis(moveAxisY);

	const bool INVERT_Y = false;
	if (INVERT_Y)
		move.y = -move.y;
	const float SENS = 0.01;
	move *= SENS;
	const float DEADZONE = 0.1;
	if (glm::abs(move.y) < DEADZONE)move.y = 0;
	if (glm::abs(move.x) < DEADZONE)move.x = 0;
	if (glm::length(move) > 1)
		move = move / glm::length(move);

	bool wants_shoot = false;
	if (Input::is_mouse_down(0))
		wants_shoot = true;
	if (Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 0.5)
		wants_shoot = true;


	Input::is_con_button_down(SDL_CONTROLLER_BUTTON_A);

	if(Input::is_key_down(SDL_SCANCODE_W)) {

	}


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
