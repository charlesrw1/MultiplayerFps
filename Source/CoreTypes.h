#ifndef GAMETYPE_H
#define GAMETYPE_H
#include <SDL2/SDL.h>
#include "Net.h"
#include "Util.h"

const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;
const int MAX_GAME_ENTS = 256;

class Core
{
public:
	
	SDL_Window* window = nullptr;
	SDL_GLContext context = nullptr;
	int vid_width = DEFAULT_WIDTH;
	int vid_height = DEFAULT_HEIGHT;

	double time = 0.0;			// time since program start
	double frame_time = 0.0;	// total frame time of program
	double frame_remainder = 0.0;	// frame time accumulator
	double tick_interval = 0.0;	// 1/tick_rate

	struct InputState
	{
		bool keyboard[SDL_NUM_SCANCODES];
		int mouse_delta_x = 0;
		int mouse_delta_y = 0;
		int scroll_delta = 0;
	};
	bool mouse_grabbed = false;
	InputState input;
};
extern Core core;


#endif // !GAMETYPE_H
