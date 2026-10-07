#pragma once
#include <string>
#include <vector>

// Flags that apply across every subcommand (see docs/tooling/cscli.md for the full contract).
struct GlobalOptions
{
	std::string format = "human"; // human | json | tsv | ndjson
	int timeout = 10;			  // seconds, bounds connect + response read
	bool quiet = false;			  // suppress "connecting to..." style status lines
	bool verbose = false;		  // on failure, also print the raw JSON error response
	int port_filter = 0;		  // 0 = no filter
	int pid_filter = 0;			  // 0 = no filter
	std::string mode_filter;	  // "" = no filter, else "editor" or "game"
	std::string project_filter;	  // project name, project file, or project base
};

// Each returns the process exit code (see the table in docs/tooling/cscli.md):
// 0 success, 1 command-level failure, 2 usage error, 3 couldn't reach an instance.
int cmd_status(GlobalOptions& g);
int cmd_wait_ready(GlobalOptions& g, int wait_timeout_seconds);
int cmd_eval(GlobalOptions& g, const std::string& code);
int cmd_call(GlobalOptions& g, const std::string& name, const std::string& json_args);
int cmd_tools(GlobalOptions& g, const std::string& name);
// name empty -> list all commands/cvars. name + help -> describe. name + (help==false) -> run,
// joining name and args into one command line exactly like typing it at the console.
int cmd_command(GlobalOptions& g, const std::string& name, bool help, const std::vector<std::string>& args);
int cmd_instances(GlobalOptions& g);
// Tail of the connected instance's engine log (Logs/output.log or Logs/test_<mode>_output.log).
int cmd_log(GlobalOptions& g, int lines);
