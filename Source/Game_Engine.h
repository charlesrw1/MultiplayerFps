#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Config.h"
#include "Particles.h"
#include "Physics.h"

class Model;
class Game_Media
{
public:
	void load();
	const Model* get_game_model(const char* name, int* index = nullptr);
	const Model* get_game_model_from_index(int index);
	
	std::vector<std::string> model_manifest;
	std::vector<Model*> model_cache;
	std::vector<std::string> sound_manifest;
	Texture* blob_shadow;
};



class Game_Local
{
public:
	Game_Local();

	void init();
	void update_view();
	void update_viewmodel();

	Auto_Config_Var thirdperson_camera;
	Auto_Config_Var fov;
	Auto_Config_Var mouse_sensitivity;
	Auto_Config_Var fake_movement_debug;

	Particle_Manager pm;
	vec3 view_angles;
	Move_Command last_command;
	View_Setup last_view;
	bool has_run_tick = false;
	bool using_debug_cam = false;
	Fly_Camera fly_cam;

	int prev_item_state = ITEM_IDLE;
	glm::vec3 viewmodel_offsets = glm::vec3(0.f);
	glm::vec3 view_recoil = glm::vec3(0.f);			// local recoil to apply to view

	Animator viewmodel_animator;
	Model* viewmodel=nullptr;

	glm::vec3 vm_offset = glm::vec3(0.f,-2.9f,0.f);
	glm::vec3 vm_scale = glm::vec3(1.f);
	float vm_reload_start = 0.f;
	float vm_reload_end = 0.f;
	float vm_recoil_start_time = 0.f;
	float vm_recoil_end_time = 0.f;
	glm::vec3 viewmodel_recoil_ofs = glm::vec3(0.f);
	glm::vec3 viewmodel_recoil_ang = glm::vec3(0.f);
};

enum Engine_State
{
	ENGINE_MENU,
	ENGINE_LOADING,
	ENGINE_GAME,
};

using std::string;

class Archive;
struct ImGuiContext;
class Client;
class Server;
class Level;
class Game_Engine
{
public:
	Game_Engine();

	void init();
	void cleanup();

	void loop();
	void draw_screen();
	void pre_render_update();

	void bind_key(int key, string command);	// binds key to command

	// game functions
	void fire_bullet(Entity* p, vec3 direction, vec3 origin);
	void build_physics_world(float time);

	// state relevant functions
	void connect_to(string address);
	void set_tick_rate(float tick_rate);
	bool start_map(string map, bool is_client = false);
	void set_state(Engine_State state);
	void exit_map();

	// Host functions
	Entity* new_entity();
	void free_entity(Entity* e);
	void make_client(int num);
	void client_leave(int num);
	void update_game_tick();
	void execute_player_move(int player, Move_Command command);
	void damage(Entity* inflictor, Entity* target, int amount, int flags);

	// entity accessor functions
	Entity& local_player();
	int player_num();
	Entity& get_ent(int index);
	int find_by_classname(int start, const char* classname);
public:
	Client* cl;
	Server* sv;

	string mapname;
	Level* level;
	PhysicsWorld phys;
	Entity ents[MAX_GAME_ENTS];
	int num_entities;
	Engine_State state;
	bool pending_state = false;
	Engine_State nextstate;
	bool is_host;
	Game_Local local;
	Game_Media media;

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	int tick = 0;				// this is the discretized time tick
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

	ImGuiContext* imgui_context;
	SDL_Window* window;
	SDL_GLContext gl_context;
	Auto_Config_Var window_w;
	Auto_Config_Var window_h;
	Auto_Config_Var window_fullscreen;
	Auto_Config_Var host_port;
	bool show_console = false;

	Archive* data_archive = nullptr;

	bool dedicated_server = false;
	bool keys[SDL_NUM_SCANCODES];
	bool keychanges[SDL_NUM_SCANCODES];
	int mousekeys;
	bool game_focused = false;

	string* binds[SDL_NUM_SCANCODES];

	int argc;
	char** argv;
private:
	Engine_State next_state;

	void view_angle_update();
	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);

	void draw_debug_interface();

	void game_update_tick();

	void on_game_start();
};

extern Game_Engine* eng;