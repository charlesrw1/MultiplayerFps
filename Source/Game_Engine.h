#pragma once
#include <SDL2/SDL.h>
#include <string>
#include "Types.h"
#include "Framework/Config.h"
#include "ParticlesPublic.h"
#include "Physics.h"
#include "Entity.h"

#include "IEditorTool.h"

#include "Debug.h"

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
};

extern Auto_Config_Var g_thirdperson;
extern Auto_Config_Var g_fov;
extern Auto_Config_Var g_mousesens;
extern Auto_Config_Var g_fakemovedebug;
extern Auto_Config_Var g_drawdebugmenu;
extern Auto_Config_Var g_drawimguidemo;



enum class Engine_State
{
	Idle,		// main menu or in tool state
	Loading,	// loading next level
	Game,		// in game state
};

// what tool is active
enum class Eng_Tool_state
{
	None,
	Level,
	Animgraph,
	// Particle
	// Material
};


using std::string;

class Game_Engine;
struct Ent_Iterator
{
	Ent_Iterator(int start = 0, int count = 0);
	Ent_Iterator& next();
	bool finished() const;
	Entity& get() const;
	int get_index() const { return index; }
	void decrement_count() { summed_count--; }
	bool finish_at(const Ent_Iterator& other) const {
		return index != -1 && other.index <= index;
	}
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

	bool is_in_game() const {
		return state == Engine_State::Game;
	}
	Engine_State get_state() const { return state; }
	Eng_Tool_state get_tool_state() const { return tool_state; }
	bool is_in_an_editor_state() { return get_tool_state() != Eng_Tool_state::None; }
	IEditorTool* get_current_tool();

	void change_editor_state(Eng_Tool_state tool, const char* file = "");

	void queue_load_map(string nextmap);

	void execute_map_change();
	void stop_game();
	void spawn_starting_players(bool initial);
	void populate_map();
	void leave_current_game();

	bool game_draw_screen();


	// Host functions

	Entity* create_entity(const char* classname, int forceslot = -1);
	void free_entity(entityhandle handle);
	void make_client(int num);
	void client_leave(int num);
	void update_game_tick();
	// returns if there was anything to draw

	// entity accessor functions
	Entity& local_player();
	int player_num();
	Player* get_local_player();
	Entity* get_ent(int index);
	Entity* get_ent_from_handle(entityhandle id);
	int find_by_classtype(int start, StringName classtype);
	Player* get_client_player(int slot);
public:
	bool map_spawned() { return level != nullptr; }

	Client* cl=nullptr;
	Server* sv=nullptr;
	string mapname;
	Level* level= nullptr;
	PhysicsWorld phys;

	Game_Media media;

	double time = 0.0;			// this is essentially tick*tick_interval +- smoothing on client
	int tick = 0;				// this is the discretized time tick
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

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

	bool is_host() const { return true; }

private:

	int num_entities;
	vector<Entity*> ents;
	vector<char> spawnids;

	Engine_State state = Engine_State::Idle;
	Eng_Tool_state tool_state = Eng_Tool_state::None;

	bool is_hosting_game = false;

	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);

	void draw_any_imgui_interfaces();

	void game_update_tick();
	
	void spawn_player(int slot);

	friend class Ent_Iterator;
};

extern Game_Engine* eng;