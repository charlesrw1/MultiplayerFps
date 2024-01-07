#pragma once
#include "Server.h"
#include "Types.h"

using std::vector;

enum Engine_State
{
	MAINMENU,
	LOADING,
	SPAWNED,
};

class Game_Local
{
public:
	void init();
	void update_view();
	void update_viewmodel();

	Config_Var* thirdperson_camera;
	Config_Var* fov;

	PlayerState last_player_state;
	vec3 view_angles;
	MoveCommand last_command;
	View_Setup last_view;

	bool using_debug_cam = false;
	FlyCamera fly_cam;

	Item_Use_State prev_item_state = ITEM_IDLE;
	glm::vec3 viewmodel_offsets = glm::vec3(0.f);
	glm::vec3 view_recoil = glm::vec3(0.f);			// local recoil to apply to view

	float vm_reload_start = 0.f;
	float vm_reload_end = 0.f;
	float vm_recoil_start_time = 0.f;
	float vm_recoil_end_time = 0.f;
	glm::vec3 viewmodel_recoil_ofs = glm::vec3(0.f);
	glm::vec3 viewmodel_recoil_ang = glm::vec3(0.f);
};

class Client;
class Game_Engine
{
public:
	void init();
	void cleanup();
	void loop();
	void draw_screen();

	void start_map(string map, bool is_client = false);
	void exit_map();

	void build_physics_world(float time);
	void update_game_tick();
	void execute_player_move(Entity* ent, MoveCommand cmd);

	void pre_render_update();

	void bind_key(int key, string command);	// binds key to command

	void connect_to(string address);
	int player_num();
	Entity& local_player();

public:
	Client* cl;

	string mapname;
	Level* level;
	PhysicsWorld phys;
	Entity ents[MAX_GAME_ENTS];
	int num_entities;
	Engine_State engine_state;
	bool is_host;
	Game_Local local;

	double time = 0.0;			// time since program start
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate
	int tick = 0;

	SDL_Window* window;
	SDL_GLContext gl_context;
	Config_Var* window_w;
	Config_Var* window_h;
	Config_Var* window_fullscreen;
	Config_Var* host_port;
	Config_Var* mouse_sensitivity;

	bool dedicated_server = false;
	bool keys[SDL_NUM_SCANCODES];
	int mousekeys;
	bool game_focused = false;

	string* binds[SDL_NUM_SCANCODES];

	int argc;
	char** argv;
private:
	void startup_client();
	void end_client();

	void view_angle_update();
	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);
};

extern Game_Engine engine;