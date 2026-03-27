#pragma once

#include <string>
#include <memory>

#include "ITestRunner.h"

class ITestRunner;
struct MainConfigurationOptions {
	bool no_console_print = false;
	std::string vars_file = "vars.txt";
	std::string init_file = "init.txt";
	std::string log_file = "output.log";

	std::unique_ptr<ITestRunner> pending_test_runnner;
	bool skip_swap = false;
};

int game_engine_main(MainConfigurationOptions& config_options, int argc, char** argv);