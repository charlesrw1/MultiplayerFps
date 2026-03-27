#include <SDL2/SDL.h>

#include "EngineMain.h"

int main(int argc, char** argv) {
	MainConfigurationOptions options;
	return game_engine_main(options,argc, argv);
}