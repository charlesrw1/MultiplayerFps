#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Config.h"
#include "ParticlesPublic.h"
#include "Physics.h"
#include "Entity.h"

using glm::vec3;

class Player;
class Model;
class Game_Media
{
public:
	void load();
	Model* get_game_model(const char* name, int* index = nullptr);
	Model* get_game_model_from_index(int index);
	
	std::vector<std::string> model_manifest;
	std::vector<Model*> model_cache;
	std::vector<std::string> sound_manifest;
	Texture* blob_shadow;
};

class Debug
{
public:
	static void add_line(glm::vec3 from, glm::vec3 to, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_sphere(glm::vec3 center, float rad, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_box(glm::vec3 center, glm::vec3 size, Color32 color, float lifetime, bool fixedupdate = true);
	static void on_fixed_update_start();
	static void on_frame_begin();
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

	glm::vec3 view_angles;
	Move_Command last_command;
	View_Setup last_view;
	bool has_run_tick = false;
	bool using_debug_cam = false;
	User_Camera fly_cam;

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

	ENGINE_LVL_EDITOR,

	ENGINE_ANIMATION_EDITOR,

	ENGINE_STATE_COUNT
};

using std::string;

class Game_Engine;
struct Ent_Iterator
{
	Ent_Iterator(int start = 0, int count = 0);
	Ent_Iterator& next();
	bool finished() const;
	Entity& get();
	int get_index() const { return index; }
	void decrement_count() { summed_count--; }
private:
	int summed_count = 0;
	int index = 0;
};


class Archive;
struct ImGuiContext;
class Client;
class Server;
class Level;
class ImNodesContext;
class Game_Engine
{
public:
	Game_Engine();

	glm::ivec2 get_game_viewport_dimensions();	// either full window or sub window

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
	void client_enter_into_game();
	void exit_to_menu(const char* log_reason);
	Engine_State get_state() { return state; }

#ifdef EDITDOC
	void start_editor(const char* map);
	void start_anim_editor(const char* name);
	void close_editor();

	void enable_imgui_docking();
	void disable_imgui_docking();
#endif

	void travel_to_engine_state(Engine_State state, const char* exit_reason);

	// Host functions
	Entity* create_entity(entityclass classtype, int forceslot = -1);
	void free_entity(entityhandle handle);

	void make_client(int num);
	void client_leave(int num);
	void update_game_tick();
	void execute_player_move(int player, Move_Command command);
	void damage(Entity* inflictor, Entity* target, int amount, int flags);

	// entity accessor functions
	Entity& local_player();
	int player_num();
	Entity* get_ent(int index);
	Entity* get_ent_from_handle(entityhandle id);
	int find_by_classtype(int start, entityclass classtype);
	Ent_Iterator get_ent_start();
	Player* get_client_player(int slot);
public:
	Client* cl=nullptr;
	Server* sv=nullptr;
	string mapname;
	Level* level= nullptr;
	PhysicsWorld phys;


	bool is_host=false;
	Game_Local local;
	Game_Media media;

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	int tick = 0;				// this is the discretized time tick
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

	ImNodesContext* imgui_node_context = nullptr;
	ImGuiContext* imgui_context = nullptr;
	SDL_Window* window = nullptr;
	SDL_GLContext gl_context = nullptr;
	Auto_Config_Var window_w;
	Auto_Config_Var window_h;
	Auto_Config_Var window_fullscreen;
	Auto_Config_Var host_port;
	bool show_console = false;

	Archive* data_archive = nullptr;

	bool dedicated_server = false;
	bool keys[SDL_NUM_SCANCODES];
	bool keychanges[SDL_NUM_SCANCODES];
	int mousekeys = 0;

	bool game_focused = false;
	bool is_drawing_to_window_viewport();
	glm::ivec2 window_viewport_size = glm::ivec2(DEFAULT_WIDTH,DEFAULT_HEIGHT);

	string* binds[SDL_NUM_SCANCODES];

	int argc = 0;
	char** argv = nullptr;


private:

	int num_entities;
	vector<Entity*> ents;
	vector<char> spawnids;
	Engine_State state;

	void set_state(Engine_State state);
	void unload_current_level();

	void view_angle_update();
	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);

	void draw_debug_interface();

	void game_update_tick();

	void on_game_start();

	friend class Ent_Iterator;
};

extern Game_Engine* eng;