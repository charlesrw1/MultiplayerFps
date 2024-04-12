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
#include "MathLib.h"
#include "Model.h"
#include "MeshBuilder.h"
#include "Util.h"
#include "Animation.h"
#include "Level.h"
#include "Physics.h"
#include "Net.h"
#include "Game_Engine.h"
#include "Types.h"
#include "Client.h"
#include "Server.h"
#include "Player.h"
#include "Config.h"
#include "DrawPublic.h"
#include "Entity.h"


#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

#include "AnimationGraphEditorPublic.h"

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

void Game_Local::update_view()
{
	if (!has_run_tick) has_run_tick = true;

	// decay view_recoil
	view_recoil.x = view_recoil.x * 0.9f;

	vec3 true_viewangles = eng->local.view_angles + view_recoil;

	if (view_recoil.y != 0)
		printf("view_recoil: %f\n", view_recoil.y);

	vec3 true_front = AnglesToVector(true_viewangles.x, true_viewangles.y);

	View_Setup setup;
	auto viewport_sz = eng->get_game_viewport_dimensions();
	setup.height = viewport_sz.y;
	setup.width = viewport_sz.x;
	setup.fov = glm::radians(fov.real());
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, 0.01f, 100.0f);
	setup.near = 0.01f;
	setup.far = 100.f;

	//if (update_camera) {
	//	fly_cam.UpdateFromInput(eng->keys, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	//}

	if (thirdperson_camera.integer()) {
		//ClientEntity* player = client.GetLocalPlayer();
		Entity& playerreal = eng->local_player();

		vec3 view_angles = eng->local.view_angles;

		vec3 front = AnglesToVector(view_angles.x, view_angles.y);
		vec3 side = normalize(cross(front, vec3(0,1,0)));


		vec3 camera_pos = playerreal.position + vec3(0, STANDING_EYE_OFFSET, 0) - front * 2.5f + side * 0.8f;
		

		//fly_cam.position = GetLocalPlayerInterpedOrigin()+vec3(0,0.5,0) - front * 3.f;
		fly_cam.position = camera_pos;
		setup.view = glm::lookAt(fly_cam.position, fly_cam.position + front, fly_cam.up);
		setup.front = front;
		setup.origin = fly_cam.position;
	}
	else
	{
		Entity& player = eng->local_player();
		float view_height = (player.state & PMS_CROUCHING) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
		vec3 cam_position = player.position + vec3(0, view_height, 0);
		setup.view = glm::lookAt(cam_position, cam_position + true_front, vec3(0, 1, 0));
		setup.origin = cam_position;
		setup.front = true_front;
	}

	setup.viewproj = setup.proj * setup.view;
	last_view = setup;

}

static void SDLError(const char* msg)
{
	printf(" % s: % s\n", msg, SDL_GetError());
	SDL_Quit();
	exit(-1);
}


void Game_Engine::view_angle_update()
{
	int x, y;
	SDL_GetRelativeMouseState(&x, &y);
	float x_off = eng->local.mouse_sensitivity.real() * x;
	float y_off = eng->local.mouse_sensitivity.real() * y;

	vec3 view_angles = local.view_angles;
	view_angles.x -= y_off;	// pitch
	view_angles.y += x_off;	// yaw
	view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_angles.y = fmod(view_angles.y, TWOPI);
	local.view_angles = view_angles;
}

void Game_Engine::make_move()
{
	// hack for local players
	Entity& e = local_player();
	if (e.force_angles == 1) {
		local.view_angles = e.diff_angles;
		e.force_angles = 0;
	}

	Move_Command command;
	command.view_angles = local.view_angles;
	command.tick = tick;

	if (eng->local.fake_movement_debug.integer() != 0)
		command.lateral_move = std::fmod(GetTime(), 2.f) > 1.f ? -1.f : 1.f;
	if (eng->local.fake_movement_debug.integer() == 2)
		command.button_mask |= BUTTON_JUMP;

	if (!game_focused) {
		local.last_command = command;
		if(cl->get_state()>=CS_CONNECTED) cl->get_command(cl->OutSequence()) = command;
		return;
	}

	if(!(e.flags & EF_FROZEN_VIEW))	
		view_angle_update();


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
	local.last_command = command;
	if(cl->get_state()>=CS_CONNECTED) cl->get_command(cl->OutSequence()) = command;
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

	blob_shadow = FindOrLoadTexture("blob_shadow.png");
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
	model = eng->media.get_game_model(model_name, &model_index);
	if (model && model->bones.size() > 0) {
		anim.set_model(model);
		anim.owner = this;
	}
}


struct Sound
{
	char* buffer;
	int length;
};

int Game_Engine::player_num()
{
	if (is_host)
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

void Game_Engine::connect_to(string address)
{
	travel_to_engine_state(ENGINE_LOADING, "connecting to another server");

	sys_print("Connecting to server %s\n", address.c_str());
	cl->connect(address);
}

#ifdef EDITDOC
#include "EditorDocPublic.h"

DECLARE_ENGINE_CMD(editdoc)
{
	if (args.size() != 2) {
		sys_print("Usage: editdoc mapname\n");
		return;
	}
	eng->start_editor(args.at(1));
}

DECLARE_ENGINE_CMD(animedit)
{
	if (args.size() != 2) {
		sys_print("Usage: animedit character\n");
		return;
	}
	eng->start_anim_editor(args.at(1));
}

/* particleedit */

void Game_Engine::enable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}
void Game_Engine::disable_imgui_docking()
{
	ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_DockingEnable);
}

void Game_Engine::travel_to_engine_state(Engine_State next_state, const char* exit_reason)
{
	const bool going_to_same_state = next_state == state;

	static const char* last_settings = nullptr;
	static size_t last_settings_size = 0;

	if (state == ENGINE_LVL_EDITOR) {
		g_editor_doc->close_doc();	// calls save on current document
	}
	else if (state == ENGINE_ANIMATION_EDITOR) {
		g_anim_ed_graph->close();
		last_settings = ImGui::SaveIniSettingsToMemory(&last_settings_size);
		disable_imgui_docking();
	}

	if (next_state == ENGINE_ANIMATION_EDITOR) {
		enable_imgui_docking();
		if (last_settings)
			ImGui::LoadIniSettingsFromMemory(last_settings, last_settings_size);
	}



	// if the client is transitioning from loading to game, don't unload
	if(!(next_state == ENGINE_GAME && state == ENGINE_LOADING))
		exit_to_menu(exit_reason);

	set_state(next_state);
}


void Game_Engine::start_anim_editor(const char* name)
{
	sys_print("starting anim editor %s\n", name);

	travel_to_engine_state(ENGINE_ANIMATION_EDITOR, "exiting because starting animation editor");

	eng->level = open_empty_level();
	idraw->on_level_start();
	g_anim_ed_graph->open(name);
}

void Game_Engine::start_editor(const char* map)
{
	sys_print("starting editor on map %s\n", map);

	travel_to_engine_state(ENGINE_LVL_EDITOR, "exiting because starting level editor");

	mapname = map;
	level = LoadLevelFile(map);
	if (!level) {
		sys_print("level not found, creating new level\n");
		level = open_empty_level();
		level->name = mapname;
	}

	idraw->on_level_start();

	g_editor_doc->open_doc(map);
}

void Game_Engine::close_editor()
{

}

#endif

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

DECLARE_ENGINE_CMD_CAT("sv.",end)
{
	eng->travel_to_engine_state(ENGINE_MENU, "server is ending");
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
	eng->travel_to_engine_state(ENGINE_MENU, "disconnecting from server");
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

	eng->start_map(args.at(1), false);
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


DECLARE_ENGINE_CMD(print_ents)
{
	sys_print("%--15s %--15s %--15s %--15s\n", "index", "class", "posx", "posz", "has_model");
	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
		Entity& e = ei.get();
		sys_print("%-15d %-15d %-15f %-15f %-15d\n", ei.get_index(), (int)e.class_, e.position.x, e.position.z, (int)e.model);
	}
}

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
#include "LispInterpreter.h"
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

Game_Local::Game_Local() : 
	thirdperson_camera("view.tp",1),
	fov("view.fov", 70.f),
	mouse_sensitivity("view.sens", 0.01f),
	fake_movement_debug("game.fakemove",0, (int)CVar_Flags::INTEGER)
{
	Var_Manager::get()->create_var("stopai", "", Generic_Value(int(0)), 0);
}

DECLARE_ENGINE_CMD(spawn_npc)
{
	if (eng->get_state() != ENGINE_GAME) return;
	
	vec3 front = AnglesToVector(eng->local.view_angles.x, eng->local.view_angles.y);
	vec3 pos = eng->local_player().position + vec3(0.f, STANDING_EYE_OFFSET, 0.f);

	RayHit rh = eng->phys.trace_ray(Ray(pos, front), -1, PF_WORLD);
	if (rh.dist > 0) {

		auto npc = eng->create_entity(entityclass::NPC);
		npc->position = rh.pos;
	}
}


void Game_Local::init()
{
	view_angles = glm::vec3(0.f);

	viewmodel = FindOrLoadModel("arms.glb");
	viewmodel_animator.set_model(viewmodel);
}

bool Game_Engine::start_map(string map, bool is_client)
{
	travel_to_engine_state(is_client ? ENGINE_LOADING : ENGINE_GAME, is_client ? "exiting to start client" : "exiting to start server");

	sys_print("Starting map %s\n", map.c_str());
	mapname = map;

	phys.ClearObjs();
	for (int i = 0; i < NUM_GAME_ENTS; i++) {
		ents[i] = nullptr;
		spawnids[i] = 0;
	}

	num_entities = 0;
	level = LoadLevelFile(mapname.c_str());
	if (!level) {
		return false;
	}
	tick = 0;
	time = 0;
	
	if (!is_client) {
		sv->start();
		eng->is_host = true;
		on_game_start();
		sv->connect_local_client();
	}

	idraw->on_level_start();

	return true;
}

void Game_Engine::set_tick_rate(float tick_rate)
{
	if (is_host) {
		sys_print("Can't change tick rate while server is running\n");
		return;
	}
	tick_interval = 1.0 / tick_rate;
}

void Game_Engine::unload_current_level()
{
	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
		free_entity(ei.get_index());
		ei.decrement_count();
	}

	FreeLevel(level);
	level = nullptr;
	phys.ClearObjs();
	num_entities = 0;
	tick = 0;

	Debug::on_fixed_update_start();
}

void Game_Engine::client_enter_into_game()
{
	sys_print("Local client entering into game state\n");
	travel_to_engine_state(ENGINE_GAME, "going from loading to game state");
}

// my design sucks, client calls exit_to_menu() so only server needs to be ended here
void Game_Engine::exit_to_menu(const char* reason)
{
	if (get_state() == ENGINE_MENU)
		return;

	if (cl->get_state() != CS_DISCONNECTED) {
		cl->disconnect_from_server(reason);
	}
	if (sv->initialized) {
		sv->end("exiting to menu");
	}

	unload_current_level();
	is_host = false;
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
		ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
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

float g_time_speedup = 1.f;
void draw_wind_menu()
{
	//ImGui::DragFloat("radius", &draw.ssao.radius, 0.02);
	//ImGui::DragFloat("angle bias", &draw.ssao.bias, 0.02);
	
	ImGui::DragFloat("g_time_speedup", &g_time_speedup, 0.01);
	if (g_time_speedup <= 0.0001) g_time_speedup = 0.0001;

	//ImGui::DragFloat("roughness", &draw.rough, 0.02);
	//ImGui::DragFloat("metalness", &draw.metal, 0.02);
	//ImGui::DragFloat3("aosphere", &draw.aosphere.x, 0.02);
	//ImGui::DragFloat2("vfog", &draw.vfog.x, 0.02);
	//ImGui::DragFloat3("ambient", &draw.ambientvfog.x, 0.02);
	//ImGui::DragFloat("spread", &draw.volfog.spread, 0.02);
	//ImGui::DragFloat("frustum", &draw.volfog.frustum_end, 0.02);
	//ImGui::DragFloat("slice", &draw.slice_3d, 0.04, 0, 4);
	

	//ImGui::Image(ImTextureID(EnviornmentMapHelper::get().integrator.lut_id), ImVec2(128, 128));

	//ImGui::Image(ImTextureID(draw.ssao.texture.viewnormal), ImVec2(512, 512));



	//ImGui::Image(ImTextureID(draw.tex.reflected_color), ImVec2(512, 512));
	//ImGui::Image(ImTextureID(draw.tex.scene_color), ImVec2(512, 512));
	//ImGui::SliderInt("layer", &bloom_layer, 0, BLOOM_MIPS - 1);
	//ImGui::Checkbox("upscale", &bloom_stop);
	//ImGui::Image(ImTextureID(draw.tex.bloom_chain[bloom_layer]), ImVec2(256, 256));


	ImGui::DragFloat3("wind dir", &wswind_dir.x, 0.04);
	ImGui::DragFloat("speed", &speed, 0.04);
	ImGui::DragFloat("height", &wsheight, 0.04);
	ImGui::DragFloat("radius", &wsradius, 0.04);
	ImGui::DragFloat("startheight", &wsstartheight, 0.04, 0.f, 1.f);
	ImGui::DragFloat("startradius", &wsstartradius, 0.04, 0.f, 1.f);
}

void menu_playervars()
{
	if (eng->get_state() != ENGINE_GAME) return;

	Entity& p = eng->local_player();
	ImGui::DragFloat3("vm", &eng->local.vm_offset[0], 0.02f);
	ImGui::DragFloat3("vm2", &eng->local.vm_scale[0], 0.02f);

	ImGui::DragFloat3("pos", &p.position.x);
	ImGui::DragFloat3("dir", &eng->local.view_angles.x);

	ImGui::DragFloat3("vel", &p.velocity.x);
	ImGui::LabelText("jump", "%d", bool(p.state & PMS_JUMPING));


	if (eng->is_host) {
		ImGui::Text("delta %f", eng->cl->time_delta);
		ImGui::Text("tick %f", eng->tick);
		ImGui::Text("frr %f", eng->frame_remainder);
	}
}

bool Game_Engine::is_drawing_to_window_viewport()
{
	return state == ENGINE_ANIMATION_EDITOR;
}

glm::ivec2 Game_Engine::get_game_viewport_dimensions()
{
	if (is_drawing_to_window_viewport())
		return window_viewport_size;
	else
		return { window_w.integer(), window_h.integer() };
}

void Game_Engine::draw_debug_interface()
{
	Debug_Interface::get()->draw();

	if (state == ENGINE_LVL_EDITOR)
		g_editor_doc->imgui_draw();

	if (state == ENGINE_ANIMATION_EDITOR) {
		ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

		g_anim_ed_graph->begin_draw();
	}

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
}

void Game_Engine::draw_screen()
{
	GPUFUNCTIONSTART;

	glCheckError();
	int x, y;
	SDL_GetWindowSize(window, &x, &y);
	if (x % 2 == 1)x -= 1;
	if (y % 2 == 1)y -= 1;
	SDL_SetWindowSize(window, x, y);

	window_w.integer() = x;
	window_h.integer() = y;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	View_Setup view;
	if (state == ENGINE_GAME && local.has_run_tick) {
		view = local.last_view;
		idraw->scene_draw(view);
	}
	else if (state == ENGINE_LVL_EDITOR) {
		view = g_editor_doc->get_vs();
		idraw->scene_draw(view, special_render_mode::lvl_editor);
	}
	else if (state == ENGINE_ANIMATION_EDITOR) {
		view = g_anim_ed_graph->get_vs();
		idraw->scene_draw(view, special_render_mode::anim_editor);
	}

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	ImGui_ImplSDL2_NewFrame();
	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
	draw_debug_interface();
	ImGui::ShowDemoWindow();

	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	glCheckError();
	{
		GPUSCOPESTART("SDL_GL_SwapWindow");
		SDL_GL_SwapWindow(window);
	}
	glCheckError();
}

void Game_Engine::set_state(Engine_State state)
{
	static const char* engine_strs[] = { "menu", "loading", "game", "lvl editor", "anim editor"};
	static_assert((sizeof(engine_strs) / sizeof(char*)) == ENGINE_STATE_COUNT, "forgot to add engine state str");

	if (this->state != state)
		sys_print("Engine going to %s state\n", engine_strs[(int)state]);
	this->state = state;
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

		if (!(ce.flags & EF_SOLID)) continue;

		PhysicsObject po;
		po.userindex = ce.selfid;
		if (ce.class_ == entityclass::PLAYER) {
			float height = (!(ce.state & PMS_CROUCHING)) ? CHAR_STANDING_HB_HEIGHT : CHAR_CROUCING_HB_HEIGHT;
			vec3 mins = ce.position - vec3(CHAR_HITBOX_RADIUS, 0, CHAR_HITBOX_RADIUS);
			vec3 maxs = ce.position + vec3(CHAR_HITBOX_RADIUS, height, CHAR_HITBOX_RADIUS);
			po.max = maxs;
			po.min_or_origin = mins;
			po.player = true;
		}
		else if (ce.physics == EPHYS_MOVER && ce.model && ce.model->collision) {
			// have to transform the verts... bad bad bad
			mat4 model = glm::translate(mat4(1), ce.position);
			model = model * glm::eulerAngleXYZ(0.f, ce.rotation.y, 0.f);
			po.transform = model;
			po.inverse_transform = glm::inverse(model);

			po.is_mesh = true;
			po.mesh.verticies = &ce.model->collision->verticies;
			po.mesh.tris = &ce.model->collision->tris;
			po.mesh.structure = &ce.model->collision->bvh;
		}
		else
			continue;

		phys.AddObj(po);
	}
}

void player_update(Entity* e);
void DummyUpdate(Entity* e);
void grenade_update(Entity* e);


void Game_Engine::update_game_tick()
{
	// update local player
	execute_player_move(0, local.last_command);

	for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
		Entity& e = ei.get();

		e.physics_update();
		e.update();
		if(e.model && e.model->animations)
			e.anim.AdvanceFrame(tick_interval);
	}

	if (is_host) {
		// for local server interpolation
		for (auto ei = Ent_Iterator(1); !ei.finished(); ei = ei.next()) {
			Entity& e = ei.get();
		}
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
	program_time_start = GetTime();

	memset(keys, 0, sizeof(keys));
	memset(keychanges, 0, sizeof(keychanges));
	memset(binds, 0, sizeof(binds));
	mousekeys = 0;
	num_entities = 0;
	level = nullptr;
	tick_interval = 1.0 / DEFAULT_UPDATE_RATE;
	state = ENGINE_MENU;
	is_host = false;
	sv = new Server;
	cl = new Client;
	
	dbg_console.init();

	// hook debug menus
	Debug_Interface::get()->add_hook("Movement Vars", move_variables_menu);
	Debug_Interface::get()->add_hook("Wind Vars", draw_wind_menu);
	Debug_Interface::get()->add_hook("Player vars", menu_playervars);

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

	local.init();
	TIMESTAMP("local init");


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
	if (!is_host)
		cl->SendMovesAndMessages();

	time = tick * tick_interval;
	if (is_host) {
		// build physics world now as ReadPackets() executes player commands
		build_physics_world(0.f);
		//sv->ReadPackets();
		update_game_tick();
		//sv->make_snapshot();
		//for (int i = 0; i < sv->clients.size(); i++)
		//	sv->clients[i].Update();
		tick += 1;
	}

	if (!is_host) {
		cl->ReadPackets();
		cl->run_prediction();
		tick += 1;
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


void Game_Engine::loop()
{
	Debug_Interface::get()->add_hook("Frametime", perf_tracker);

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

			#ifdef EDITDOC
			if (state == ENGINE_LVL_EDITOR) {
				g_editor_doc->handle_event(event);
			}
			else if (state == ENGINE_ANIMATION_EDITOR) {
				g_anim_ed_graph->handle_event(event);
			}
			#endif
		}

		// update state
		switch (state)
		{
		case ENGINE_MENU:
			// later, will add menu controls, now all you can do is use the console to change state
			SDL_Delay(5);
			break;

		#ifdef EDITDOC
		case ENGINE_LVL_EDITOR:
			g_editor_doc->update();
			break;

		case ENGINE_ANIMATION_EDITOR:
			g_anim_ed_graph->tick(frame_time);
			break;
		#endif
		case ENGINE_LOADING:
			// here, the client tries to connect and waits for snapshot to arrive before going to game state
			cl->TrySendingConnect();
			cl->ReadPackets();
			SDL_Delay(5);
			break;
		case ENGINE_GAME: {
			ASSERT(cl->get_state() == CS_SPAWNED || is_host);

			double secs_per_tick = tick_interval;
			frame_remainder += dt;
			int num_ticks = (int)floor(frame_remainder / secs_per_tick);
			frame_remainder -= num_ticks * secs_per_tick;

			if (!is_host) {
				frame_remainder += cl->adjust_time_step(num_ticks);
			}
			float orig_ft = frame_time;
			float orig_ti = tick_interval;
			frame_time *= g_time_speedup;
			tick_interval *= g_time_speedup;

			for (int i = 0; i < num_ticks && state == ENGINE_GAME; i++) {
				game_update_tick();

				CPUSCOPESTART("animation update");
				for (auto ei = Ent_Iterator(); !ei.finished(); ei = ei.next()) {
					Entity& e = ei.get();
					if (!e.model || !e.model->animations)
						continue;

					e.anim.SetupBones();
					e.anim.ConcatWithInvPose();
				}
			}

			if(state == ENGINE_GAME)
				pre_render_update();

			frame_time = orig_ft;
			tick_interval = orig_ti;
		}break;
		}

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
Entity& Ent_Iterator::get()
{
	return *eng->get_ent(index);
}

Entity* Game_Engine::get_ent(int index)
{
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
	ASSERT(state == ENGINE_GAME);

		// interpolate entities for rendering
	if (!is_host)
		cl->interpolate_states();
	else {
		for (auto ei = Ent_Iterator(1); !ei.finished(); ei = ei.next()) {
			auto& e = ei.get();
			
		}
	}

	local.update_viewmodel();

	local.update_view();

	for (auto ei = Ent_Iterator(0); !ei.finished(); ei = ei.next()) {
		ei.get().update_visuals();
	}
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