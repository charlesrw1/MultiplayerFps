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

#include "Scripting/ScriptManagerPublic.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"

#include "tracy/public/tracy/Tracy.hpp"
#include "tracy/public/tracy/TracyOpenGL.hpp"

#include "Framework/Jobs.h"

GameEngineLocal eng_local;
GameEnginePublic* eng = &eng_local;

static double program_time_start;

bool CheckGlErrorInternal_(const char* file, int line)
{
	GLenum error_code = glGetError();
	bool has_error = 0;
	while (error_code != GL_NO_ERROR)
	{
		has_error = true;
		const char* error_name = "Unknown error";
		switch (error_code)
		{
		case GL_INVALID_ENUM:
			error_name = "GL_INVALID_ENUM"; break;
		case GL_INVALID_VALUE:
			error_name = "GL_INVALID_VALUE"; break;
		case GL_INVALID_OPERATION:
			error_name = "GL_INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW:
			error_name = "GL_STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW:
			error_name = "GL_STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY:
			error_name = "GL_OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION:
			error_name = "GL_INVALID_FRAMEBUFFER_OPERATION"; break;
		default:
			break;
		}
		sys_print(Error, "%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
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

class Debug_Console
{
public:
	static const int INPUT_BUFFER_SIZE = 256;
	Debug_Console() {
		memset(input_buffer, 0, sizeof input_buffer);
	}
	void init();
	void draw();
	void print(const char* fmt, ...);
	void print_args(const char* fmt, va_list list);
	vector<string> lines;
	vector<string> history;
	int history_index = -1;
	bool auto_scroll = true;
	bool scroll_to_bottom = false;
	bool set_keyboard_focus = false;
	char input_buffer[INPUT_BUFFER_SIZE];
};
static Debug_Console dbg_console;

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
	sys_print(Info, "Quiting...\n");
	eng_local.cleanup();
	exit(0);
}

ConfigVar loglevel("loglevel", "4", CVAR_INTEGER, "(0=disable,4=all)", 0, 4);



void sys_print(LogType type, const char* fmt, ...)
{
	if ((int(type)) > loglevel.get_integer())
		return;

	va_list args;
	va_start(args, fmt);
	
	int len = strlen(fmt);
	bool print_end = true;
	if (type == LogType::Error) {
		printf("\033[91m");
	}
	else if (type==LogType::Warning) {
		printf("\033[33m");
	}
	else if (type==LogType::Debug) {
		printf("\033[32m");
	}
	else if (len >= 1 && fmt[0] == '>') {
		printf("\033[35m");
		char buf[1024];
		vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
		buf[IM_ARRAYSIZE(buf) - 1] = 0;
		eng->log_to_fullscreen_gui(Debug, buf);
	}
	else
		print_end = false;


	std::lock_guard<std::mutex> printLock(printMutex);
	vprintf(fmt, args);
	dbg_console.print_args(fmt, args);
	va_end(args);

	if (print_end)
		printf("\033[0m");
}


void sys_vprint(const char* fmt, va_list args)
{
	std::lock_guard<std::mutex> printLock(printMutex);

	vprintf(fmt, args);
	dbg_console.print_args(fmt, args);
}

void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	sys_vprint(format, list);
	va_end(list);
	fflush(stdout);
	{
		std::lock_guard<std::mutex> printLock(printMutex);
		eng_local.cleanup();
	}
	exit(-1);
}
double GetTime()
{
	return SDL_GetPerformanceCounter() / (double)SDL_GetPerformanceFrequency();
}
double TimeSinceStart()
{
	return GetTime() - program_time_start;
}


void GameEngineLocal::log_to_fullscreen_gui(LogType type, const char* msg)
{
	const Color32 err = { 255, 105, 105 };
	const Color32 warn = { 252, 224, 121 };
	const Color32 info = COLOR_WHITE;
	const Color32 debug = { 136, 161, 252 };
	Color32 out = info;
	if (type == LogType::Error) out = err;
	else if (type == LogType::Warning)out = warn;
	else if (type == LogType::Debug) out = debug;

	gui_log->add_text(out, msg);
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
	return orbit_mode || eng->is_game_focused();
}

static void vector_to_angles(const glm::vec3& v, float& pitch, float& yaw) {
	pitch = std::atan2(v.y, std::sqrt(v.x * v.x + v.z * v.z));
	yaw = std::atan2(v.x, v.z);
}

void User_Camera::set_orbit_target(glm::vec3 target, float object_size)
{
	orbit_target = target;
	position = orbit_target - front * object_size * 4.f;
}


void User_Camera::scroll_callback(int amt)
{
	if(orbit_mode) {
		float lookatpointdist = dot(position - orbit_target, front);
		glm::vec3 lookatpoint = position + front * lookatpointdist;
		lookatpointdist += (lookatpointdist * 0.25) * amt;
		if (abs(lookatpointdist) < 0.01)
			lookatpointdist = 0.01;
		position = (lookatpoint - front * lookatpointdist);
	}
	else {
		move_speed += (move_speed * 0.5) * amt;
		if (abs(move_speed) < 0.000001)
			move_speed = 0.0001;
	}
}

void User_Camera::update_from_input(const bool keys[], int mouse_dx, int mouse_dy, int width, int height, float aratio, float fov)
{
	int xpos, ypos;
	xpos = mouse_dx;
	ypos = mouse_dy;

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

		auto keystate = SDL_GetKeyboardState(nullptr);
		bool pan_in_orbit_model = keystate[SDL_SCANCODE_LSHIFT];

		if (!pan_in_orbit_model) {
			update_pitch_yaw();
		}

		front = AnglesToVector(pitch, yaw);
		glm::vec3 right = normalize(cross(up, front));
		glm::vec3 real_up = glm::cross(right, front);
		float dist = glm::length(orbit_target - position);

		// panning
		if (pan_in_orbit_model) {
			// scale by dist, not accurate, fixme
			
			float x_s = tan(fov / 2) * dist * 0.5;
			float y_s = x_s * aratio;
			orbit_target = orbit_target - real_up * y_off * y_s + right * x_off * x_s;
		}

		position = orbit_target - front * dist;
	}
	else
	{
		update_pitch_yaw();

		front = AnglesToVector(pitch, yaw);
		glm::vec3 delta = glm::vec3(0.f);
		vec3 right = normalize(cross(up, front));
		if (keys[SDL_SCANCODE_W])
			delta += move_speed * front;
		if (keys[SDL_SCANCODE_S])
			delta -= move_speed * front;
		if (keys[SDL_SCANCODE_A])
			delta += right * move_speed;
		if (keys[SDL_SCANCODE_D])
			delta -= right * move_speed;
		if (keys[SDL_SCANCODE_Z])
			delta += move_speed * up;
		if (keys[SDL_SCANCODE_X])
			delta -= move_speed * up;
		position += delta;
	}
}

void GameEngineLocal::connect_to(string address)
{
	ASSERT(0);
	///travel_to_engine_state(Engine_State::Loading, "connecting to another server");

	sys_print(Info, "Connecting to server %s\n", address.c_str());
	//cl->connect(address);
}

#ifdef EDITOR_BUILD
DECLARE_ENGINE_CMD(TOGGLE_PLAY_EDIT_MAP)
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
			sys_print(Error, "can only play Scene levels\n");
			return;
		}
		auto source = level->get_source_asset();
		if (source) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, string_format("map %s", source->get_name().c_str()));
		}
		else sys_print(Error, "no valid map");
	}
}
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

DECLARE_ENGINE_CMD(close_ed)
{
	{
		eng_local.engine_map_state_history.push_back(
			"close_ed"
		);
	}
	eng_local.change_editor_state(nullptr,"");
}


DECLARE_ENGINE_CMD(start_ed)
{
	static const char* usage_str = "Usage: starteditor [Map,AnimGraph,Model,<asset metadata typename>] <file>\n";
	if (args.size() != 2 && args.size()!=3) {
		sys_print(Info, usage_str);
		return;
	}

	if (eng_local.is_waiting_on_load_level_callback) {
		sys_print(Error, "start_ed but waiting on an async map load call (%s), skipping.\n",eng_local.queued_mapname.c_str());
		return;
	}

	std::string edit_type = args.at(1);
	const AssetMetadata* metadata = AssetRegistrySystem::get().find_type(edit_type.c_str());
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


		eng_local.change_editor_state(metadata->tool_to_edit_me(),metadata->get_arg_for_editortool(), file_to_open);
	}
	else {
		sys_print(Error, "unknown editor\n");
		sys_print(Info, usage_str);
	}
}

#endif
static void enable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
static void disable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable);
}

#ifdef EDITOR_BUILD
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
	}
	active_tool = next_tool;
	if (active_tool != nullptr) {
		leave_level();
		enable_imgui_docking();
		global_asset_browser.init();
		bool could_open = get_current_tool()->open(file, arg);
		if (!could_open) {
			active_tool = nullptr;
		}
	}

	if (!active_tool)
		disable_imgui_docking();
}
#endif



DECLARE_ENGINE_CMD(quit)
{
	Quit();
}


std::string* GameEngineLocal::find_keybind(SDL_Scancode code, uint16_t keymod) {

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
	keybinds.insert({ both,bind });
}


DECLARE_ENGINE_CMD(bind)
{
	if (args.size() < 2) return;
	SDL_Scancode scancode = SDL_GetScancodeFromName(args.at(1));
	if (scancode == SDL_SCANCODE_UNKNOWN) return;
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

DECLARE_ENGINE_CMD_CAT("cl.",force_update)
{
	//eng->get_client()->ForceFullUpdate();
}

DECLARE_ENGINE_CMD(connect)
{
	if (args.size() < 2) {
		sys_print(Info, "usage connect <address>");
		return;
	}

	eng_local.connect_to(args.at(1));
}
DECLARE_ENGINE_CMD(disconnect)
{
	eng->leave_level();
}
DECLARE_ENGINE_CMD(reconnect)
{
	//if(eng->get_client()->get_state() != CS_DISCONNECTED)
	//	eng->get_client()->Reconnect();
}

// open a map for playing
DECLARE_ENGINE_CMD(map)
{
	if (args.size() < 2) {
		sys_print(Info, "usage: map <map name>");
		return;
	}

	if (eng_local.is_waiting_on_load_level_callback) {
		sys_print(Error, "map called but already waiting on another async map load (%s), skipping.\n", eng_local.queued_mapname.c_str());
		return;
	}

#ifdef EDITOR_BUILD
	if (eng->get_current_tool() != nullptr) {
		sys_print(Warning,"starting game so closing any editors\n");
		eng_local.change_editor_state(nullptr,"");	// close any editors
	}

	{
		std::string cmd = args.at(0);
		cmd += " ";
		cmd += args.at(1);
		eng_local.engine_map_state_history.push_back(
			cmd
		);
	}
#endif

	eng->open_level(args.at(1));
}
extern ConfigVar g_entry_level;
// start game from the entry map
DECLARE_ENGINE_CMD(goto_entry_map)
{
	if (args.size() != 1) {
		sys_print(Info,"usage: goto_entry_map");
		return;
	}

	const char* cmd = string_format("map %s\n", g_entry_level.get_string());
	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, cmd);
}

DECLARE_ENGINE_CMD(exec)
{
	if (args.size() < 2) {
		sys_print(Info,"usage: exec <exec filename>");
		return;
	}

	Cmd_Manager::get()->execute_file(Cmd_Execute_Mode::NOW, args.at(1));
}
DECLARE_ENGINE_CMD(toggle)
{
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

DECLARE_ENGINE_CMD(inc)
{
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
}
DECLARE_ENGINE_CMD(dec)
{
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
}


DECLARE_ENGINE_CMD(net_stat)
{
	float mintime = INFINITY;
	float maxtime = -INFINITY;
	int maxbytes = -5000;
	int totalbytes = 0;
	//for (int i = 0; i < 64; i++) {
	//	auto& entry = eng->get_client()->server.incoming[i];
	//	maxbytes = glm::max(maxbytes, entry.bytes);
	//	totalbytes += entry.bytes;
	//	mintime = glm::min(mintime, entry.time);
	//	maxtime = glm::max(maxtime, entry.time);
	//}

	sys_print(Debug,"Client Network Stats:\n");
	//sys_print(Debug,"%--15s %f\n", "Rtt", eng->get_client()->server.rtt);
	sys_print(Debug,"%--15s %f\n", "Interval", maxtime - mintime);
	sys_print(Debug,"%--15s %d\n", "Biggest packet", maxbytes);
	sys_print(Debug,"%--15s %f\n", "Kbits/s", 8.f*(totalbytes / (maxtime-mintime))/1000.f);
	sys_print(Debug,"%--15s %f\n", "Bytes/Packet", totalbytes / 64.0);
}


DECLARE_ENGINE_CMD(reload_shaders)
{
	idraw->reload_shaders();
}

#include "Input/InputAction.h"

class SwizzleModifier : public InputModifier
{
public:
	SwizzleModifier(bool swizzle, bool negate, float exp = 1.f) : swizzle(swizzle), negate(negate), exp(exp) {}
	InputValue modify(InputValue value, float dt) const {

		float f = value.v.x;
		if (abs(f) <= 0.3)
			f = 0;

		f = glm::pow(abs(f), exp) * glm::sign(f);

		if (negate) {
			f = -f;
		}
		
		InputValue out;
		if (swizzle)
			out.v.y = f;
		else
			out.v.x = f;

		return out;
	}
	bool swizzle;
	bool negate;
	float exp;
};


class LookModifier : public InputModifier
{
public:
	LookModifier(bool swizzle) : swizzle(swizzle) {}
	InputValue modify(InputValue value, float dt) const {
		if (swizzle)
			std::swap(value.v.x, value.v.y);
		value.v *= g_mousesens.get_float();
		return value;
	}
	bool swizzle;
};
class LookModifierController : public InputModifier
{
public:
	LookModifierController(bool swizzle) : swizzle(swizzle) {}
	InputValue modify(InputValue value, float dt) const {

		float f = value.v.x;
		if (abs(f) <= 0.05)
			f = 0;

		float exp = 2.0;
		f = glm::pow(abs(f),exp) * glm::sign(f);

		f *= 0.1f;
		InputValue v;
		if (swizzle) {
			v.v.y = f;
		}
		else
			v.v.x = f;
		return v;
	}
	bool swizzle;
};
class BasicButtonTrigger : public InputTrigger
{
public:
	BasicButtonTrigger(float thresh = 0.5f) : thresh(thresh){
	}
	TriggerMask check_trigger(InputValue value, float dt) const {
 		return value.get_value<float>() >= thresh ? TriggerMask::Active : TriggerMask();
	}

	float thresh;
};

void register_input_actions_for_game()
{
	using IA = InputAction;

	IA::register_action("game", "move", true)
		->add_bind("x", IA::controller_axis(SDL_CONTROLLER_AXIS_LEFTX), new SwizzleModifier(false,true,1.0), nullptr)
		->add_bind("y", IA::controller_axis(SDL_CONTROLLER_AXIS_LEFTY), new SwizzleModifier(true, true,1.0), nullptr)
		->add_bind("y+", IA::keyboard_key(SDL_SCANCODE_W), new SwizzleModifier(true, false), nullptr)
		->add_bind("y-", IA::keyboard_key(SDL_SCANCODE_S), new SwizzleModifier(true, true), {})
		->add_bind("x-", IA::keyboard_key(SDL_SCANCODE_A), new SwizzleModifier(false, false), {})
		->add_bind("x+", IA::keyboard_key(SDL_SCANCODE_D), new SwizzleModifier(false, true), {});

	IA::register_action("game", "look", true)
		->add_bind("x", GIB::MouseX, new LookModifier(false), {})
		->add_bind("y", GIB::MouseY, new LookModifier(true), {})
		->add_bind("x", IA::controller_axis(SDL_CONTROLLER_AXIS_RIGHTX), new LookModifierController(false), {})
		->add_bind("y", IA::controller_axis(SDL_CONTROLLER_AXIS_RIGHTY), new LookModifierController(true), {});
	IA::register_action("game", "shoot", false)
		->add_bind("", GIB::MBLeft, {}, new BasicButtonTrigger())
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), {}, new BasicButtonTrigger());

	IA::register_action("game", "sprint")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER), {}, {})
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_LSHIFT), {}, {});
	IA::register_action("game", "inv_next")
		->add_bind("", GIB::MouseScroll, {}, {})
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), {}, {});
	IA::register_action("game", "inv_prev")
		->add_bind("", GIB::MouseScroll, {}, {})
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT), {}, {});
	IA::register_action("game", "inv_0")
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_0), {}, {});
	IA::register_action("game", "inv_1")
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_1), {}, {});
	IA::register_action("game", "inv_2")
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_2), {}, {});
	IA::register_action("game", "crouch")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER), {}, {});

	IA::register_action("game", "jump")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_A), nullptr, new BasicButtonTrigger())
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_SPACE), nullptr, new BasicButtonTrigger());
	IA::register_action("game", "test1")
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_Z), nullptr, new BasicButtonTrigger());

	IA::register_action("ui", "right")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT), {}, {})
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_RIGHT), {}, {});
	IA::register_action("ui", "menu")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_START), {}, new BasicButtonTrigger())
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_ESCAPE), {}, new BasicButtonTrigger());
	IA::register_action("game", "reload")
		->add_bind("", IA::controller_button(SDL_CONTROLLER_BUTTON_X), {}, new BasicButtonTrigger())
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_R), {}, new BasicButtonTrigger());

	IA::register_action("game", "accelerate")
		->add_bind("", IA::controller_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT), {}, {})
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_W), {}, {});
	IA::register_action("game", "deccelerate")
		->add_bind("", IA::controller_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), {}, {})
		->add_bind("", IA::keyboard_key(SDL_SCANCODE_S), {}, {});
}


int main(int argc, char** argv)
{
	eng_local.init(argc,argv);	
	eng_local.loop();
	eng_local.cleanup();
	
	return 0;
}

// Set these in vars.txt (or init.txt) located in $ROOT
// They function identically, but vars is preferred for const configuration and init for short term changes
// init.txt is ran after vars.txt, so it will overwrite

// The entry point of the game! (not used in the editor)
// Takes in a string of the level to start with on the final game
// Should look like: "mylevel.tmap" for $ROOT/gamedat/mylevel.tmap
ConfigVar g_entry_level("g_entry_level", "", CVAR_DEV, "the entry point of the game, this takes in a level filepath");

ConfigVar g_gamemain_class("g_gamemain_class", "GameMain", CVAR_DEV, "the default gamemain class of the program");
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


void GameEngineLocal::open_level(string nextname)
{
	// level will get loaded in next ::loop()
	queued_mapname = nextname;
	state = Engine_State::Loading;
}

void GameEngineLocal::leave_level()
{
	// current map gets unloaded in next ::loop()
	state = Engine_State::Idle;
}


void GameEngineLocal::on_map_change_callback(bool this_is_for_editor, SceneAsset* loadedLevel)
{
	sys_print(Info, "on_map_change_callback %s\n",(loadedLevel)?loadedLevel->get_name().c_str():"<nullptr>");

	g_assets.remove_unreferences();
	g_modelMgr.compact_memory();	// fixme, compacting memory here means newly loaded objs get moved twice, should be queuing uploads

	ASSERT(!level);
	ASSERT(is_waiting_on_load_level_callback);

	is_waiting_on_load_level_callback = false;
	is_loading_editor_level = false;

	if (!loadedLevel) {
		sys_print(Error, "couldn't load map !!!\n");
		state = Engine_State::Idle;

		on_map_load_return.invoke(false);
		return;
	}

	// constructor initializes level state
	this->level = std::make_unique<Level>();
	this->level->create(loadedLevel, this_is_for_editor);

	time = 0.0;
	set_tick_rate(60.f);

	idraw->on_level_start();

	sys_print(Info, "changed state to Engine_State::Game\n");

	state = Engine_State::Game;

	on_map_load_return.invoke(true);
}

void GameEngineLocal::execute_map_change()
{
	sys_print(Info, "-------- Map Change: %s --------\n", queued_mapname.c_str());

	// free current map
	stop_game();
	ASSERT(!level);

	// try loading map
#ifdef EDITOR_BUILD
	const bool this_is_for_editor = is_in_an_editor_state();
#else
	const bool this_is_for_editor = false;
#endif
	is_loading_editor_level = this_is_for_editor;	// set temporary variable

	ASSERT(!is_waiting_on_load_level_callback);
	is_waiting_on_load_level_callback = true;	// flipped to false in on_map_change_callback

	// special name to create a map
	if (this_is_for_editor && queued_mapname == "__empty__") {	

		// not memory leak, gets cleaned up
		SceneAsset* temp = new SceneAsset;
		g_assets.install_system_asset(temp, "empty.tmap");
		on_map_change_callback(true /* == this_is_for_editor */, temp);
	}
	else {
		g_assets.find_async<SceneAsset>(queued_mapname, [this_is_for_editor](GenericAssetPtr ptr)
			{
				auto level = (ptr)?ptr.cast_to<SceneAsset>():nullptr;
				eng_local.on_map_change_callback(this_is_for_editor, level.get());

			}, 0 /* default lifetime channel 0*/);

		// goto idle while waint for loading to finish
		state = Engine_State::Idle;
	}
}


void GameEngineLocal::spawn_starting_players(bool initial)
{
	// fixme for multiplayer, spawn in clients that are connected to server ex on restart
	login_new_player(0);	// player 0
}

void GameEngineLocal::set_tick_rate(float tick_rate)
{
	if (state == Engine_State::Game) {
		sys_print(Warning, "Can't change tick rate while running\n");
		return;
	}
	tick_interval = 1.0 / tick_rate;
}

void GameEngineLocal::set_game_focused(bool focused)
{
	if (focused == game_focused)
		return;

	if (focused) {
		// reset deltas
		SDL_GetRelativeMouseState(nullptr, nullptr);
		SDL_GetMouseState(&saved_mouse_x, &saved_mouse_y);
		SDL_SetRelativeMouseMode(SDL_TRUE);
	}
	else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
	}
	game_focused = focused;
}

void GameEngineLocal::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;
		//console.set_keyboard_focus = show_console;
	}

	const bool is_in_game_level = eng->get_level() && !eng->is_editor_level();
	if (!is_in_game_level && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse) {
		set_game_focused(false);
		return;
	}
	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard && event.key.keysym.mod == 0 /* if mod is active, then skip this BS ImGui taking over input*/)
		return;

	if (event.type == SDL_KEYDOWN) {

		SDL_Scancode scancode = event.key.keysym.scancode;
		inp.keys[scancode] = true;
		inp.keychanges[scancode] = true;

		// check keybind activation
		std::string* keybind = find_keybind(scancode, event.key.keysym.mod);
	
		if (keybind != nullptr) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, keybind->c_str());
		}
	}
	else if (event.type == SDL_KEYUP) {
		inp.keys[event.key.keysym.scancode] = false;
	}
	// when drawing to windowed viewport, handle game_focused during drawing
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (!is_in_game_level && event.button.button == 3 && !is_drawing_to_window_viewport()) {
			set_game_focused(true);
		}
		inp.mousekeys |= (1<<event.button.button);
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		if (!is_in_game_level && event.button.button == 3 && !is_drawing_to_window_viewport()) {
			set_game_focused(false);
		}
		inp.mousekeys &= ~(1 << event.button.button);
	}
}

void GameEngineLocal::cleanup()
{
#ifdef EDITOR_BUILD
	if (get_current_tool())
		get_current_tool()->close();
#endif
	isound->cleanup();

	// could get fatal error before initializing this stuff
	if (gl_context && window) {
		//NetworkQuit();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}


	gui_sys.reset(nullptr);
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
#ifdef EDITOR_BUILD
	return is_in_an_editor_state();
#else
	return false;
#endif
}

glm::ivec2 GameEngineLocal::get_game_viewport_size() const
{
	if (is_drawing_to_window_viewport())
		return window_viewport_size;
	else
		return { g_window_w.get_integer(), g_window_h.get_integer() };
}


static bool scene_hovered = false;

ConfigVar g_drawconsole("drawconsole", "0", CVAR_BOOL, "draw the console");

#include "Framework/MyImguiLib.h"
void GameEngineLocal::draw_any_imgui_interfaces()
{
	CPUSCOPESTART(imgui_draw);

	ImGui::PushStyleColor(ImGuiCol_WindowBg, color32_to_imvec4({51, 51, 51 }));
	ImGui::PushStyleColor(ImGuiCol_FrameBg, color32_to_imvec4({35, 35, 35 }));

	if (g_drawdebugmenu.get_bool())
		Debug_Interface::get()->draw();

#ifdef EDITOR_BUILD
	// draw tool interface if its active
	if (is_in_an_editor_state()) {
		dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, get_current_tool());
		get_current_tool()->draw_imgui_public();
		global_asset_browser.imgui_draw();
	}
#endif

	// draw after to enable docking
	if (g_drawconsole.get_bool())
		dbg_console.draw();

#ifdef EDITOR_BUILD
	// will only be true if in a tool state
	if (is_drawing_to_window_viewport() && eng->get_state()==Engine_State::Game) {

		uint32_t flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (scene_hovered)
			flags |=  ImGuiWindowFlags_NoMove;

		if (is_in_an_editor_state() && get_current_tool()->wants_scene_viewport_menu_bar())
			flags |= ImGuiWindowFlags_MenuBar;

		bool next_focus = false;
		//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		if (ImGui::Begin("Scene viewport",nullptr, flags)) {

			if (is_in_an_editor_state())
				get_current_tool()->hook_pre_scene_viewport_draw();

			auto size = ImGui::GetWindowSize();
			size.y -= 50;
			size.x -= 30;
			if (size.y < 0) size.y = 0;
			if (size.x < 0) size.x = 0;

			auto pos = ImGui::GetCursorPos();
			auto winpos = ImGui::GetWindowPos();
			ImGui::Image((ImTextureID)uint64_t(idraw->get_composite_output_texture_handle()), 
				ImVec2(size.x, size.y), /* magic numbers ;) */
				ImVec2(0,1),ImVec2(1,0));	// this is the scene draw texture
			auto sz = ImGui::GetItemRectSize();
			scene_hovered = ImGui::IsItemHovered();
			window_viewport_size = { size.x,size.y };

			// save off where the viewport is the GUI for mouse events
			get_gui()->set_viewport_size(size.x, size.y);
			get_gui()->set_viewport_ofs(pos.x + winpos.x, pos.y + winpos.y);

			bool focused_window = scene_hovered;
			next_focus = focused_window && ImGui::GetIO().MouseDown[1];

			// hook tool for drag and drop stuff
			if (is_in_an_editor_state())
				get_current_tool()->hook_scene_viewport_draw();
		}

		set_game_focused(next_focus);
		ImGui::End();
		//ImGui::PopStyleVar();
	}
	else 
#endif
	{
		// normal game path, scene view was already drawn the the window framebuffer
		get_gui()->set_viewport_size(g_window_w.get_integer(), g_window_h.get_integer());
		get_gui()->set_viewport_ofs(0,0);
	}

	if(g_drawimguidemo.get_bool())
		ImGui::ShowDemoWindow();

	ImGui::PopStyleColor(2);
}

void GameEngineLocal::get_draw_params(SceneDrawParamsEx& params, View_Setup& setup)
{
	params = SceneDrawParamsEx(time, frame_time);
	params.output_to_screen = !is_drawing_to_window_viewport();
	// so the width/height parameters are valid
	View_Setup vs_for_gui;
	auto viewport = get_game_viewport_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;


	if (state == Engine_State::Loading || state == Engine_State::Idle) {
		// draw loading ui etc.
		params.draw_world = false;
		setup = vs_for_gui;
		if (get_current_tool() != nullptr)
			params.is_editor = true;
		//idraw->scene_draw(params, vs_for_gui, get_gui());
	}
	else if (state == Engine_State::Game) {

#ifdef EDITOR_BUILD
		// draw general ui
		if (get_current_tool() != nullptr) {
			params.is_editor = true;	// draw to the id buffer for mouse picking
			auto vs = get_current_tool()->get_vs();

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
			params.output_to_screen = !is_drawing_to_window_viewport();
			View_Setup vs_for_gui;
			auto viewport = get_game_viewport_size();
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
				isound->set_listener_position(vs.origin, in[1]);
			}
		
		}
	}
}

// RH, reverse Z, infinite far plane perspective matrix

glm::mat4 MakeInfReversedZProjRH(float fovY_radians, float aspectWbyH, float zNear)
{
	float f = 1.0f / tan(fovY_radians / 2.0f);
	return glm::mat4(
		f / aspectWbyH, 0.0f, 0.0f, 0.0f,
		0.0f, f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, zNear, 0.0f);
}
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


extern void register_input_actions_for_game();
void GameEngineLocal::init(int argc, char** argv)
{
	this->argc = argc;
	this->argv = argv;

	sys_print(Info, "--------- Initializing Engine ---------\n");

	float start = GetTime();

	program_time_start = GetTime();

	// must come first
	ClassBase::init_class_reflection_system();

	level = nullptr;
	tick_interval = 1.0 / 60.0;
	state = Engine_State::Idle;
	is_hosting_game = false;
	//sv.reset( new Server );
	//cl.reset( new Client );

	init_sdl_window();

	Profiler::init();

	FileSys::init();
	g_assets.init();

	jobs::init();	// spawns worker threads

#ifdef EDITOR_BUILD
	AssetRegistrySystem::get().init();
#endif
	g_scriptMgr->init();

	g_inputSys.init();
	register_input_actions_for_game();

	g_physics.init();
	//network_init();
	// renderer init
	idraw->init();
	imaterials->init();
	g_fonts.init();
	gui_sys.reset(GuiSystemPublic::create_gui_system());
	isound->init();
	g_modelMgr.init();
	g_gameAnimationMgr.init();
	//cl->init();
	//sv->init();
	imgui_context = ImGui::CreateContext();
	DebugShapeCtx::get().init();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();
	auto path = FileSys::get_full_path_from_game_path("Inconsolata-Bold.ttf");
	ImGui::GetIO().Fonts->AddFontFromFileTTF(path.c_str(), 14.0);
	ImGui::GetIO().Fonts->Build();

	engine_fullscreen_gui = new GUIFullscreen();
	gui_log = new OnScreenLogGui();
	engine_fullscreen_gui->add_this(gui_log);
	engine_fullscreen_gui->recieve_events = false;
	gui_sys->add_gui_panel_to_root(engine_fullscreen_gui);

	Cmd_Manager::get()->set_set_unknown_variables(true);
	Cmd_Manager::get()->execute_file(Cmd_Execute_Mode::NOW, "vars.txt");
	
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

			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, cmd.c_str());
		}
	}
	Cmd_Manager::get()->set_set_unknown_variables(false);


	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());


	Cmd_Manager::get()->execute_file(Cmd_Execute_Mode::NOW, "init.txt");

	// not in editor and no queued map, load the entry point
	if (!is_in_an_editor_state() && get_state() != Engine_State::Loading) {
		open_level(g_entry_level.get_string());
	}

	TIMESTAMP("execute startup");
}


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

		if (!is_editor_level())
			g_inputSys.tick_users(dt);
		level->update_level();
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

		if (state != Engine_State::Game)
			break;
	}

	// call level update
	update(frame_time);

	g_gameAnimationMgr.update_animating();

	time += frame_time;

	frame_time = orig_ft;
	tick_interval = orig_ti;
}


// unloads all game state
void GameEngineLocal::stop_game()
{
	if (!map_spawned())
		return;

	const char* str = (level->get_source_asset()) ? level->get_source_asset()->get_name().c_str() : "<empty>";
	sys_print(Info,"-------- Clearing Map (%s) --------\n", str);

	ASSERT(level);

	idraw->on_level_end();

	level->close_level();
	level.reset();

	// clear any debug shapes
	DebugShapeCtx::get().fixed_update_start();
}

bool GameEngineLocal::game_thread_update()
{
	if (state == Engine_State::Game) {
		game_update_tick();
		if (state != Engine_State::Game)
			return false;	// goto next frame (to exit or change map)
	}

#ifdef EDITOR_BUILD
	if (is_in_an_editor_state()) {
		get_current_tool()->tick(frame_time);
	}
#endif

	isound->tick(frame_time);


	// draw imgui here
	// draw ui

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
	//printf("abc\n");
	auto out = (GameUpdateOuput*)user;
	out->drawOut = eng_local.game_thread_update();
	eng_local.get_draw_params(out->paramsOut, out->vsOut);
	//return;
	// update particles, doesnt draw, only builds meshes FIXME
	ParticleMgr::get().draw(out->vsOut);

	// paint the UI
	eng_local.get_gui()->paint();
}


void GameEngineLocal::loop()
{
	auto frame_start = [&]() -> bool
	{
		ZoneScopedN("frame_start");

		// reset cursor if in relative mode
		if (is_game_focused()) {
			SDL_WarpMouseInWindow(window, saved_mouse_x, saved_mouse_y);
		}

		// update input
		memset(inp.keychanges, 0, sizeof inp.keychanges);
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);


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

					//SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());
				}
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
				key_event(event);
				break;
			}

			g_inputSys.handle_event(event);

			if (!is_game_focused()) {
				if ((event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) && ImGui::GetIO().WantCaptureKeyboard)
					continue;
				if (!scene_hovered && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse)
					continue;
			}
			get_gui()->handle_event(event);
		}
		get_gui()->post_handle_events();

		get_gui()->think();

		Cmd_Manager::get()->execute_buffer();

		// update state
		switch (state)
		{
		case Engine_State::Idle:
			if (map_spawned()) {	// map is spawned, unload it
				stop_game();
				return true;			// goto next frame
			}

			SDL_Delay(5);	// assuming this is a menu/tool state, delay a bit to save CPU
			break;
		case Engine_State::Loading:
			execute_map_change();
			return true; // goto next frame
			break;
		case Engine_State::Game:	// handled in game_thread_update()
			break;
		};

		return false;
	};

	auto do_overlapped_update = [&](bool& shouldDrawNext, SceneDrawParamsEx& drawparamsNext, View_Setup& setupNext)
	{
		ZoneScopedN("OverlappedUpdate");
		CPUSCOPESTART(OverlappedUpdate);


		BooleanScope scope(b_is_in_overlapped_period);

		GameUpdateOuput out;
		jobs::Counter* gameupdatecounter{};
		jobs::add_job(game_update_job,uintptr_t(&out), gameupdatecounter);

		if (!shouldDrawNext) {
			drawparamsNext.draw_world = drawparamsNext.draw_ui = false;
		}
		idraw->scene_draw(drawparamsNext, setupNext);
		jobs::wait_and_free_counter(gameupdatecounter);// wait for game update to finish while render is on this thread

		shouldDrawNext = out.drawOut;
		drawparamsNext = out.paramsOut;
		setupNext = out.vsOut;
	};

	// This happens on main thread
	// I could double buffer draw data so ImGui can update on game thread and render simultaneously
	auto imgui_render = [&]()
	{
		ZoneScopedN("ImguiDraw");

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	};
	auto imgui_update = [&]()
	{
		ZoneScopedN("ImGuiUpdate");

		// draw imgui interfaces
		// if a tool is active, game screen gets drawn to an imgui viewport
		ImGui_ImplSDL2_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();
#ifdef EDITOR_BUILD
		if (get_current_tool())
			get_current_tool()->hook_imgui_newframe();
#endif
		draw_any_imgui_interfaces();
	};

	auto do_sync_update = [&]()
	{
		ZoneScopedN("SyncUpdate");

		DebugShapeCtx::get().update(frame_time);

		g_assets.tick_asyncs();	// tick async loaded assets, this will call the async load_map callback
#ifdef EDITOR_BUILD
	// update hot reloading
		AssetRegistrySystem::get().update();
#endif
		if (get_level())
			get_level()->sync_level_render_data();

		gui_sys->sync_to_renderer();

		g_physics.sync_render_data();
		
		idraw->sync_update();

		// sync any rendering objects
		// sync meshbuilders
		// sync UI
		// sync materials

	};
	auto wait_for_swap = [&]()
	{
		ZoneScopedN("SwapWindow");
		TracyGpuZone("SwapWindow");
		CPUSCOPESTART(SwapWindow);

		SDL_GL_SwapWindow(window);
	};


	double last = GetTime() - 0.1;
	// these are from the last game frame
	SceneDrawParamsEx drawparamsNext(0, 0);
	View_Setup setupNext;
	bool shouldDrawNext = true;


	for (;;)
	{
		// update time
		const double now = GetTime();
		double dt = now - last;
		last = now;
		if (dt > 0.1)
			dt = 0.1;
		frame_time = dt;

		// update input, console cmd buffer
		const bool should_skip = frame_start();
		if (should_skip) {
			// hack, do a sync update here to refresh assets etc
			do_sync_update();
			continue;
		}

		// overlapped update (game+render)
		do_overlapped_update(shouldDrawNext, drawparamsNext, setupNext);

		// sync period
		imgui_update();	// fixme
		imgui_render();
		do_sync_update();
		wait_for_swap();	// wait for swap last

		{
			static int counter = 0;
			counter++;
			if (counter > 5) {
				counter = 0;
			}
			TracyGpuCollect;
		}
		FrameMark;

		Profiler::end_frame_tick(frame_time);
	}
}


int debug_console_text_callback(ImGuiInputTextCallbackData* data)
{
	Debug_Console* console = &dbg_console;
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
				bool has_color = false;
				if (!lines[i].empty() && lines[i][0] == '>') { color = { 136,23,152 }; has_color = true; }
				if (has_color)
					ImGui::PushStyleColor(ImGuiCol_Text, color.to_uint());
				ImGui::TextUnformatted(lines[i].c_str());
				if (has_color)
					ImGui::PopStyleColor();
			}
			if (scroll_to_bottom || (auto_scroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()))
				ImGui::SetScrollHereY(1.0f);
			scroll_to_bottom = false;

			ImGui::PopStyleVar();
		}
		ImGui::EndChild();
		ImGui::Separator();
	}
	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
		ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackCompletion;
	if (set_keyboard_focus) {
		ImGui::SetKeyboardFocusHere();
		set_keyboard_focus = false;
	}

	if (ImGui::InputText("Input", input_buffer, IM_ARRAYSIZE(input_buffer), input_text_flags, debug_console_text_callback, this))
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

	// Auto-focus on window apparition
	ImGui::SetItemDefaultFocus();
	if (reclaim_focus)
		ImGui::SetKeyboardFocusHere(-1); // Auto focus previous widget

	ImGui::End();
}
void Debug_Console::print_args(const char* fmt, va_list args)
{
	if (lines.size() > 1000)
		lines.clear();


	char buf[1024];
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	lines.push_back(buf);
}

void Debug_Console::print(const char* fmt, ...)
{
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
	buf[IM_ARRAYSIZE(buf) - 1] = 0;
	va_end(args);
	lines.push_back(buf);
}

ConfigVar g_editor_cfg_folder("g_editor_cfg_folder", "Cfg", CVAR_DEV, "what folder to save .ini and other editor cfg to");

DECLARE_ENGINE_CMD(dump_imgui_ini)
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
DECLARE_ENGINE_CMD(load_imgui_ini)
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

