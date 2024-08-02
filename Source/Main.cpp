#include <SDL2/SDL.h>

#include "glad/glad.h"
#include "stb_image.h"

#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

#include "GlmInclude.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "Render/Shader.h"
#include "Render/Texture.h"
#include "Framework/MathLib.h"
#include "Render/Model.h"

#include "Level.h"
#include "Physics.h"
#include "Net.h"
#include "GameEngineLocal.h"
#include "Types.h"
#include "Client.h"
#include "Server.h"
#include "Game/Player.h"
#include "Framework/Config.h"
#include "Render/DrawPublic.h"
#include "Game/Entity.h"
#include "Physics/Physics2.h"
#include "Level.h"

#include "Assets/AssetBrowser.h"
#include "Animation/AnimationTreePublic.h"
#include "Animation/Editor/AnimationGraphEditorPublic.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"


#include "Framework/ClassBase.h"

#include "Render/MaterialPublic.h"

MeshBuilder phys_debug;

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
		sys_print("%s | %s (%d)\n", error_name, file, line);

		error_code = glGetError();
	}
	return has_error;
}

class Debug_Console
{
public:
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
	char input_buffer[256];
};
static Debug_Console dbg_console;

char* string_format(const char* fmt, ...) {
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
	sys_print("Quiting...\n");
	eng_local.cleanup();
	exit(0);
}

#include "Framework/ConsolePrint.h"


void sys_print(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	int len = strlen(fmt);
	bool print_end = true;
	if (len >= 3 && fmt[0] == '!' && fmt[1] == '!' && fmt[2] == '!') {
		printf("\033[91m");
	}
	else if (len >= 3 && fmt[0] == '?' && fmt[1] == '?' && fmt[2] == '?') {
		printf("\033[33m");
	}
	else if (len >= 3 && fmt[0] == '*' && fmt[1] == '*' && fmt[2] == '*') {
		printf("\033[94m");
	}
	else if (len >= 3 && fmt[0] == '`' && fmt[1] == '`' && fmt[2] == '`') {
		printf("\033[32m");
	}
	else if (len >= 1 && fmt[0] == '>') {
		printf("\033[35m");
	}
	else
		print_end = false;

	vprintf(fmt, args);
	dbg_console.print_args(fmt, args);
	va_end(args);

	if (print_end)
		printf("\033[0m");
}

class GUI_RootControl : public UIControl
{
public:
	// Inherited from UIControl
	void ui_paint() override {
		if (eng->get_current_tool() != nullptr)
			eng->get_current_tool()->ui_paint();
	}
	bool handle_event(const SDL_Event& event) override {
		if (eng->get_current_tool() != nullptr)
			return eng->get_current_tool()->handle_event(event);

		return false;
	}

public:
	void init();
	void set_ui_rect();

	void set_screen_rect_base(Rect2d rect) {
		this->size = rect;
	}

	// game panel
	// tool panel
	// log panel
};

void sys_vprint(const char* fmt, va_list args)
{
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
	eng_local.cleanup();
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

static void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}


static void view_angle_update(glm::vec3& view_angles)
{
	int x, y;
	SDL_GetRelativeMouseState(&x, &y);
	float x_off = g_mousesens.get_float() * x;
	float y_off = g_mousesens.get_float() * y;

	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);

}

template<typename T>
T* checked_cast(ClassBase* c) {
	return c ? c->cast_to<T>() : nullptr;
}

void GameEngineLocal::make_move()
{
	Player* p = checked_cast<Player>(get_local_player());

	if (!p)
		return;

	Move_Command command;
	command.view_angles = p->view_angles;
	command.tick = tick;

	if (g_fakemovedebug.get_integer() != 0)
		command.lateral_move = std::fmod(GetTime(), 2.f) > 1.f ? -1.f : 1.f;
	if (g_fakemovedebug.get_integer() == 2)
		command.button_mask |= BUTTON_JUMP;

	if (!is_game_focused()) {
		p->set_input_command(command);
		if(cl->get_state()>=CS_CONNECTED) 
			cl->get_command(cl->OutSequence()) = command;
		return;
	}

	if (!p->has_flag(PlayerFlags::FrozenView))
		view_angle_update(command.view_angles);


	int forwards_key = SDL_SCANCODE_W;
	int back_key = SDL_SCANCODE_S;

	if (inp.keys[forwards_key])
		command.forward_move += 1.f;
	if (inp.keys[back_key])
		command.forward_move -= 1.f;
	if (inp.keys[SDL_SCANCODE_A])
		command.lateral_move += 1.f;
	if (inp.keys[SDL_SCANCODE_D])
		command.lateral_move -= 1.f;
	if (inp.keys[SDL_SCANCODE_Z])
		command.up_move += 1.f;
	if (inp.keys[SDL_SCANCODE_X])
		command.up_move -= 1.f;
	if (inp.keys[SDL_SCANCODE_SPACE])
		command.button_mask |= BUTTON_JUMP;
	if (inp.keys[SDL_SCANCODE_LSHIFT])
		command.button_mask |= BUTTON_DUCK;
	if (inp.mousekeys & (1<<1))
		command.button_mask |= BUTTON_FIRE1;
	if (inp.keys[SDL_SCANCODE_E])
		command.button_mask |= BUTTON_RELOAD;
	if (inp.keychanges[SDL_SCANCODE_LEFTBRACKET])
		command.button_mask |= BUTTON_ITEM_PREV;
	if (inp.keychanges[SDL_SCANCODE_RIGHTBRACKET])
		command.button_mask |= BUTTON_ITEM_NEXT;
	// quantize and unquantize for local prediction
	command.forward_move	= Move_Command::unquantize(Move_Command::quantize(command.forward_move));
	command.lateral_move	= Move_Command::unquantize(Move_Command::quantize(command.lateral_move));
	command.up_move			= Move_Command::unquantize(Move_Command::quantize(command.up_move));
	
	// FIXME:
	p->set_input_command(command);
	if(cl->get_state()>=CS_CONNECTED)
		cl->get_command(cl->OutSequence()) = command;
}

void GameEngineLocal::init_sdl_window()
{
	ASSERT(!window);

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		sys_print("!!! init sdl failed: %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		g_window_w.get_integer(), g_window_h.get_integer(), SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		sys_print("!!! create sdl window failed: %s\n", SDL_GetError());
		exit(-1);
	}

	gl_context = SDL_GL_CreateContext(window);
	sys_print("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	sys_print("``` Vendor: %s\n", glGetString(GL_VENDOR));
	sys_print("``` Renderer: %s\n", glGetString(GL_RENDERER));
	sys_print("``` Version: %s\n\n", glGetString(GL_VERSION));

	SDL_GL_SetSwapInterval(0);
}

glm::mat4 User_Camera::get_view_matrix() const {
	return glm::lookAt(position, position + front, up);
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
#define PRINTVEC(name,v) printf(name": %f %f %f\n", v.x,v.y,v.z);
void User_Camera::update_from_input(const bool keys[], int mouse_dx, int mouse_dy, glm::mat4 invproj)
{
	int xpos, ypos;
	xpos = mouse_dx;
	ypos = mouse_dy;

	float x_off = xpos;
	float y_off = ypos;
	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;


	bool pan_in_orbit_model = keys[SDL_SCANCODE_LSHIFT];


	// position = cam position
	// orbit target = what you rotate around = constant
	// front = what dir camera is pointing
	// dist = dot(front, orbit_target-position)
	if (orbit_mode)
	{
		float lookatpointdist = dot(position - orbit_target, front);	// project target onto front
		glm::vec3 lookatpoint = position + front * lookatpointdist;
		glm::vec3 tolookat = lookatpoint - position;

		glm::vec3 otol = tolookat - orbit_target;
		glm::vec3 toorbit = orbit_target - position;
		float toorbitdist = length(toorbit);

		glm::vec3 side = glm::normalize(glm::cross(up, front));
		glm::vec3 up = cross(side, front);
		if (pan_in_orbit_model) {

			glm::vec4 offsetdeltafrompanning = invproj * glm::vec4(mouse_dx, mouse_dy, lookatpointdist, 1.f);
			offsetdeltafrompanning /= offsetdeltafrompanning.w;
			otol += offsetdeltafrompanning.x * side + offsetdeltafrompanning.y * up;
		}
		else {
			yaw += x_off;
			pitch -= y_off;
			//glm::clamp(yaw, 0.f, TWOPI);
			//glm::clamp(pitch, -HALFPI + 0.01f, HALFPI - 0.01f);
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
		}

		vec3 newtoorbit = AnglesToVector(pitch, yaw) * toorbitdist;
		vec3 newposition = orbit_target - newtoorbit;

		vec3 newlookatpoint = newtoorbit - otol;
		float len_ = glm::length(newlookatpoint - newposition);
		
		PRINTVEC("pos          ", newposition);
		printf(  "lapdist      %f\n", lookatpointdist);
		PRINTVEC("tolookat     ", tolookat);
		PRINTVEC("look-at-point", newlookatpoint);
		PRINTVEC("to-orbit     ", newtoorbit);
		PRINTVEC("otol         ", otol);
		if (abs(len_) > 0.000001f) {
			vec3 oldfront = front;
			front = (newlookatpoint - newposition) / len_;
		PRINTVEC("frontdelta   ", (oldfront - front));
		}
		printf("----------------------------\n");
		position = newposition;
	}
	else {
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
#include "Framework/Files.h"


struct Sound
{
	char* buffer;
	int length;
};


void GameEngineLocal::connect_to(string address)
{
	ASSERT(0);
	///travel_to_engine_state(Engine_State::Loading, "connecting to another server");

	sys_print("Connecting to server %s\n", address.c_str());
	cl->connect(address);
}

#ifdef EDITDOC
#include "EditorDocPublic.h"

DECLARE_ENGINE_CMD(close_ed)
{
	eng_local.change_editor_state(nullptr);
}


DECLARE_ENGINE_CMD(start_ed)
{
	static const char* usage_str = "Usage: starteditor [Map,AnimGraph,Model,<asset name>] <file>\n";
	if (args.size() != 2 && args.size()!=3) {
		sys_print(usage_str);
		return;
	}

	std::string edit_type = args.at(1);
	const AssetMetadata* metadata = AssetRegistrySystem::get().find_type(edit_type.c_str());
	if (metadata && metadata->tool_to_edit_me()) {
		const char* file_to_open = "";	// make a new map
		if (args.size() == 3)
			file_to_open = args.at(2);
		eng_local.change_editor_state(metadata->tool_to_edit_me(), file_to_open);
	}
	else {
		sys_print("unknown editor\n");
		sys_print(usage_str);
	}
}


static void enable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
static void disable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable);
}

#define ASSERT_ENUM_STRING( enumstr, index )		( 1 / (int)!( (int)enumstr - index ) ) ? #enumstr : ""

void GameEngineLocal::change_editor_state(IEditorTool* next_tool, const char* file)
{
	sys_print("--------- Change Editor State ---------\n");
	if (active_tool != next_tool && active_tool != nullptr) {
		// this should call close internally
		get_current_tool()->set_focus_state(editor_focus_state::Closed);
	}
	active_tool = next_tool;
	if (active_tool != nullptr) {
		enable_imgui_docking();
		global_asset_browser.init();
		bool could_open = get_current_tool()->open_and_set_focus(file, (get_state() == Engine_State::Game) ? editor_focus_state::Background : editor_focus_state::Focused);
		if (!could_open) {
			active_tool = nullptr;
		}
	}

	if (!active_tool )
		disable_imgui_docking();
}



#endif


enum class NetState
{
	Idle,					// => Idle
	WaitingForConnect,		// => Idle
	Connecting,				// => Idle
	Loading,				// => Loading
	WatingForInitialState,	// => Idle
	Game,					// => Game
};

DECLARE_ENGINE_CMD(quit)
{
	Quit();
}

DECLARE_ENGINE_CMD(bind)
{
	if (args.size() < 2) return;
	int scancode = SDL_GetScancodeFromName(args.at(1));
	if (scancode == SDL_SCANCODE_UNKNOWN) return;
	if (args.size() < 3)
		eng_local.bind_key(scancode, "");
	else
		eng_local.bind_key(scancode, args.at(2));
}

DECLARE_ENGINE_CMD_CAT("cl.",force_update)
{
	eng->get_client()->ForceFullUpdate();
}

DECLARE_ENGINE_CMD(connect)
{
	if (args.size() < 2) {
		sys_print("usage connect <address>");
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
	if(eng->get_client()->get_state() != CS_DISCONNECTED)
		eng->get_client()->Reconnect();
}

DECLARE_ENGINE_CMD(map)
{
	if (args.size() < 2) {
		sys_print("usage map <map name>");
		return;
	}
	eng->open_level(args.at(1));
}
DECLARE_ENGINE_CMD(exec)
{
	if (args.size() < 2) {
		sys_print("usage map <map name>");
		return;
	}

	Cmd_Manager::get()->execute_file(Cmd_Execute_Mode::NOW, args.at(1));
}

DECLARE_ENGINE_CMD(net_stat)
{
	float mintime = INFINITY;
	float maxtime = -INFINITY;
	int maxbytes = -5000;
	int totalbytes = 0;
	for (int i = 0; i < 64; i++) {
		auto& entry = eng->get_client()->server.incoming[i];
		maxbytes = glm::max(maxbytes, entry.bytes);
		totalbytes += entry.bytes;
		mintime = glm::min(mintime, entry.time);
		maxtime = glm::max(maxtime, entry.time);
	}

	sys_print("Client Network Stats:\n");
	sys_print("%--15s %f\n", "Rtt", eng->get_client()->server.rtt);
	sys_print("%--15s %f\n", "Interval", maxtime - mintime);
	sys_print("%--15s %d\n", "Biggest packet", maxbytes);
	sys_print("%--15s %f\n", "Kbits/s", 8.f*(totalbytes / (maxtime-mintime))/1000.f);
	sys_print("%--15s %f\n", "Bytes/Packet", totalbytes / 64.0);
}

#ifdef _DEBUG
DECLARE_ENGINE_CMD(print_ents)
{

	sys_print("%--15s %--15s %--15s %--15s\n", "index", "class", "posx", "posz", "has_model");
	auto level = eng->get_level();
	for (auto eptr : level->all_world_ents) {
		Entity& e = *eptr;
		sys_print("%-15llu %-15s %-15f %-15f\n", e.self_id.handle, e.get_type().classname, e.position.x, e.position.z);
	}
}
#endif

DECLARE_ENGINE_CMD(print_vars)
{
	//if (args.size() == 1)
	//	Var_Manager::get()->print_vars(nullptr);
	//else
	//	Var_Manager::get()->print_vars(args.at(1));
}

DECLARE_ENGINE_CMD(reload_shaders)
{
	idraw->reload_shaders();
}


typedef int sound_handle;
enum Sound_Channels
{
	UI_LAYER,


};

class Audio_System
{
public:
	void init();
	sound_handle start_sound(const char* sound, bool looping);
	void set_sound_position(sound_handle handle, vec3 position);
	void free_sound(sound_handle* handle);

	

};

void Audio_System::init()
{

}

void init_audio()
{
}

extern void benchmark_run();
extern void benchmark_gltf();
extern void at_test();
#include "Framework/ExpressionLang.h"

#include "Game/EntityTypes.h"
#include "Game/Schema.h"

#include "Render/MaterialLocal.h"
#include "Framework/PropHashTable.h"
int main(int argc, char** argv)
{
	eng_local.argc = argc;
	eng_local.argv = argv;
	eng_local.init();


	eng_local.loop();
	eng_local.cleanup();
	
	return 0;
}

ConfigVar g_mousesens("g_mousesens", "0.005", CVAR_FLOAT, 0.0, 1.0);
ConfigVar g_fov("fov", "70.0", CVAR_FLOAT, 55.0, 110.0);
ConfigVar g_thirdperson("thirdperson", "70.0", CVAR_BOOL);
ConfigVar g_fakemovedebug("fakemovedebug", "0", CVAR_INTEGER, 0,2);
ConfigVar g_drawimguidemo("g_drawimguidemo", "1", CVAR_BOOL);
ConfigVar g_debug_skeletons("g_debug_skeletons", "1", CVAR_BOOL);
ConfigVar g_draw_grid("g_draw_grid", "1", CVAR_BOOL);
ConfigVar g_grid_size("g_grid_size", "1", CVAR_FLOAT, 0.01,10);

ConfigVar g_drawdebugmenu("g_drawdebugmenu","1",CVAR_BOOL);

ConfigVar g_window_w("vid.width","1200",CVAR_INTEGER,1,4000);
ConfigVar g_window_h("vid.height", "800", CVAR_INTEGER, 1, 4000);
ConfigVar g_window_fullscreen("vid.fullscreen", "0",CVAR_BOOL);
ConfigVar g_host_port("net.hostport","47000",CVAR_INTEGER|CVAR_READONLY,0,UINT16_MAX);
ConfigVar g_dontsimphysics("stop_physics", "0", CVAR_BOOL | CVAR_DEV);


DECLARE_ENGINE_CMD(spawn_npc)
{
	if (!eng->get_level())
		return;
	
	auto p = checked_cast<Player>(eng->get_local_player());
	if (!p) {
		sys_print("no player\n");
		return;
	}
	auto va = p->view_angles;
	vec3 front = AnglesToVector(va.x, va.y);
	vec3 pos = p->calc_eye_position();

	auto npc = eng->spawn_entity_class<NPC>();
	npc->position = pos;
}

void GameEngineLocal::open_level(string nextname)
{
	// level will get loaded in next ::loop()
	queued_mapname = nextname;
	state = Engine_State::Loading;
}

void GameEngineLocal::leave_level()
{
	// disconnect clients etc.
	// current map gets unloaded in next ::loop()
	state = Engine_State::Idle;
}

// Animation debugger is seperate

// In 1 world at a time: editor world, game world, animation editor world, etc.
// when switching maps, you implicity leave any editor state behind
// can have 0 or 1 map loaded
// gui is always loaded, insert explicit branch logic for editor vs game for the GUI
// use world ticks if any map is loaded

// execute map change then handles all logic for editor/game switching

// switching to another editor or closing it goes through the same queued path
// when a map is loaded, all the subsystems are ticked, like physics, sound, etc.


void GameEngineLocal::execute_map_change()
{
	sys_print("-------- Map Change: %s --------\n", queued_mapname.c_str());

	// free current map
	stop_game();

	// try loading map
	level = LevelSerialization::unserialize_level(queued_mapname, false/* not for editor*/);
	if (!level) {
		sys_print("!!! couldn't load map !!!\n");
		state = Engine_State::Idle;
		return;
	}
	level->init_entities_post_load();

	tick = 0;
	time = 0.0;

	if (is_host())
		spawn_starting_players(true);

	idraw->on_level_start();

	sys_print("*** changed state to Engine_State::Game\n");

	// fixme, for server set state to game, but clients will sit in a wait loop till they recieve their first
	// snapshot before continuing
	state = Engine_State::Game;
}


void GameEngineLocal::spawn_starting_players(bool initial)
{
	// fixme for multiplayer, spawn in clients that are connected to server ex on restart
	login_new_player(0);	// player 0
}

void GameEngineLocal::set_tick_rate(float tick_rate)
{
	if (state == Engine_State::Game) {
		sys_print("Can't change tick rate while running\n");
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

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse) {
		set_game_focused(false);
		return;
	}
	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

	if (event.type == SDL_KEYDOWN) {
		int scancode = event.key.keysym.scancode;
		inp.keys[scancode] = true;
		inp.keychanges[scancode] = true;

		if (binds[scancode]) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, binds[scancode]->c_str());
		}
	}
	else if (event.type == SDL_KEYUP) {
		inp.keys[event.key.keysym.scancode] = false;
	}
	// when drawing to windowed viewport, handle game_focused during drawing
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (event.button.button == 3 && !is_drawing_to_window_viewport()) {
			set_game_focused(true);
		}
		inp.mousekeys |= (1<<event.button.button);
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		if (event.button.button == 3 && !is_drawing_to_window_viewport()) {
			set_game_focused(false);
		}
		inp.mousekeys &= ~(1 << event.button.button);
	}
}

void GameEngineLocal::bind_key(int key, string command)
{
	ASSERT(key >= 0 && key < SDL_NUM_SCANCODES);
	if (!binds[key])
		binds[key] = new string;
	*binds[key] = std::move(command);
}

void GameEngineLocal::cleanup()
{
	if (get_current_tool())
		get_current_tool()->set_focus_state(editor_focus_state::Closed);

	// could get fatal error before initializing this stuff
	if (gl_context && window) {
		NetworkQuit();
		SDL_GL_DeleteContext(gl_context);
		SDL_DestroyWindow(window);
	}

	for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
		delete binds[i];
	}
}

static int bloom_layer = 0;
extern float wsheight;
extern float wsradius;
extern float wsstartheight;
extern float wsstartradius;
extern vec3 wswind_dir;
extern float speed;


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

ConfigVar g_slomo("slomo", "1.0", CVAR_FLOAT | CVAR_DEV, 0.0001, 5.0);


bool GameEngineLocal::is_drawing_to_window_viewport() const
{
	return is_in_an_editor_state();
}

glm::ivec2 GameEngineLocal::get_game_viewport_size() const
{
	if (is_drawing_to_window_viewport())
		return window_viewport_size;
	else
		return { g_window_w.get_integer(), g_window_h.get_integer() };
}

static bool window_hovered = true;
static bool window_focused = true;
static bool scene_hovered = false;
static bool scene_focused = false;


void f345()
{
	ImGui::Text("window_hovered: %d",int(window_hovered));
	ImGui::Text("window_focused: %d", int(window_focused));
	ImGui::Text("scene_hovered: %d", int(scene_hovered));
	ImGui::Text("scene_focused: %d", int(scene_focused));
	ImGui::Text("eng->game_focused: %d", int(eng_local.is_game_focused()));
}
AddToDebugMenu asdf45("func", f345);
#include "Framework/MyImguiLib.h"
void GameEngineLocal::draw_any_imgui_interfaces()
{
	CPUSCOPESTART(imgui_draw);

	if (g_drawdebugmenu.get_bool())
		Debug_Interface::get()->draw();

	// draw tool interface if its active
	if (is_in_an_editor_state()) {
		dock_over_viewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode, get_current_tool());
		get_current_tool()->imgui_draw();
		global_asset_browser.imgui_draw();
	}

	// will only be true if in a tool state
	if (is_drawing_to_window_viewport()) {

		uint32_t flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (scene_hovered)
			flags |=  ImGuiWindowFlags_NoMove;
		bool next_focus = false;
		if (ImGui::Begin("Scene viewport",nullptr, flags)) {

			auto size = ImGui::GetWindowSize();
			auto pos = ImGui::GetCursorPos();
			auto winpos = ImGui::GetWindowPos();
			ImGui::Image((ImTextureID)idraw->get_composite_output_texture_handle(), ImVec2(size.x, size.y), ImVec2(0,1),ImVec2(1,0));	// this is the scene draw texture
			scene_hovered = ImGui::IsItemHovered();
			window_viewport_size = { size.x,size.y };
			window_hovered = ImGui::IsWindowHovered();
			window_focused = ImGui::IsWindowFocused();
			scene_focused = ImGui::IsItemFocused();

			// save off where the viewport is the GUI for mouse events
			Rect2d rect;
			rect.x = pos.x+winpos.x;
			rect.y = pos.y+winpos.y;
			rect.w = size.x;
			rect.h = size.y;
			gui_root->set_screen_rect_base(rect);

			bool focused_window = scene_hovered;
			next_focus = focused_window && ImGui::GetIO().MouseDown[1];

			// hook tool for drag and drop stuff
			if (is_in_an_editor_state())
				get_current_tool()->hook_scene_viewport_draw();
		}

		set_game_focused(next_focus);

		ImGui::End();
	}
	else {
		// normal game path, scene view was already drawn the the window framebuffer

		Rect2d rect;
		rect.x = 0;
		rect.y = 0;
		rect.w = g_window_w.get_integer();
		rect.h = g_window_h.get_integer();

		gui_root->set_screen_rect_base(rect);
	}

	if(g_drawimguidemo.get_bool())
		ImGui::ShowDemoWindow();
}

bool GameEngineLocal::game_draw_screen()
{
	SceneDrawParamsEx params(time,frame_time);
	params.output_to_screen = !is_drawing_to_window_viewport();
	View_Setup vs_for_gui;
	auto viewport = get_game_viewport_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;

	
	if (get_local_player() == nullptr) {
		params.draw_world = false;
		params.draw_ui = true;
		idraw->scene_draw(params, vs_for_gui, get_gui(), nullptr);	// not spawned, so just update the UI
		return true;
	}

	Player* p = checked_cast<Player>(get_local_player());
	ASSERT(p)
	
	glm::vec3 position(0,0,0);
	glm::vec3 angles(0, 0, 0);
	float fov = g_fov.get_float();
	p->get_view(position, angles, fov);
	glm::vec3 front = AnglesToVector(angles.x, angles.y);

	View_Setup vs = View_Setup(position, front, glm::radians(fov), 0.01, 100.0, viewport.x, viewport.y);

	idraw->scene_draw(params,vs, get_gui(), nullptr);

	return true;
}

void GameEngineLocal::draw_screen()
{
	GPUFUNCTIONSTART;

	SDL_SetWindowSize(window, g_window_w.get_integer(), g_window_h.get_integer());

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	SceneDrawParamsEx params(time, frame_time);
	params.output_to_screen = !is_drawing_to_window_viewport();
	// so the width/height parameters are valid
	View_Setup vs_for_gui;
	auto viewport = get_game_viewport_size();
	vs_for_gui.width = viewport.x;
	vs_for_gui.height = viewport.y;

	if (state == Engine_State::Idle) {
		// draw general ui
		if (get_current_tool() != nullptr) {
			params.is_editor = true;	// draw to the id buffer for mouse picking
			idraw->scene_draw(params, get_current_tool()->get_vs(), get_gui(), get_current_tool());
		}
		else {
			params.draw_world = false;	// no world to draw
			idraw->scene_draw(params, vs_for_gui, get_gui(), nullptr);
		}
	}
	else if (state == Engine_State::Loading) {
		// draw loading ui etc.
		params.draw_world = false;
		idraw->scene_draw(params, vs_for_gui, get_gui(), nullptr);
	}
	else if (state == Engine_State::Game) {
		bool good = game_draw_screen();

		if (!good) {
			glClearColor(1.0, 1.0, 1.0, 1.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		}
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	// draw imgui interfaces
	// if a tool is active, game screen gets drawn to an imgui viewport
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
	if (get_current_tool())
		get_current_tool()->hook_imgui_newframe();

	draw_any_imgui_interfaces();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	{
		GPUSCOPESTART("SDL_GL_SwapWindow");
		SDL_GL_SwapWindow(window);
	}
}


extern IEditorTool* g_model_editor;


DECLARE_ENGINE_CMD(reload_mats)
{
	ASSERT(0);
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
View_Setup::View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height)
	: origin(origin),front(front),fov(fov),near(near),far(far),width(width),height(height)
{
	view = glm::lookAt(origin, origin + front, glm::vec3(0, 1.f, 0));
	//proj = glm::perspective(fov, width / (float)height, near, far);

	const float aspectRatio = width / (float)height;
	proj = MakeInfReversedZProjRH(fov, aspectRatio, near);

	viewproj = proj * view;
}

#define TIMESTAMP(x) sys_print("```%s in %f\n",x,(float)GetTime()-start); start = GetTime();

GameEngineLocal::GameEngineLocal()
{
	memset(binds, 0, sizeof binds);
}

extern ModelMan mods;

ImNodesContext* ImNodesCreateContext()
{
	return nullptr;
}

void GameEngineLocal::init()
{
	sys_print("--------- Initializing Engine ---------\n");

	float first_start = GetTime();
	float start = GetTime();

	program_time_start = GetTime();

	// initialize class reflection/creation system
	ClassBase::init();

	memset(binds, 0, sizeof(binds));
	level = nullptr;
	tick_interval = 1.0 / DEFAULT_UPDATE_RATE;
	state = Engine_State::Idle;
	is_hosting_game = false;
	sv.reset( new Server );
	cl.reset( new Client );
	
	dbg_console.init();

	// engine initilization
	init_sdl_window();

	Profiler::init();
	FileSys::init();

	g_physics->init();
	init_audio();
	network_init();
	idraw->init();
	imaterials->init();

	gui_root.reset(new GUI_RootControl );

	anim_tree_man->init();
	mods.init();
	iparticle->init();
	cl->init();
	sv->init();
	imgui_context = ImGui::CreateContext();
	TIMESTAMP("init everything");

	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();
	ImGui::GetIO().Fonts->AddFontFromFileTTF("./Data/Inconsolata-Bold.ttf", 14.0);
	ImGui::GetIO().Fonts->Build();

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
			SDL_SetWindowTitle(window, "CsRemake - VISUAL STUDIO\n");
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

	TIMESTAMP("execute startup");
}


/*
make input (both listen and clients)
client send input to server (listens do not)

listens read packets from clients
listens update game (sim local character with frame's input)
listen build a snapshot frame for sending to clients

listens dont have packets to "read" from server, stuff like sounds/particles are just branched when they are created on sim frames
listens dont run prediction for local player
*/


void GameEngineLocal::game_update_tick()
{
	CPUFUNCTIONSTART;

	Debug::on_fixed_update_start();

	// create input
	make_move();
	//if (!is_host())
	//	cl->SendMovesAndMessages();

	time = tick * tick_interval;
	//build_physics_world(0.f);
	
	// update entities
	for (auto ent : level->all_world_ents) {
		ent->update_entity_and_components();
	}

	// update the physics
	g_physics->simulate_and_fetch(tick_interval);

	// call present() on any entities that need it (get physics results)

	tick += 1;

	//if (is_host()) {
		// build physics world now as ReadPackets() executes player commands
		//sv->ReadPackets();
		//sv->make_snapshot();
		//for (int i = 0; i < sv->clients.size(); i++)
		//	sv->clients[i].Update();
	//}

	//if (!is_host()) {
	//	cl->ReadPackets();
	//	cl->run_prediction();
	//	tick += 1;
	//}

	// fixme:

}

void perf_tracker()
{
	static std::vector<float> time(200,0.f);
	static int index = 0;
	time.at(index) = eng->get_frame_time();
	index = index + 1;
	index %= 200;

	float avg_ft = 0.f;
	for (int i = 0; i < time.size(); i++)avg_ft += time.at(i);
	avg_ft /= 200.f;

	ImGui::Text("Frametime: %f", avg_ft*1000.f);

}


// unloads all game state
void GameEngineLocal::stop_game()
{
	if (!map_spawned())
		return;

	sys_print("-------- Clearing Map --------\n");

	ASSERT(level);

	idraw->on_level_end();
	
	delete level;
	level = nullptr;

	// clear any debug shapes
	Debug::on_fixed_update_start();
}

void GameEngineLocal::loop()
{
	double last = GetTime() - 0.1;

	for (;;)
	{
		// update time
		double now = GetTime();
		double dt = now - last;
		last = now;
		if (dt > 0.1)
			dt = 0.1;
		frame_time = dt;

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
			if (!is_game_focused()) {
				if ((event.type == SDL_KEYUP || event.type == SDL_KEYDOWN) && ImGui::GetIO().WantCaptureKeyboard)
					continue;
				if (!scene_hovered && (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse)
					continue;
			}

			get_gui()->handle_event(event);
		}

		Cmd_Manager::get()->execute_buffer();

		// update state
		switch (state)
		{
		case Engine_State::Idle:
			SDL_Delay(5);	// assuming this is a menu/tool state, delay a bit to save CPU

			if (map_spawned() && !get_level()->is_editor_level()) {
				stop_game();
				continue;	// goto next frame
			}
			else if(is_in_an_editor_state()){
				get_current_tool()->set_focus_state(editor_focus_state::Focused);
			}
			break;
		case Engine_State::Loading:

			// for compiling data etc.
			if (is_in_an_editor_state())
				get_current_tool()->set_focus_state(editor_focus_state::Background);

			execute_map_change();
			continue; // goto next frame
			break;
		case Engine_State::Game: {
			//ASSERT(cl->get_state() == CS_SPAWNED || is_host);

			double secs_per_tick = tick_interval;
			frame_remainder += dt;
			int num_ticks = (int)floor(frame_remainder / secs_per_tick);
			frame_remainder -= num_ticks * secs_per_tick;

			if (!is_host()) {
				frame_remainder += cl->adjust_time_step(num_ticks);
			}
			float orig_ft = frame_time;
			float orig_ti = tick_interval;

			frame_time *= g_slomo.get_float();
			tick_interval *= g_slomo.get_float();

			for (int i = 0; i < num_ticks; i++) {

				game_update_tick();

				// if update_tick() causes game to end
				if (state != Engine_State::Game)
					break;
			}

			if (state != Engine_State::Game)
				continue;	// goto next frame

			pre_render_update();

			frame_time = orig_ft;
			tick_interval = orig_ti;
		}break;
		
		}

		// can tick() tool when in a game state
		if (is_in_an_editor_state()) {
			get_current_tool()->tick(frame_time);
			// hack: when in editor state, still want physics simulation
			if (state != Engine_State::Game) {
				float dt = frame_time * g_slomo.get_float();
				g_physics->simulate_and_fetch(dt);
			}
		}

		draw_screen();

		Profiler::end_frame_tick();
	}
}


void GameEngineLocal::pre_render_update()
{
	ASSERT(state == Engine_State::Game);

}

void draw_console_hook() {
	dbg_console.draw();
}
void Debug_Console::init() {
	Debug_Interface::get()->add_hook("Console", draw_console_hook);
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
	return 0;
}

void Debug_Console::draw()
{
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

	// Command-line
	bool reclaim_focus = false;
	ImGuiInputTextFlags input_text_flags = ImGuiInputTextFlags_EnterReturnsTrue | 
		ImGuiInputTextFlags_EscapeClearsAll | ImGuiInputTextFlags_CallbackHistory;
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