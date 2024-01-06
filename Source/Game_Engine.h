#pragma once
#include "Server.h"
#include "Types.h"

class Game_Engine
{
public:
	void init();
	void cleanup();
	void loop();
	void draw_screen();

	void update_game();

	void bind_key(int key, string command);	// binds key to command
public:
	Level* level;
	PhysicsWorld phys;
	Entity ents[MAX_GAME_ENTS];
	int num_entities;

	double time = 0.0;			// time since program start
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

	SDL_Window* window;
	SDL_GLContext gl_context;
	Config_Var* window_w;
	Config_Var* window_h;
	Config_Var* window_fullscreen;
	Config_Var* host_port;

	bool dedicated_server = false;
	bool keys[SDL_NUM_SCANCODES];
	int mousekeys;
	bool game_focused = false;

	string* binds[SDL_NUM_SCANCODES];

	int argc;
	char** argv;
private:

	void init_sdl_window();
	void key_event(SDL_Event event);
};

extern Game_Engine engine;