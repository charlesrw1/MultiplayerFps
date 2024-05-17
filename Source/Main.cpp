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

#include "Shader.h"
#include "Texture.h"
#include "Framework/MathLib.h"
#include "Model.h"
#include "Animation/Runtime/Animation.h"
#include "Level.h"
#include "Physics.h"
#include "Net.h"
#include "Game_Engine.h"
#include "Types.h"
#include "Client.h"
#include "Server.h"
#include "Player.h"
#include "Framework/Config.h"
#include "DrawPublic.h"
#include "Entity.h"

#include "Animation/AnimationTreePublic.h"
#include "Animation/Editor/AnimationGraphEditorPublic.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"


MeshBuilder phys_debug;
Game_Engine* eng;

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
	eng->cleanup();
	exit(0);
}
void sys_print(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	dbg_console.print_args(fmt, args);
	va_end(args);
}

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
	eng->cleanup();
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
	float x_off = g_mousesens.real() * x;
	float y_off = g_mousesens.real() * y;

	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);

}

void Game_Engine::make_move()
{
	Player* p = get_local_player();

	if (!p)
		return;


	Move_Command command;
	command.view_angles = p->view_angles;
	command.tick = tick;

	if (g_fakemovedebug.integer() != 0)
		command.lateral_move = std::fmod(GetTime(), 2.f) > 1.f ? -1.f : 1.f;
	if (g_fakemovedebug.integer() == 2)
		command.button_mask |= BUTTON_JUMP;

	if (!game_focused) {
		p->set_input_command(command);
		if(cl->get_state()>=CS_CONNECTED) 
			cl->get_command(cl->OutSequence()) = command;
		return;
	}

	if (!(p->state_flags & EF_FROZEN_VIEW))
		view_angle_update(command.view_angles);


	int forwards_key = SDL_SCANCODE_W;
	int back_key = SDL_SCANCODE_S;

	if (keys[forwards_key])
		command.forward_move += 1.f;
	if (keys[back_key])
		command.forward_move -= 1.f;
	if (keys[SDL_SCANCODE_A])
		command.lateral_move += 1.f;
	if (keys[SDL_SCANCODE_D])
		command.lateral_move -= 1.f;
	if (keys[SDL_SCANCODE_Z])
		command.up_move += 1.f;
	if (keys[SDL_SCANCODE_X])
		command.up_move -= 1.f;
	if (keys[SDL_SCANCODE_SPACE])
		command.button_mask |= BUTTON_JUMP;
	if (keys[SDL_SCANCODE_LSHIFT])
		command.button_mask |= BUTTON_DUCK;
	if (mousekeys & (1<<1))
		command.button_mask |= BUTTON_FIRE1;
	if (keys[SDL_SCANCODE_E])
		command.button_mask |= BUTTON_RELOAD;
	if (keychanges[SDL_SCANCODE_LEFTBRACKET])
		command.button_mask |= BUTTON_ITEM_PREV;
	if (keychanges[SDL_SCANCODE_RIGHTBRACKET])
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

void Game_Engine::init_sdl_window()
{
	ASSERT(!window);

	if (SDL_Init(SDL_INIT_EVERYTHING)) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window_w.integer(), window_h.integer(), SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	if (!window) {
		printf(__FUNCTION__": %s\n", SDL_GetError());
		exit(-1);
	}

	gl_context = SDL_GL_CreateContext(window);
	sys_print("OpenGL loaded\n");
	gladLoadGLLoader(SDL_GL_GetProcAddress);
	sys_print("Vendor: %s\n", glGetString(GL_VENDOR));
	sys_print("Renderer: %s\n", glGetString(GL_RENDERER));
	sys_print("Version: %s\n\n", glGetString(GL_VERSION));

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

void Game_Media::load()
{
	model_manifest.clear();

	File_Buffer* manifest_f = Files::open("./Data/Models/manifest.txt", Files::TEXT | Files::LOOK_IN_ARCHIVE);


	//std::ifstream manifest_f("./Data/Models/manifest.txt");
	if (!manifest_f) {
		sys_print("Couldn't open model manifest\n");
		return;
	}
	char buffer[256];
	Buffer str;
	str.buffer = buffer;
	str.length = 256;
	int index = 0;
	while (file_getline(manifest_f, &str, &index, '\n')) {
		std::string s = str.buffer;
		model_manifest.push_back(s);
	}

	Files::close(manifest_f);

	model_cache.resize(model_manifest.size());

}

Model* Game_Media::get_game_model(const char* model, int* out_index)
{
	int i = 0;
	for (; i < model_manifest.size(); i++) {
		if (model_manifest[i] == model)
			break;
	}
	if (i == model_manifest.size()) {
		sys_print("Model %s not in manifest\n", model);
		if (out_index) *out_index = -1;
		return nullptr;
	}

	if(out_index) *out_index = i;
	if (model_cache[i]) return model_cache[i];
	model_cache[i] = FindOrLoadModel(model);
	return model_cache[i];
}
Model* Game_Media::get_game_model_from_index(int index)
{
	if (index < 0 || index >= model_manifest.size()) return nullptr;
	if (model_cache[index]) return model_cache[index];
	model_cache[index] = FindOrLoadModel(model_manifest[index].c_str());
	return model_cache[index];
}

void Entity::set_model(const char* model_name)
{
	int model_index = 0;
	model = eng->media.get_game_model(model_name, &model_index);
}


struct Sound
{
	char* buffer;
	int length;
};

int Game_Engine::player_num()
{
	if (is_host())
		return 0;
	if (cl && cl->client_num != -1)
		return cl->client_num;
	return -1;
}
Entity& Game_Engine::local_player()
{
	if (player_num() < 0) ASSERT(!"player not assigned");
	ASSERT(ents[player_num()]);
	return *ents[player_num()];
}
Player* Game_Engine::get_local_player()
{
	if (player_num() < 0) return nullptr;
	return (Player*)ents[player_num()];
}

void Game_Engine::connect_to(string address)
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
	eng->change_editor_state(Eng_Tool_state::None);
}

DECLARE_ENGINE_CMD(start_ed)
{
	static const char* usage_str = "Usage: starteditor [map,anim] <file>\n";
	if (args.size() != 3) {
		sys_print(usage_str);
		return;
	}

	std::string edit_type = args.at(1);
	Eng_Tool_state e{};
	if (edit_type == "map") {
		e = Eng_Tool_state::Level;
	}
	else if (edit_type == "anim") {
		e = Eng_Tool_state::Animgraph;
	}
	else {
		sys_print("unknown editor\n");
		sys_print(usage_str);
		return;
	}

	eng->change_editor_state(e, args.at(2));
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

void Game_Engine::change_editor_state(Eng_Tool_state next_tool, const char* file)
{
	sys_print("--------- Change Editor State ---------\n");
	if (tool_state != next_tool && tool_state != Eng_Tool_state::None)
		get_current_tool()->close();
	tool_state = next_tool;
	if (tool_state != Eng_Tool_state::None) {
		get_current_tool()->open(file);
		enable_imgui_docking();
	}
	else
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
		eng->bind_key(scancode, "");
	else
		eng->bind_key(scancode, args.at(2));
}

DECLARE_ENGINE_CMD_CAT("cl.",force_update)
{
	eng->cl->ForceFullUpdate();
}

DECLARE_ENGINE_CMD(connect)
{
	if (args.size() < 2) {
		sys_print("usage connect <address>");
		return;
	}

	eng->connect_to(args.at(1));
}
DECLARE_ENGINE_CMD(disconnect)
{
	eng->leave_current_game();
}
DECLARE_ENGINE_CMD(reconnect)
{
	if(eng->cl->get_state() != CS_DISCONNECTED)
		eng->cl->Reconnect();
}

DECLARE_ENGINE_CMD(map)
{
	if (args.size() < 2) {
		sys_print("usage map <map name>");
		return;
	}
	eng->queue_load_map(args.at(1));
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
		auto& entry = eng->cl->server.incoming[i];
		maxbytes = glm::max(maxbytes, entry.bytes);
		totalbytes += entry.bytes;
		mintime = glm::min(mintime, entry.time);
		maxtime = glm::max(maxtime, entry.time);
	}

	sys_print("Client Network Stats:\n");
	sys_print("%--15s %f\n", "Rtt", eng->cl->server.rtt);
	sys_print("%--15s %f\n", "Interval", maxtime - mintime);
	sys_print("%--15s %d\n", "Biggest packet", maxbytes);
	sys_print("%--15s %f\n", "Kbits/s", 8.f*(totalbytes / (maxtime-mintime))/1000.f);
	sys_print("%--15s %f\n", "Bytes/Packet", totalbytes / 64.0);
}

#ifdef _DEBUG
DECLARE_ENGINE_CMD(print_ents)
{

	sys_print("%--15s %--15s %--15s %--15s\n", "index", "class", "posx", "posz", "has_model");
	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
		Entity& e = ei.get();
		sys_print("%-15d %-15s %-15f %-15f %-15d\n", ei.get_index(), ei.get().get_classname().get_c_str(), e.position.x, e.position.z, (int)e.model);
	}
}
#endif

DECLARE_ENGINE_CMD(print_vars)
{
	if (args.size() == 1)
		Var_Manager::get()->print_vars(nullptr);
	else
		Var_Manager::get()->print_vars(args.at(1));
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



int main(int argc, char** argv)
{

	eng = new Game_Engine;

	eng->argc = argc;
	eng->argv = argv;
	eng->init();

	eng->loop();

	eng->cleanup();
	
	return 0;
}

#define DECLVAR(name,varname, value) Auto_Config_Var varname(name, value);
DECLVAR("mousesens", g_mousesens, 0.005f);
DECLVAR("fov", g_fov, 70.f);
DECLVAR("thirdperson", g_thirdperson, 0);
DECLVAR("fakemovedebug", g_fakemovedebug, 0);
DECLVAR("drawdebugmenu", g_drawdebugmenu, 1);
DECLVAR("drawimguidemo", g_drawimguidemo, 1);
DECLVAR("debug_skeletons", g_debug_skeletons, 1);
DECLVAR("debug_grid", g_draw_grid, 1);


DECLARE_ENGINE_CMD(spawn_npc)
{
	if (!eng->is_in_game())
		return;
	
	auto p = eng->get_local_player();
	if (!p) {
		sys_print("no player\n");
		return;
	}
	auto va = p->view_angles;
	vec3 front = AnglesToVector(va.x, va.y);
	vec3 pos = p->calc_eye_position();

	RayHit rh = eng->phys.trace_ray(Ray(pos, front), -1, PF_WORLD);
	if (rh.dist > 0) {

		auto npc = eng->create_entity("NPC");
		if (npc) {
			npc->position = rh.pos;
			npc->spawn();
		}
	}
}

void Game_Engine::queue_load_map(string nextname)
{
	// level will get loaded in next ::loop()
	mapname = nextname;
	state = Engine_State::Loading;
}

void Game_Engine::leave_current_game()
{
	// disconnect clients etc.
	// current map gets unloaded in next ::loop()
	state = Engine_State::Idle;
}

void Game_Engine::execute_map_change()
{
	sys_print("-------- Map Change: %s --------\n", mapname.c_str());

	// free current map
	stop_game();

	// try loading map
	level = LoadLevelFile(mapname.c_str());
	if (!level) {
		sys_print("!!! couldn't load map !!!\n");
		state = Engine_State::Idle;
		return;
	}

	num_entities = 0;
	tick = 0;
	time = 0.0;
	ents.resize(NUM_GAME_ENTS, nullptr);

	populate_map();

	if (is_host())
		spawn_starting_players(true);

	idraw->on_level_start();

	sys_print("changed state to Engine_State::Game\n");

	// fixme, for server set state to game, but clients will sit in a wait loop till they recieve their first
	// snapshot before continuing
	state = Engine_State::Game;
}


void Game_Engine::spawn_starting_players(bool initial)
{
	// fixme for multiplayer, spawn in clients that are connected to server ex on restart
	make_client(0);
}

void Game_Engine::set_tick_rate(float tick_rate)
{
	if (state == Engine_State::Game) {
		sys_print("Can't change tick rate while running\n");
		return;
	}
	tick_interval = 1.0 / tick_rate;
}


void Game_Engine::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;
		//console.set_keyboard_focus = show_console;
	}

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse) {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		eng->game_focused = false;
		return;
	}
	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

	if (event.type == SDL_KEYDOWN) {
		int scancode = event.key.keysym.scancode;
		keys[scancode] = true;
		keychanges[scancode] = true;

		if (binds[scancode]) {
			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, binds[scancode]->c_str());
		}
	}
	else if (event.type == SDL_KEYUP) {
		keys[event.key.keysym.scancode] = false;
	}
	// when drawing to windowed viewport, handle game_focused during drawing
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (event.button.button == 3 && !eng->is_drawing_to_window_viewport()) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			int x, y;
			SDL_GetRelativeMouseState(&x, &y);
			eng->game_focused = true;
		}
		mousekeys |= (1<<event.button.button);
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		if (event.button.button == 3 && !eng->is_drawing_to_window_viewport()) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			eng->game_focused = false;
		}
		mousekeys &= ~(1 << event.button.button);
	}
}

void Game_Engine::bind_key(int key, string command)
{
	ASSERT(key >= 0 && key < SDL_NUM_SCANCODES);
	if (!binds[key])
		binds[key] = new string;
	*binds[key] = std::move(command);
}

void Game_Engine::cleanup()
{
	FreeLoadedTextures();
	NetworkQuit();
	SDL_GL_DeleteContext(gl_context);
	SDL_DestroyWindow(window);

	for (int i = 0; i < SDL_NUM_SCANCODES; i++) {
		delete binds[i];
	}
}

bool bloom_stop = false;
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

DECLVAR("slomo", g_slomo, 1.f);


bool Game_Engine::is_drawing_to_window_viewport()
{
	return tool_state != Eng_Tool_state::None;
}

glm::ivec2 Game_Engine::get_game_viewport_dimensions()
{
	if (is_drawing_to_window_viewport())
		return window_viewport_size;
	else
		return { window_w.integer(), window_h.integer() };
}

void Game_Engine::draw_any_imgui_interfaces()
{
	if(g_drawdebugmenu.integer())
		Debug_Interface::get()->draw();

	// draw tool editor
	if (is_in_an_editor_state())
		get_current_tool()->imgui_draw();

	if (is_drawing_to_window_viewport()) {
		eng->game_focused = false;

		if (ImGui::Begin("Scene viewport",nullptr,  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {

			auto size = ImGui::GetWindowSize();
			ImGui::Image((ImTextureID)idraw->get_composite_output_texture_handle(), ImVec2(size.x, size.y), ImVec2(0,1),ImVec2(1,0));
			window_viewport_size = { size.x,size.y };

			bool focused_window = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

			eng->game_focused = focused_window && ImGui::GetIO().MouseDown[1];

		}
		if (eng->game_focused)
			SDL_SetRelativeMouseMode(SDL_TRUE);
		else
			SDL_SetRelativeMouseMode(SDL_FALSE);
		ImGui::End();
	}

	if(g_drawimguidemo.integer())
		ImGui::ShowDemoWindow();
}

bool Game_Engine::game_draw_screen()
{
	if (get_local_player() == nullptr)
		return false;

	Player* p = get_local_player();
	
	glm::vec3 position(0,0,0);
	glm::vec3 angles(0, 0, 0);
	float fov = g_fov.real();
	p->get_view(position, angles, fov);
	glm::vec3 front = AnglesToVector(angles.x, angles.y);

	auto viewport = get_game_viewport_dimensions();

	View_Setup vs = View_Setup(position, front, glm::radians(fov), 0.01, 100.0, viewport.x, viewport.y);

	idraw->scene_draw(vs, nullptr);

	return true;
}

void Game_Engine::draw_screen()
{
	GPUFUNCTIONSTART;

	int x, y;
	SDL_GetWindowSize(window, &x, &y);
	if (x % 2 == 1) x -= 1;
	if (y % 2 == 1) y -= 1;
	SDL_SetWindowSize(window, x, y);

	window_w.integer() = x;
	window_h.integer() = y;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// in main menu, draw the shell screen
	// in game, draw game and or shell screen
	// in loading, draw loading
	// in tool != none, draw tool gui

	if (state == Engine_State::Idle) {
		// draw general ui

		if (get_current_tool() != nullptr)
			get_current_tool()->draw_frame();

	}
	else if (state == Engine_State::Loading) {

		// draw loading ui etc.

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
	// if a tool is active, this is where drawing of the ui part happens
	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
	draw_any_imgui_interfaces();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	{
		GPUSCOPESTART("SDL_GL_SwapWindow");
		SDL_GL_SwapWindow(window);
	}
}

IEditorTool* Game_Engine::get_current_tool()
{
	if (tool_state == Eng_Tool_state::None)
		return nullptr;
	else if (tool_state == Eng_Tool_state::Animgraph)
		return  g_anim_ed_graph;
	else if (tool_state == Eng_Tool_state::Level)
		return g_editor_doc;
	else
		ASSERT(0);
}

void Game_Engine::build_physics_world(float time)
{
	static Auto_Config_Var only_world("dbg.onlyworld",0);

	phys.ClearObjs();
	{
		PhysicsObject obj;
		obj.is_level = true;
		obj.solid = true;
		obj.is_mesh = true;
		obj.mesh.structure = &level->collision->bvh;
		obj.mesh.verticies = &level->collision->verticies;
		obj.mesh.tris = &level->collision->tris;

		phys.AddObj(obj);
	}
	if (only_world.integer()) return;

	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
		Entity& ce = ei.get();

		//if (!(ce.flags & EF_SOLID)) continue;

		PhysicsObject po;
		po.userindex = ce.selfid;
		po.max = ce.phys_opt.size * 0.5f + ce.position;
		continue;

		phys.AddObj(po);
	}
}

void Game_Engine::update_game_tick()
{

	for (auto ei = Ent_Iterator(); ei.finish_at(Ent_Iterator(MAX_CLIENTS)); ei = ei.next()) {
		Entity& e = ei.get();
		ASSERT(e.get_classname() == NAME("Player"));
		Player* p = (Player*)&e;

		// fixme
		p->update();
		p->present();
	}

	for (auto ei = Ent_Iterator(MAX_CLIENTS); !ei.finished(); ei = ei.next()) {
		Entity& e = ei.get();
		e.update();
		e.present();
	}
}


DECLARE_ENGINE_CMD(reload_mats)
{
	sys_print("reloading materials\n");
	if (args.size() == 1)
		mats.load_material_file_directory("./Data/Materials/");
	else
		mats.load_material_file_directory(args.at(1));
}



View_Setup::View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height)
	: origin(origin),front(front),fov(fov),near(near),far(far),width(width),height(height)
{
	view = glm::lookAt(origin, origin + front, glm::vec3(0, 1.f, 0));
	proj = glm::perspective(fov, width / (float)height, near, far);
	viewproj = proj * view;
}

#define TIMESTAMP(x) printf("%s in %f\n",x,(float)GetTime()-start); start = GetTime();

Game_Engine::Game_Engine() :
	window_w("vid.width", 1200),
	window_h("vid.height", 800),
	window_fullscreen("vid.fullscreen", 0),
	host_port("net.hostport", DEFAULT_SERVER_PORT),
	ents(NUM_GAME_ENTS, nullptr),
	spawnids(NUM_GAME_ENTS,0)
{
	memset(binds, 0, sizeof binds);
	memset(keychanges, 0, sizeof keychanges);
	memset(keys, 0, sizeof keys);
}

extern Game_Mod_Manager mods;

ImNodesContext* ImNodesCreateContext()
{
	return nullptr;
}

void Game_Engine::init()
{
	sys_print("--------- Initializing Engine ---------\n");

	program_time_start = GetTime();

	memset(keys, 0, sizeof(keys));
	memset(keychanges, 0, sizeof(keychanges));
	memset(binds, 0, sizeof(binds));
	mousekeys = 0;
	num_entities = 0;
	level = nullptr;
	tick_interval = 1.0 / DEFAULT_UPDATE_RATE;
	state = Engine_State::Idle;
	is_hosting_game = false;
	sv = new Server;
	cl = new Client;
	
	dbg_console.init();

	// engine initilization
	float first_start = GetTime();
	float start = GetTime();
	init_sdl_window();
	TIMESTAMP("init sdl window");

	Profiler::init();
	Files::init();

	init_audio();
	TIMESTAMP("init audio");

	network_init();
	TIMESTAMP("net init");

	idraw->init();
	TIMESTAMP("draw init");

	mats.init();
	mats.load_material_file_directory("./Data/Materials/");
	anim_tree_man->init();
	TIMESTAMP("mats init");

	mods.init();
	TIMESTAMP("mods init");

	media.load();
	TIMESTAMP("media init");

	iparticle->init();

	cl->init();
	TIMESTAMP("cl init");

	sv->init();
	TIMESTAMP("sv init");



	// debug interface
	imgui_context = ImGui::CreateContext();
	
	g_anim_ed_graph->init();

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
			window_w.integer() = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-h") == 0) {
			window_h.integer() = atoi(argv[++i]);
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
	SDL_SetWindowSize(window, window_w.integer(), window_h.integer());
	TIMESTAMP("cfg exectute");


	Cmd_Manager::get()->execute_file(Cmd_Execute_Mode::NOW, "init.txt");

	printf("Total execute time: %f\n", GetTime() - first_start);
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


void Game_Engine::game_update_tick()
{
	CPUFUNCTIONSTART;

	Debug::on_fixed_update_start();

	make_move();
	//if (!is_host())
	//	cl->SendMovesAndMessages();

	time = tick * tick_interval;
	build_physics_world(0.f);
	update_game_tick();
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

	CPUSCOPESTART(animation_update); 
	{
		for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
			Entity& e = ei.get();
			if (!e.animator)
				continue;
			e.animator->tick_tree_new(eng->tick_interval);
		}
	}
}

void perf_tracker()
{
	static std::vector<float> time(200,0.f);
	static int index = 0;
	time.at(index) = eng->frame_time;
	index = index + 1;
	index %= 200;

	float avg_ft = 0.f;
	for (int i = 0; i < time.size(); i++)avg_ft += time.at(i);
	avg_ft /= 200.f;

	ImGui::Text("Frametime: %f", avg_ft*1000.f);

}


// unloads all game state
void Game_Engine::stop_game()
{
	if (!map_spawned())
		return;

	sys_print("-------- Clearing Map --------\n");

	ASSERT(level);
	
	for (auto ei = Ent_Iterator(0); !ei.finished(); ei = ei.next()) {
		ASSERT(&ei.get());
		delete &ei.get();
	}
	ents.clear();
	num_entities = 0;

	idraw->on_level_end();
	
	FreeLevel(level);
	level = nullptr;

	phys.ClearObjs();

	// clear any debug shapes
	Debug::on_fixed_update_start();
}

void Game_Engine::loop()
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

		// update input
		memset(keychanges, 0, sizeof keychanges);
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL2_ProcessEvent(&event);

			switch (event.type) {
			case SDL_QUIT:
				::Quit();
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEWHEEL:
				key_event(event);
				break;
			}
			if (get_tool_state() != Eng_Tool_state::None)
				get_current_tool()->handle_event(event);
		}

		// update state
		switch (state)
		{
		case Engine_State::Idle:
			// later, will add menu controls, now all you can do is use the console to change state
			SDL_Delay(5);

			if (map_spawned()) {
				stop_game();
				continue;	// goto next frame
			}
			break;
		case Engine_State::Loading:

			// for compiling data etc.
			if (get_tool_state() != Eng_Tool_state::None)
				get_current_tool()->signal_going_to_game();

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

			g_slomo.real() = glm::clamp(g_slomo.real(), 0.00001f, 2.0f);

			frame_time *= g_slomo.real();
			tick_interval *= g_slomo.real();

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
		
		}	// switch(Engine_State::?)

		if (tool_state != Eng_Tool_state::None)
			get_current_tool()->tick(frame_time);

		draw_screen();

		static float next_print = 0;
		static Auto_Config_Var print_fps("dbg.print_fps", 0);
		if (next_print <= 0 && print_fps.integer()) {
			next_print += 2.0;
			sys_print("fps %f", 1.0 / eng->frame_time);
		}
		else if (print_fps.integer())
			next_print -= eng->frame_time;

		Profiler::end_frame_tick();
	}
}

bool Ent_Iterator::finished() const
{
	return index == -1;
}
Ent_Iterator& Ent_Iterator::next()
{
	*this = Ent_Iterator(index + 1, summed_count);
	return *this;
}
Ent_Iterator::Ent_Iterator(int index_, int summed_count_)
{
	this->index = -1;
	this->summed_count = 0;
	while (index_ < NUM_GAME_ENTS && summed_count_ < eng->num_entities) {
		if (eng->ents[index_]) {
			this->index = index_;
			this->summed_count = summed_count_ + 1;
			break;
		}
		index_++;
	}
}
Entity& Ent_Iterator::get() const
{
	return *eng->get_ent(index);
}

Entity* Game_Engine::get_ent(int index)
{
	if (index < 0 || index >= ents.size())
		return nullptr;

	ASSERT(index >= 0 && index < NUM_GAME_ENTS);
	return ents[index];
}
Entity* Game_Engine::get_ent_from_handle(entityhandle index)
{
	ASSERT(index >= 0 && index < NUM_GAME_ENTS);
	return ents[index];
}

void Game_Engine::pre_render_update()
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
			ImVec4 color;
			bool has_color = false;
			if (!lines[i].empty() && lines[i][0]=='#') { color = ImVec4(1.0f, 0.8f, 0.6f, 1.0f); has_color = true; }
			if (has_color)
				ImGui::PushStyleColor(ImGuiCol_Text, color);
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
			print("#%s", input_buffer);

			Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, input_buffer);

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