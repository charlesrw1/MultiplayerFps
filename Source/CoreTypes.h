#ifndef GAMETYPE_H
#define GAMETYPE_H
#include <SDL2/SDL.h>
#include "Net.h"
#include "Physics.h"
#include "Util.h"
#include "Animation.h"

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

	// Time vals
	double game_time = 0.0;
	double frame_time = 0.0;
	double frame_remainder = 0.0;
	double frame_alpha = 0.0;

	struct InputState
	{
		bool keyboard[SDL_NUM_SCANCODES];
		int mouse_delta_x = 0;
		int mouse_delta_y = 0;
		int scroll_delta = 0;
	};
	InputState input;
};
extern Core core;


#endif // !GAMETYPE_H
