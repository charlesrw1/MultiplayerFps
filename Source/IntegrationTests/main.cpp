// Source/IntegrationTests/main.cpp
#include <SDL2/SDL.h>
#include <string>
#include "ITestRunner.h"
#include "TestRegistry.h"
#include "GameTestRunner.h"
#include "TestGameApp.h"
#include "EngineMain.h"
#include "Framework/Config.h"

// Defined in EngineMain.cpp — set before calling game_engine_main
extern ITestRunner* g_pending_test_runner;
extern bool g_pending_skip_swap;
extern ConfigVar g_application_class;


static bool has_arg(int argc, char** argv, const char* flag) {
	for (int i = 1; i < argc; ++i)
		if (std::string(argv[i]) == flag)
			return true;
	return false;
}

static std::string get_arg(int argc, char** argv, const char* prefix) {
	std::string pre(prefix);
	for (int i = 1; i < argc; ++i) {
		std::string a(argv[i]);
		if (a.rfind(pre, 0) == 0)
			return a.substr(pre.size());
	}
	return {};
}

int main(int argc, char** argv) {
	std::string mode = get_arg(argc, argv, "--mode=");
	if (mode.empty()) {
		fprintf(stderr, "Usage: IntegrationTests.exe --mode=game|editor [--test=<glob>] [--promote] [--interactive] "
						"[--timing-assert]\n");
		return 1;
	}

	std::string test_filter = get_arg(argc, argv, "--test=");
	bool promote = has_arg(argc, argv, "--promote");
	bool interactive = has_arg(argc, argv, "--interactive");
	bool timing_assert = has_arg(argc, argv, "--timing-assert");
	bool skip_swap = !interactive;

	TestRunnerConfig cfg;
	cfg.test_filter = test_filter;
	cfg.promote = promote;
	cfg.interactive = interactive;
	cfg.timing_assert = timing_assert;


	MainConfigurationOptions mainOptions;
	mainOptions.skip_swap = skip_swap;
	mainOptions.no_console_print = true;

	if (mode == "game") {
		//g_application_class.set_string("TestGameApp");
		auto tests = TestRegistry::get_filtered(TestMode::Game, test_filter.c_str());
		mainOptions.pending_test_runnner.reset(new GameTestRunner("Game", tests, cfg));
		mainOptions.vars_file = "test_game_vars.txt";
		mainOptions.log_file = "test_game_output.log";
	} else if (mode == "editor") {
		auto tests = TestRegistry::get_filtered(TestMode::Editor, test_filter.c_str());
		mainOptions.pending_test_runnner.reset(new GameTestRunner("Editor", tests, cfg));
		mainOptions.vars_file = "test_editor_vars.txt";
		mainOptions.log_file = "test_editor_output.log";
	} else {
		fprintf(stderr, "Unknown mode: %s\n", mode.c_str());
		return 1;
	}

	//g_pending_skip_swap = skip_swap;
	return game_engine_main(mainOptions, argc, argv);
}
