#pragma once

#include <string>
#include <memory>

#include "ITestRunner.h"

class ITestRunner;
struct MainConfigurationOptions {
	bool no_console_print = false;
	std::string vars_file = "EngineVars.ini";
	// If non-empty, only the matching `[section]` block in `vars_file` is executed.
	// Empty = legacy behaviour (run all lines, regardless of section headers).
	std::string vars_section = "app";
	std::string init_file = "init.txt";
	// Per-project .ini, applied after vars_file/vars_section. Empty = use the
	// `startup_project` cvar set by vars_file. Set from `--project <path>` on
	// the command line. Not consulted for --tests runs (game_test/editor_test
	// stay self-contained in vars_file).
	std::string project_file;
	std::string log_file = "Logs/output.log";

	std::unique_ptr<ITestRunner> pending_test_runnner;
	bool skip_swap = false;
	bool editor_mode = false;
};

int game_engine_main(MainConfigurationOptions& config_options, int argc, char** argv);