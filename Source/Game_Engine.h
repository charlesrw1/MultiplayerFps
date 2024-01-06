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

class Local_State
{
public:
	vec3 view_angles;
	vector<MoveCommand> commands;
	MoveCommand last_command;

	void init();
	MoveCommand& get_command(int sequence) {
		return commands.at(sequence % commands.size());
	}
};


class Game_Engine
{
public:
	void init();
	void cleanup();
	void loop();
	void draw_screen();

	void start_map(string map);
	void exit_map();

	void build_physics_world(float time);
	void update_game_tick();
	void execute_player_move(Entity* ent, MoveCommand cmd);

	void pre_render_update();

	void bind_key(int key, string command);	// binds key to command
public:
	string mapname;
	Level* level;
	PhysicsWorld phys;
	Entity ents[MAX_GAME_ENTS];
	int num_entities;
	Engine_State engine_state;
	bool is_host;

	Local_State local;

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
	void view_angle_update();
	void make_move();
	void init_sdl_window();
	void key_event(SDL_Event event);
};

extern Game_Engine engine;