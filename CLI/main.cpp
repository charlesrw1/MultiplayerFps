#include "Commands.h"
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
  status                     Show mode/pid/project_base/port of the connected instance
  eval "<lua code>"          Execute Lua against the running instance's Lua state
  command [name] [args...]   Run a console command; omit name to list all; add --help to describe one
  instances                  List cscli-discoverable running instances (editor and/or game)

Global options:
  --format <human|json|tsv|ndjson>  Output format (default: human)
  --timeout <seconds>                Connect/response timeout (default: 10)
  --port <n>  --pid <n>  --mode <editor|game>
                                      Disambiguate when more than one instance is running
  --quiet                             Suppress "connecting to..." status lines
  --verbose                           On failure, also print the raw JSON error response
  -h, --help                          Show this help
  -V, --version                       Show cscli's version

Examples:
  cscli status
  cscli eval "1+1"
  cscli command give_weapon rifle 30
  cscli command give_weapon --help
  cscli command
  cscli instances
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

	if (sub == "status") {
		return cmd_status(g);
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

	std::cerr << "cscli: unknown subcommand '" << sub << "' (see 'cscli --help')\n";
	return 2;
}
