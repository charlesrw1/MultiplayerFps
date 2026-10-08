#pragma once

#include "Commands.h"
#include <string>

int cmd_project_new(GlobalOptions& options, const std::string& name, const std::string& path);
int cmd_project_build(GlobalOptions& options, const std::string& project, const std::string& configuration,
					  const std::string& target);
int cmd_project_launch(GlobalOptions& options, const std::string& project, const std::string& configuration,
					   bool editor, bool build_first, bool wait_ready);
