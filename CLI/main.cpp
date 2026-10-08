#include "Commands.h"
#include "ProjectCommands.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void print_top_help() {
	std::cout <<
		R"(cscli - CLI for driving a running CsRemake editor/game instance over AgentBridge

Usage: cscli [global options] <subcommand> [args...]

Subcommands:
  new <name> --path <dir>    Create an external C++ game project
  build [options]            Build an external game project
  editor [options]           Build and launch the project's editor
  run [options]              Build and launch the project as a game
  test [options]             Build and run the project's editor tests
  projects                   List projects created by cscli on this machine
  cache clear [options]      Clear external-project shader caches
  status                     Show mode/pid/project_base/port of the connected instance
  wait-ready                 Wait until the selected instance reports ready
  tools [name]               List typed bridge tools or describe one
  call <name> [--args JSON]  Invoke any typed bridge tool
  eval "<lua code>"          Execute Lua against the running instance's Lua state
  command [name] [args...]   Run a console command; omit name to list all; add --help to describe one
  instances                  List cscli-discoverable running instances (editor and/or game)
  log [--lines N]            Print the tail of the connected instance's engine log (default 200)

Global options:
  --format <human|json|tsv|ndjson>  Output format (default: human)
  --timeout <seconds>                Connect/response timeout (default: 10)
  --port <n>  --pid <n>  --mode <editor|game>  --project <name|file|data-dir>
                                      Disambiguate when more than one instance is running
  --quiet                             Suppress "connecting to..." status lines
  --verbose                           On failure, also print the raw JSON error response
  -h, --help                          Show this help
  -V, --version                       Show cscli's version

Examples:
  cscli new Asteroids --path D:/Gamedev/Asteroids
  cscli build --project D:/Gamedev/Asteroids
  cscli editor --project D:/Gamedev/Asteroids --wait-ready
  cscli status
  cscli wait-ready --wait-timeout 30
  cscli tools scene_hierarchy
  cscli call scene_hierarchy
  cscli call capture_frame --args "{\"path\":\"TestFiles/screenshots/live.png\"}"
  cscli eval "1+1"
  cscli command give_weapon rifle 30
  cscli command give_weapon --help
  cscli command
  cscli instances
  cscli log --lines 50
)";
}

} // namespace

int main(int argc, char** argv) {
	std::vector<std::string> args(argv + 1, argv + argc);

	GlobalOptions g;
	std::vector<std::string> rest;

	// Global options only make sense before the subcommand (per the usage line below); stop at the
	// first token that isn't one of them so a subcommand's own flags - e.g. `command foo --help`,
	// which describe_command's --help means something different from the top-level one - reach the
	// subcommand parser untouched instead of being swallowed here.
	size_t i = 0;
	for (; i < args.size(); i++) {
		const std::string& a = args[i];
		auto next = [&]() -> std::string { return (i + 1 < args.size()) ? args[++i] : std::string(); };

		if (a == "--format") {
			g.format = next();
			if (g.format != "human" && g.format != "json" && g.format != "tsv" && g.format != "ndjson") {
				std::cerr << "cscli: unknown --format '" << g.format << "' (expected human|json|tsv|ndjson)\n";
				return 2;
			}
		} else if (a == "--timeout") {
			g.timeout = std::atoi(next().c_str());
		} else if (a == "--quiet") {
			g.quiet = true;
		} else if (a == "--verbose") {
			g.verbose = true;
		} else if (a == "--port") {
			g.port_filter = std::atoi(next().c_str());
		} else if (a == "--pid") {
			g.pid_filter = std::atoi(next().c_str());
		} else if (a == "--mode") {
			g.mode_filter = next();
		} else if (a == "--project") {
			g.project_filter = next();
		} else if (a == "-V" || a == "--version") {
			std::cout << "cscli 0.1.0\n";
			return 0;
		} else if (a == "-h" || a == "--help") {
			print_top_help();
			return 0;
		} else {
			break;
		}
	}
	for (; i < args.size(); i++)
		rest.push_back(args[i]);

	if (rest.empty()) {
		print_top_help();
		return 2;
	}

	const std::string sub = rest[0];
	const std::vector<std::string> sub_args(rest.begin() + 1, rest.end());

	if (sub == "new") {
		if (sub_args.empty()) {
			std::cerr << "usage: cscli new <name> [--path directory]\n";
			return 2;
		}
		std::string path;
		for (size_t j = 1; j < sub_args.size(); j++) {
			if (sub_args[j] == "--path" && j + 1 < sub_args.size())
				path = sub_args[++j];
			else {
				std::cerr << "usage: cscli new <name> [--path directory]\n";
				return 2;
			}
		}
		return cmd_project_new(g, sub_args[0], path);
	}

	if (sub == "build") {
		std::string project = g.project_filter;
		std::string configuration = "Debug";
		std::string target = "all";
		bool restart_running = false;
		for (size_t j = 0; j < sub_args.size(); j++) {
			if (sub_args[j] == "--project" && j + 1 < sub_args.size())
				project = sub_args[++j];
			else if (sub_args[j] == "--configuration" && j + 1 < sub_args.size())
				configuration = sub_args[++j];
			else if (sub_args[j] == "--target" && j + 1 < sub_args.size())
				target = sub_args[++j];
			else if (sub_args[j] == "--restart-running")
				restart_running = true;
			else {
				std::cerr << "usage: cscli build [--project path] [--configuration name] [--target editor|game|test|all] [--restart-running]\n";
				return 2;
			}
		}
		return cmd_project_build(g, project, configuration, target, restart_running);
	}

	if (sub == "editor" || sub == "run") {
		std::string project = g.project_filter;
		std::string configuration = "Debug";
		bool build_first = true;
		bool wait_ready = false;
		for (size_t j = 0; j < sub_args.size(); j++) {
			if (sub_args[j] == "--project" && j + 1 < sub_args.size())
				project = sub_args[++j];
			else if (sub_args[j] == "--configuration" && j + 1 < sub_args.size())
				configuration = sub_args[++j];
			else if (sub_args[j] == "--no-build")
				build_first = false;
			else if (sub_args[j] == "--wait-ready")
				wait_ready = true;
			else {
				std::cerr << "usage: cscli " << sub
						  << " [--project path] [--configuration name] [--no-build] [--wait-ready]\n";
				return 2;
			}
		}
		return cmd_project_launch(g, project, configuration, sub == "editor", build_first, wait_ready);
	}

	if (sub == "test") {
		std::string project = g.project_filter;
		std::string configuration = "Debug";
		bool build_first = true;
		std::string mode = "editor";
		std::vector<std::string> patterns;
		for (size_t j = 0; j < sub_args.size(); j++) {
			if (sub_args[j] == "--project" && j + 1 < sub_args.size())
				project = sub_args[++j];
			else if (sub_args[j] == "--configuration" && j + 1 < sub_args.size())
				configuration = sub_args[++j];
			else if (sub_args[j] == "--mode" && j + 1 < sub_args.size())
				mode = sub_args[++j];
			else if (sub_args[j] == "--no-build")
				build_first = false;
			else if (sub_args[j] == "--pattern" && j + 1 < sub_args.size())
				patterns.push_back(sub_args[++j]);
			else {
				std::cerr << "usage: cscli test [--project path] [--configuration name] [--mode editor|game] [--no-build] [--pattern glob]\n";
				return 2;
			}
		}
		return cmd_project_test(g, project, configuration, build_first, mode, patterns);
	}

	if (sub == "projects") {
		if (!sub_args.empty()) {
			std::cerr << "usage: cscli projects\n";
			return 2;
		}
		return cmd_projects(g);
	}

	if (sub == "cache") {
		if (sub_args.empty() || sub_args[0] != "clear") {
			std::cerr << "usage: cscli cache clear [--project path|--all]\n";
			return 2;
		}
		std::string project = g.project_filter;
		bool all_projects = false;
		for (size_t j = 1; j < sub_args.size(); ++j) {
			if (sub_args[j] == "--project" && j + 1 < sub_args.size())
				project = sub_args[++j];
			else if (sub_args[j] == "--all")
				all_projects = true;
			else {
				std::cerr << "usage: cscli cache clear [--project path|--all]\n";
				return 2;
			}
		}
		if (all_projects && !project.empty()) {
			std::cerr << "cscli: use either --project or --all\n";
			return 2;
		}
		return cmd_shader_cache_clear(g, project, all_projects);
	}

	if (sub == "status") {
		return cmd_status(g);
	}

	if (sub == "wait-ready") {
		int wait_timeout = 30;
		for (size_t j = 0; j < sub_args.size(); j++) {
			if (sub_args[j] == "--wait-timeout" && j + 1 < sub_args.size())
				wait_timeout = std::atoi(sub_args[++j].c_str());
			else {
				std::cerr << "usage: cscli wait-ready [--wait-timeout seconds]\n";
				return 2;
			}
		}
		if (wait_timeout < 1) {
			std::cerr << "cscli: --wait-timeout must be at least 1 second\n";
			return 2;
		}
		return cmd_wait_ready(g, wait_timeout);
	}

	if (sub == "tools") {
		if (sub_args.size() > 1) {
			std::cerr << "usage: cscli tools [name]\n";
			return 2;
		}
		return cmd_tools(g, sub_args.empty() ? std::string() : sub_args[0]);
	}

	if (sub == "call") {
		if (sub_args.empty()) {
			std::cerr << "usage: cscli call <name> [--args <json-object>]\n";
			return 2;
		}
		const std::string name = sub_args[0];
		std::string json_args;
		for (size_t j = 1; j < sub_args.size(); j++) {
			if (sub_args[j] == "--args" && j + 1 < sub_args.size())
				json_args = sub_args[++j];
			else {
				std::cerr << "usage: cscli call <name> [--args <json-object>]\n";
				return 2;
			}
		}
		return cmd_call(g, name, json_args);
	}

	if (sub == "eval") {
		if (sub_args.empty()) {
			std::cerr << "usage: cscli eval \"<lua code>\"\n";
			return 2;
		}
		return cmd_eval(g, sub_args[0]);
	}

	if (sub == "command") {
		bool help = false;
		std::string name;
		std::vector<std::string> cmd_args;
		for (auto& a : sub_args) {
			if (a == "--help" || a == "-h")
				help = true;
			else if (name.empty())
				name = a;
			else
				cmd_args.push_back(a);
		}
		return cmd_command(g, name, help, cmd_args);
	}

	if (sub == "instances") {
		return cmd_instances(g);
	}

	if (sub == "log") {
		int lines = 200;
		for (size_t j = 0; j < sub_args.size(); j++) {
			if ((sub_args[j] == "--lines" || sub_args[j] == "-n") && j + 1 < sub_args.size())
				lines = std::atoi(sub_args[++j].c_str());
		}
		return cmd_log(g, lines);
	}

	std::cerr << "cscli: unknown subcommand '" << sub << "' (see 'cscli --help')\n";
	return 2;
}
