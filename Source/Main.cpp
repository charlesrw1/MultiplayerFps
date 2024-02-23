#include <SDL2/SDL.h>

#include "SDL2/SDL_mixer.h"
#include "phonon.h"

#include "glad/glad.h"
#include "stb_image.h"
#include <cstdio>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "Shader.h"
#include "Texture.h"
#include "MathLib.h"
#include "GlmInclude.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
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
#include "Draw.h"
#include "Profilier.h"

#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_sdl2.h"

MeshBuilder phys_debug;
Engine_Config cfg;
Game_Engine engine;

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

void Quit()
{
	sys_print("Quiting...\n");
	engine.cleanup();
	exit(0);
}
void sys_print(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	engine.console.print_args(fmt, args);
	va_end(args);
}

void sys_vprint(const char* fmt, va_list args)
{
	vprintf(fmt, args);
	engine.console.print_args(fmt, args);
}

void Fatalf(const char* format, ...)
{
	va_list list;
	va_start(list, format);
	sys_vprint(format, list);
	va_end(list);
	fflush(stdout);
	engine.cleanup();
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

	vec3 true_viewangles = engine.local.view_angles + view_recoil;

	if (view_recoil.y != 0)
		printf("view_recoil: %f\n", view_recoil.y);

	vec3 true_front = AnglesToVector(true_viewangles.x, true_viewangles.y);

	View_Setup setup;
	setup.height = engine.window_h->integer;
	setup.width = engine.window_w->integer;
	setup.fov = glm::radians(fov->real);
	setup.proj = glm::perspective(setup.fov, (float)setup.width / setup.height, 0.01f, 100.0f);
	setup.near = 0.01f;
	setup.far = 100.f;

	//if (update_camera) {
	//	fly_cam.UpdateFromInput(engine.keys, input.mouse_delta_x, input.mouse_delta_y, input.scroll_delta);
	//}

	if (thirdperson_camera->integer) {
		//ClientEntity* player = client.GetLocalPlayer();
		Entity& playerreal = engine.local_player();

		vec3 view_angles = engine.local.view_angles;

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
		Entity& player = engine.local_player();
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
	float x_off = engine.local.mouse_sensitivity->real * x;
	float y_off = engine.local.mouse_sensitivity->real * y;

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

	if (engine.local.fake_movement_debug->integer != 0)
		command.lateral_move = std::fmod(GetTime(), 2.f) > 1.f ? -1.f : 1.f;
	if (engine.local.fake_movement_debug->integer == 2)
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

	program_time_start = GetTime();

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

	window = SDL_CreateWindow("CsRemake", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
		window_w->integer, window_h->integer, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

glm::mat4 Fly_Camera::get_view_matrix() const {
	return glm::lookAt(position, position + front, up);
}

void Fly_Camera::scroll_speed(int amt)
{
	move_speed += (move_speed * 0.5) * amt;
	if (abs(move_speed) < 0.000001)
		move_speed = 0.0001;
}
void Fly_Camera::update_from_input(const bool keys[], int mouse_dx, int mouse_dy)
{
	int xpos, ypos;
	xpos = mouse_dx;
	ypos = mouse_dy;

	float x_off = xpos;
	float y_off = ypos;
	float sensitivity = 0.01;
	x_off *= sensitivity;
	y_off *= sensitivity;

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

	vec3 right = cross(up, front);
	if (keys[SDL_SCANCODE_W])
		position += move_speed * front;
	if (keys[SDL_SCANCODE_S])
		position -= move_speed * front;
	if (keys[SDL_SCANCODE_A])
		position += right * move_speed;
	if (keys[SDL_SCANCODE_D])
		position -= right * move_speed;
	if (keys[SDL_SCANCODE_Z])
		position += move_speed * up;
	if (keys[SDL_SCANCODE_X])
		position -= move_speed * up;
}


void Game_Media::load()
{
	model_manifest.clear();

	std::ifstream manifest_f("./Data/Models/manifest.txt");
	if (!manifest_f) {
		sys_print("Couldn't open model manifest\n");
		return;
	}
	std::string line;
	while (std::getline(manifest_f, line))
		model_manifest.push_back(line);
	model_cache.resize(model_manifest.size());

	blob_shadow = FindOrLoadTexture("blob_shadow.png");
}

const Model* Game_Media::get_game_model(const char* model, int* out_index)
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

	if(out_index) *out_index = i+engine.level->linked_meshes.size();
	if (model_cache[i]) return model_cache[i];
	model_cache[i] = FindOrLoadModel(model);
	return model_cache[i];
}
const Model* Game_Media::get_game_model_from_index(int index)
{
	// check linked meshes
	if (index >= 0 && index < engine.level->linked_meshes.size())
		return engine.level->linked_meshes.at(index);
	index -= engine.level->linked_meshes.size();

	if (index < 0 || index >= model_manifest.size()) return nullptr;
	if (model_cache[index]) return model_cache[index];
	model_cache[index] = FindOrLoadModel(model_manifest[index].c_str());
	return model_cache[index];
}

void Entity::set_model(const char* model_name)
{
	model = engine.media.get_game_model(model_name, &model_index);
	if (model && model->bones.size() > 0)
		anim.set_model(model);
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
	ASSERT(0 && "player num called without game running");
	return 0;
}
Entity& Game_Engine::local_player()
{
	//ASSERT(engine_state >= LOADING);
	return ents[player_num()];
}

void Game_Engine::connect_to(string address)
{
	if (is_host)
		exit_map();
	else if (cl->get_state() != CS_DISCONNECTED)
		cl->Disconnect("connecting to another server");
	sys_print("Connecting to server %s\n", address.c_str());
	cl->connect(address);
}

void cmd_quit()
{
	Quit();
}
void cmd_bind()
{
	auto& args = cfg.get_arg_list();
	if (args.size() < 2) return;
	int scancode = SDL_GetScancodeFromName(args[1].c_str());
	if (scancode == SDL_SCANCODE_UNKNOWN) return;
	if (args.size() < 3)
		engine.bind_key(scancode, "");
	else
		engine.bind_key(scancode, args[2].c_str());
}

void cmd_server_end()
{
	engine.exit_map();
}
void cmd_client_force_update()
{
	engine.cl->ForceFullUpdate();
}
void cmd_client_connect()
{
	auto& args = cfg.get_arg_list();
	if (args.size() < 2) {
		sys_print("usage connect <address>");
		return;
	}

	engine.connect_to(args.at(1));
}
void cmd_client_disconnect()
{
	if (engine.cl->get_state()!=CS_DISCONNECTED)
		engine.cl->Disconnect("requested");
}
void cmd_client_reconnect()
{
	if(engine.cl->get_state() != CS_DISCONNECTED)
		engine.cl->Reconnect();
}
void cmd_load_map()
{
	if (cfg.get_arg_list().size() < 2) {
		sys_print("usage map <map name>");
		return;
	}

	engine.start_map(cfg.get_arg_list().at(1), false);
}
void cmd_exec_file()
{
	if (cfg.get_arg_list().size() < 2) {
		sys_print("usage map <map name>");
		return;
	}
	cfg.execute_file(cfg.get_arg_list().at(1).c_str());
}

void cmd_print_client_net_stats()
{
	float mintime = INFINITY;
	float maxtime = -INFINITY;
	int maxbytes = -5000;
	int totalbytes = 0;
	for (int i = 0; i < 64; i++) {
		auto& entry = engine.cl->server.incoming[i];
		maxbytes = glm::max(maxbytes, entry.bytes);
		totalbytes += entry.bytes;
		mintime = glm::min(mintime, entry.time);
		maxtime = glm::max(maxtime, entry.time);
	}

	sys_print("Client Network Stats:\n");
	sys_print("%--15s %f\n", "Rtt", engine.cl->server.rtt);
	sys_print("%--15s %f\n", "Interval", maxtime - mintime);
	sys_print("%--15s %d\n", "Biggest packet", maxbytes);
	sys_print("%--15s %f\n", "Kbits/s", 8.f*(totalbytes / (maxtime-mintime))/1000.f);
	sys_print("%--15s %f\n", "Bytes/Packet", totalbytes / 64.0);
}

void cmd_print_entities()
{
	sys_print("%--15s %--15s %--15s %--15s\n", "index", "class", "posx", "posz", "has_model");
	for (int i = 0; i < NUM_GAME_ENTS; i++) {
		auto& e = engine.get_ent(i);
		if (!e.active()) continue;
		sys_print("%-15d %-15d %-15f %-15f %-15d\n", i, e.type, e.position.x, e.position.z, (int)e.model);
	}
}

void cmd_print_vars()
{
	auto& args = cfg.get_arg_list();
	if (args.size() == 1)
		cfg.print_vars(nullptr);
	else
		cfg.print_vars(args.at(1).c_str());
}
void cmd_reload_shaders()
{
	draw.reload_shaders();
}

Global_Memory_Context mem_ctx;

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

Mix_Chunk* gun_sound;

void steam_audio_log_callback(IPLLogLevel level, const char* msg)
{
	sys_print("%s\n", msg);
}

const int AUDIO_SAMPLE_RATE = 44100;
const int AUDIO_FRAME = 1024;

void Audio_System::init()
{
	bool stereo = true;

	int fail = Mix_OpenAudio(44'100, AUDIO_S16SYS, (stereo) ? 2 : 1, 2048);
	if (fail) {
		printf("init audio failed\n");
		return;
	}
}

void init_audio()
{
	int fail = Mix_OpenAudio(44'100, AUDIO_S16SYS, 1, 2048);
	if (fail) {
		printf("init audio failed\n");
		return;
	}

	gun_sound = Mix_LoadWAV("Data\\Sounds\\q009\\explosion.ogg");

	IPLContext context = nullptr;
	IPLContextSettings contextsettings = {};
	contextsettings.logCallback = steam_audio_log_callback;
	contextsettings.version = STEAMAUDIO_VERSION;
	iplContextCreate(&contextsettings, &context);

	IPLHRTFSettings hrtfSettings{};
	hrtfSettings.type = IPL_HRTFTYPE_DEFAULT;

	IPLAudioSettings audio_settings{};
	audio_settings.frameSize = AUDIO_FRAME;
	audio_settings.samplingRate = AUDIO_SAMPLE_RATE;

	IPLHRTF hrtf = nullptr;
	iplHRTFCreate(context, &audio_settings, &hrtfSettings, &hrtf);

	IPLBinauralEffectSettings effectSettings{};
	effectSettings.hrtf = hrtf;
	
	IPLBinauralEffect effect = nullptr;
	iplBinauralEffectCreate(context, &audio_settings, &effectSettings, &effect);


	if (!gun_sound) {
		printf("couldn't load sound\n");
	}

	Mix_PlayChannel(0, gun_sound, 2);
}

extern void benchmark_run();
extern void benchmark_gltf();

int main(int argc, char** argv)
{

	engine.argc = argc;
	engine.argv = argv;
	engine.init();

	engine.loop();

	engine.cleanup();
	
	return 0;
}

void Game_Local::init()
{
	view_angles = glm::vec3(0.f);
	thirdperson_camera	= cfg.get_var("thirdperson_camera", "1");
	fov					= cfg.get_var("fov", "70.0");
	mouse_sensitivity	= cfg.get_var("mouse_sensitivity", "0.01");
	fake_movement_debug = cfg.get_var("fake_movement_debug", "0");
	
	pm.init();

	viewmodel = FindOrLoadModel("arms.glb");
	viewmodel_animator.set_model(viewmodel);
}

bool Game_Engine::start_map(string map, bool is_client)
{
	sys_print("Starting map %s\n", map.c_str());
	mapname = map;
	FreeLevel(level);
	phys.ClearObjs();
	for (int i = 0; i < MAX_GAME_ENTS; i++)
		ents[i] = Entity();
	num_entities = 0;
	level = LoadLevelFile(mapname.c_str());
	if (!level) {
		return false;
	}
	tick = 0;
	time = 0;
	
	if (!is_client) {
		if (cl->get_state() != CS_DISCONNECTED)
			cl->Disconnect("starting a local server");

		sv->start();
		engine.is_host = true;
		engine.set_state(ENGINE_GAME);
		on_game_start();
		sv->connect_local_client();
	}

	draw.on_level_start();

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

void Game_Engine::exit_map()
{
	if (!sv->initialized)
		return;

	FreeLevel(level);
	level = nullptr;
	sv->end("exiting to menu");
	is_host = false;
}


void Game_Engine::key_event(SDL_Event event)
{
	if (event.type == SDL_KEYDOWN && event.key.keysym.scancode == SDL_SCANCODE_GRAVE) {
		show_console = !show_console;
		console.set_keyboard_focus = show_console;
	}

	if ((event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) && ImGui::GetIO().WantCaptureMouse)
		return;
	if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && ImGui::GetIO().WantCaptureKeyboard)
		return;

	if (event.type == SDL_KEYDOWN) {
		int scancode = event.key.keysym.scancode;
		keys[scancode] = true;
		keychanges[scancode] = true;

		if (binds[scancode]) {
			cfg.execute(*binds[scancode]);
		}
	}
	else if (event.type == SDL_KEYUP) {
		keys[event.key.keysym.scancode] = false;
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		if (event.button.button == 3) {
			SDL_SetRelativeMouseMode(SDL_TRUE);
			int x, y;
			SDL_GetRelativeMouseState(&x, &y);
			engine.game_focused = true;
		}
		mousekeys |= (1<<event.button.button);
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		if (event.button.button == 3) {
			SDL_SetRelativeMouseMode(SDL_FALSE);
			engine.game_focused = false;
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
	FreeLoadedModels();
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

void draw_wind_menu()
{
	ImGui::DragFloat("radius", &draw.ssao.radius, 0.02);
	ImGui::DragFloat("angle bias", &draw.ssao.bias, 0.02);


	ImGui::DragFloat("roughness", &draw.rough, 0.02);
	ImGui::DragFloat("metalness", &draw.metal, 0.02);
	ImGui::DragFloat3("aosphere", &draw.aosphere.x, 0.02);
	ImGui::DragFloat2("vfog", &draw.vfog.x, 0.02);
	ImGui::DragFloat3("ambient", &draw.ambientvfog.x, 0.02);
	ImGui::DragFloat("spread", &draw.volfog.spread, 0.02);
	ImGui::DragFloat("frustum", &draw.volfog.frustum_end, 0.02);
	ImGui::DragFloat("slice", &draw.slice_3d, 0.04, 0, 4);
	ImGui::DragFloat("epsilon", &draw.shadowmap.epsilon, 0.0002, 0, 0.5);
	ImGui::DragFloat("log_lin_lerp_factor", &draw.shadowmap.log_lin_lerp_factor, 0.02, 0, 1.0);
	ImGui::DragFloat("poly_factor", &draw.shadowmap.poly_factor, 0.02, 0, 8.0);
	ImGui::DragFloat("poly_units", &draw.shadowmap.poly_units, 0.02, 0, 8.0);
	ImGui::DragFloat("z_dist_scaling", &draw.shadowmap.z_dist_scaling, 0.02, 0, 8.0);
	ImGui::SliderInt("cubemap index", &draw.cubemap_index, 0, 12);


	ImGui::SliderInt("layer", &bloom_layer, 0, BLOOM_MIPS - 1);
	ImGui::Checkbox("upscale", &bloom_stop);
	ImGui::Image(ImTextureID(draw.tex.bloom_chain[bloom_layer]), ImVec2(256, 256));



	ImGui::DragFloat3("wind dir", &wswind_dir.x, 0.04);
	ImGui::DragFloat("speed", &speed, 0.04);
	ImGui::DragFloat("height", &wsheight, 0.04);
	ImGui::DragFloat("radius", &wsradius, 0.04);
	ImGui::DragFloat("startheight", &wsstartheight, 0.04, 0.f, 1.f);
	ImGui::DragFloat("startradius", &wsstartradius, 0.04, 0.f, 1.f);

	ImGui::Image((ImTextureID)EnviornmentMapHelper::get().integrator.lut_id, ImVec2(512, 512));
}

void Game_Engine::draw_debug_interface()
{
	if(show_console)
		console.draw();
	move_variables_menu();

	Profilier::get_instance().draw_imgui_window();

	if (state == ENGINE_GAME) {

		// player menu
		if (ImGui::Begin("Game")) {
			draw_wind_menu();

			Entity& p = engine.local_player();
			ImGui::DragFloat3("vm", &engine.local.vm_offset[0],0.02f);
			ImGui::DragFloat3("vm2", &engine.local.vm_scale[0], 0.02f);

			ImGui::DragFloat3("pos", &p.position.x);
			ImGui::DragFloat3("dir", &engine.local.view_angles.x);

			ImGui::DragFloat3("vel", &p.velocity.x);
			ImGui::LabelText("jump", "%d", bool(p.state & PMS_JUMPING));


			if (is_host) {
					ImGui::Text("delta %f", cl->time_delta);
					ImGui::Text("tick %f", engine.tick);
					ImGui::Text("frr %f", engine.frame_remainder);
			}
		}
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

	cfg.set_var("window_w", std::to_string(x).c_str());
	cfg.set_var("window_h", std::to_string(y).c_str());
	if (draw.vsync->integer != 0 && draw.vsync->integer != 1)
		cfg.set_var("vsync", "0");
	SDL_GL_SetSwapInterval(0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glClear(GL_COLOR_BUFFER_BIT);
	if (state == ENGINE_GAME && local.has_run_tick)
		draw.FrameDraw();

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
	const char* engine_strs[] = { "menu", "loading", "game" };

	if (this->state != state)
		sys_print("Engine going to %s state\n", engine_strs[(int)state]);
	this->state = state;
}

void Game_Engine::build_physics_world(float time)
{
	static Config_Var* only_world = cfg.get_var("phys/only_world", "0");

	phys.ClearObjs();
	{
		PhysicsObject obj;
		obj.is_level = true;
		obj.solid = true;
		obj.is_mesh = true;
		obj.mesh.structure = &level->collision.bvh;
		obj.mesh.verticies = &level->collision.verticies;
		obj.mesh.tris = &level->collision.tris;

		phys.AddObj(obj);
	}
	if (only_world->integer) return;

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& ce = ents[i];
		if (ce.type == ET_FREE) continue;

		if (!(ce.flags & EF_SOLID)) continue;

		PhysicsObject po;
		po.userindex = i;
		if (ce.type == ET_PLAYER) {
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

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& e = ents[i];
		if (!ents[i].active())
			continue;

		if (e.timer > 0.f) {
			e.timer -= engine.tick_interval;
			if (e.timer <= 0.f && e.timer_callback)
				e.timer_callback(&e);
		}

		e.physics_update();
		if (e.update)
			e.update(&e);
		if(e.model && e.model->animations)
			e.anim.AdvanceFrame(tick_interval);
	}

	// for local server interpolation
	for (int i = 1; i < MAX_CLIENTS; i++) {
		if (get_ent(i).active())
			get_ent(i).shift_last();
	}
}

void cmd_debug_counter()
{
	if (cfg.get_arg_list().at(1) == "1")
		engine.cl->offset_debug++;
	else
		engine.cl->offset_debug--;
	printf("offset client: %d\n", engine.cl->offset_debug);
}
void cmd_reload_materials()
{
	sys_print("reloading materials\n");
	if (cfg.get_arg_list().size() == 1)
		mats.load_material_file_directory("./Data/Materials/");
	else
		mats.load_material_file_directory(cfg.get_arg_list().at(1).c_str());
}

#define TIMESTAMP(x) printf("%s in %f\n",x,(float)GetTime()-start); start = GetTime();

void Game_Engine::init()
{
	loading_mem.init("LOADING", 2'000'000);
	per_frame_and_level_mem.init("FRAME AND LEVEL", 4'000'000);
	program_long_mem.init("PROGRAM", 4'000'000);

	mem_ctx.temp_default = { &per_frame_and_level_mem, false };
	mem_ctx.level_default = { &per_frame_and_level_mem, true };
	mem_ctx.long_default = { &program_long_mem, false };
	mem_ctx.loading_default = { &loading_mem, false };

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
	cfg.set_unknown_variables = true;

	// config vars
	window_w			= cfg.get_var("window_w", "1200", true);
	window_h			= cfg.get_var("window_h", "800", true);
	window_fullscreen	= cfg.get_var("window_fullscreen", "0", true);
	host_port			= cfg.get_var("host_port", std::to_string(DEFAULT_SERVER_PORT).c_str());

	// engine commands
	cfg.set_command("connect", cmd_client_connect);
	cfg.set_command("reconnect", cmd_client_reconnect);
	cfg.set_command("disconnect", cmd_client_disconnect);
	cfg.set_command("map", cmd_load_map);
	cfg.set_command("sv_end", cmd_server_end);
	cfg.set_command("bind", cmd_bind);
	cfg.set_command("quit", cmd_quit);
	cfg.set_command("counter", cmd_debug_counter);
	cfg.set_command("net_stat", cmd_print_client_net_stats);
	cfg.set_command("cl_full_update", cmd_client_force_update);
	cfg.set_command("print_ents", cmd_print_entities);
	cfg.set_command("print_vars", cmd_print_vars);
	cfg.set_command("exec", cmd_exec_file);
	cfg.set_command("reload_shaders", cmd_reload_shaders);
	cfg.set_command("reload_mats", cmd_reload_materials);

	// engine initilization
	float start = GetTime();
	init_sdl_window();
	TIMESTAMP("init sdl window");

	init_audio();
	TIMESTAMP("init audio");

	network_init();
	TIMESTAMP("net init");

	draw.Init();
	TIMESTAMP("draw init");

	media.load();
	TIMESTAMP("media init");

	cl->init();
	TIMESTAMP("cl init");

	sv->init();
	TIMESTAMP("sv init");

	local.init();
	TIMESTAMP("local init");

	mats.init();
	mats.load_material_file_directory("./Data/Materials/");
	TIMESTAMP("mats init");

	// debug interface
	imgui_context = ImGui::CreateContext();
	ImGui::SetCurrentContext(imgui_context);
	ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
	ImGui_ImplOpenGL3_Init();

	int startx = SDL_WINDOWPOS_UNDEFINED;
	int starty = SDL_WINDOWPOS_UNDEFINED;
	std::vector<string> buffered_commands;
	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-x") == 0) {
			startx = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-y") == 0) {
			starty = atoi(argv[++i]);
		}
		else if (strcmp(argv[i], "-w") == 0) {
			cfg.set_var("window_w", argv[++i]);
		}
		else if (strcmp(argv[i], "-h") == 0) {
			cfg.set_var("window_h", argv[++i]);
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
			buffered_commands.push_back(cmd);
		}
	}

	cfg.execute_file("vars.txt");	// load config vars
	cfg.execute_file("init.txt");	// load startup script

	SDL_SetWindowPosition(window, startx, starty);
	SDL_SetWindowSize(window, window_w->integer, window_h->integer);
	for (const auto& cmd : buffered_commands)
		cfg.execute(cmd);
	TIMESTAMP("cfg exectute");

	cfg.set_unknown_variables = false;
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

	make_move();
	if (!is_host)
		cl->SendMovesAndMessages();

	time = tick * tick_interval;
	if (is_host) {
		// build physics world now as ReadPackets() executes player commands
		build_physics_world(0.f);
		sv->ReadPackets();
		update_game_tick();
		sv->make_snapshot();
		for (int i = 0; i < sv->clients.size(); i++)
			sv->clients[i].Update();
		tick += 1;
	}

	if (!is_host) {
		cl->ReadPackets();
		cl->run_prediction();
		tick += 1;
	}
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
		}

		// update state
		switch (state)
		{
		case ENGINE_MENU:
			// later, will add menu controls, now all you can do is use the console to change state
			SDL_Delay(5);
			break;
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

			for (int i = 0; i < num_ticks && state == ENGINE_GAME; i++)
				game_update_tick();

			if(state == ENGINE_GAME)
				pre_render_update();
		}break;
		}

		draw_screen();

		static float next_print = 0;
		Config_Var* print_fps = cfg.get_var("print_fps", "0");
		if (next_print <= 0 && print_fps->integer) {
			next_print += 2.0;
			sys_print("fps %f", 1.0 / engine.frame_time);
		}
		else if (print_fps->integer)
			next_print -= engine.frame_time;

		Profilier::get_instance().end_frame_tick();

		if (pending_state) {
			pending_state = false;
			state = next_state;
		}
	}
}

Entity& Game_Engine::get_ent(int index)
{
	ASSERT(index >= 0 && index < MAX_GAME_ENTS);
	return ents[index];
}

void Game_Engine::pre_render_update()
{
	ASSERT(state == ENGINE_GAME);

		// interpolate entities for rendering
	if (!is_host)
		cl->interpolate_states();
	else {
		for (int i = 1; i < MAX_CLIENTS; i++) {
			if (!get_ent(i).active()) continue;
			auto& e = get_ent(i);
			e.using_interpolated_pos_and_rot = true;	// FIXME
			static Config_Var* use_sv_interp = cfg.get_var("sv.use_interp","1");
			if (e.last[0].used && use_sv_interp->integer) {
				e.local_sv_interpolated_pos = e.last[0].o;
				e.local_sv_interpolated_rot = e.last[0].r;
			}
			else {
				e.local_sv_interpolated_pos = e.position;
				e.local_sv_interpolated_rot = e.rotation;
			}
		}
	}

	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		Entity& ent = ents[i];
		if (!ent.active())
			continue;
		if (!ent.model || !ent.model->animations)
			continue;

		ent.anim.SetupBones();
		ent.anim.ConcatWithInvPose();
	}

	local.pm.tick(engine.frame_time);

	local.update_viewmodel();

	local.update_view();
}


int debug_console_text_callback(ImGuiInputTextCallbackData* data)
{
	Debug_Console* console = (Debug_Console*)data->UserData;
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
			cfg.execute(input_buffer);
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