#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "EngineMain.h"
#include "Framework/Util.h"

int main(int argc, char** argv) {
	install_crash_handler();
	MainConfigurationOptions options;
	return game_engine_main(options, argc, argv);
}
