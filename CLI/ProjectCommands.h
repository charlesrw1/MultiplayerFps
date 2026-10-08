#pragma once

#include "Commands.h"
#include <string>
#include <vector>

int cmd_project_new(GlobalOptions& options, const std::string& name, const std::string& path);
int cmd_project_build(GlobalOptions& options, const std::string& project, const std::string& configuration,
					  const std::string& target, bool restart_running = false);
int cmd_project_launch(GlobalOptions& options, const std::string& project, const std::string& configuration,
						   bool editor, bool build_first, bool wait_ready);
int cmd_project_test(GlobalOptions& options, const std::string& project, const std::string& configuration,
					 bool build_first, const std::string& mode, const std::vector<std::string>& patterns);
int cmd_projects(GlobalOptions& options);
int cmd_shader_cache_clear(GlobalOptions& options, const std::string& project, bool all_projects);
