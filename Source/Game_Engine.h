#pragma once
#include "Server.h"
#include "Types.h"

using std::vector;

class Game_Media
{
public:
	void load();
	const Model* get_game_model(const char* name, int* index = nullptr);
	const Model* get_game_model_from_index(int index);

	vector<const Model*> game_models;	// non-owning list of models for entities
	Texture* blob_shadow;
};



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
	Config_Var* mouse_sensitivity;
	Config_Var* fake_movement_debug;

	//PlayerState last_player_state;
	vec3 view_angles;
	Move_Command last_command;
	View_Setup last_view;

	bool using_debug_cam = false;
	Fly_Camera fly_cam;

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

class Debug_Console
{
public:
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

struct ImGuiContext;
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

	void pre_render_update();

	void bind_key(int key, string command);	// binds key to command

	void connect_to(string address);
	int player_num();
	Entity& local_player();
	void set_tick_rate(float tick_rate);

	void fire_bullet(Entity* p, vec3 direction, vec3 origin);

	// Host functions
	Entity* new_entity();
	void free_entity(Entity* e);
	void make_client(int num);
	void client_leave(int num);
	void update_game_tick();
	void execute_player_move(int player, Move_Command command);
	void damage(Entity* inflictor, Entity* target, int amount, int flags);
public:
	Client* cl;
	Server* sv;

	string mapname;
	Level* level;
	PhysicsWorld phys;
	Entity ents[MAX_GAME_ENTS];
	int num_entities;
	Engine_State engine_state;
	bool is_host;
	Game_Local local;

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	int tick = 0;				// this is the discretized time tick
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

	ImGuiContext* imgui_context;
	SDL_Window* window;
	SDL_GLContext gl_context;
	Config_Var* window_w;
	Config_Var* window_h;
	Config_Var* window_fullscreen;
	Config_Var* host_port;
	Debug_Console console;
	bool show_console = false;

	bool dedicated_server = false;
	bool keys[SDL_NUM_SCANCODES];
	int mousekeys;
	bool game_focused = false;

	string* binds[SDL_NUM_SCANCODES];

	int argc;
	char** argv;
private:
	void view_angle_update();
	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);

	void draw_debug_interface();
};

extern Game_Media media;
extern Game_Engine engine;