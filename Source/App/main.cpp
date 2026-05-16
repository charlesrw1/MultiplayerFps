#include <SDL2/SDL.h>

#include "EngineMain.h"
#include "Framework/Util.h"

int main(int argc, char** argv) {
	install_crash_handler();
	MainConfigurationOptions options;
	return game_engine_main(options, argc, argv);
}
