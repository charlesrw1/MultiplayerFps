#pragma once
#include <cstdint>
#include <string>
#include <vector>

// One running engine instance, as advertised by its lockfile under
// %TEMP%/cscli_instances/<pid>.json (written by AgentBridge::write_instance_lockfile). `alive` is
// unset until live_check_instances fills it in - discovery alone doesn't imply liveness, since a
// crashed process can leave its lockfile behind.
struct InstanceInfo
{
	int pid = 0;
	int port = 0;
	std::string mode; // "editor" or "game"
	std::string project_base;
	int64_t started_at = 0;
	std::string lockfile_path;
	bool alive = false;
};

// Scans the shared lockfile directory. Does not filter by liveness.
std::vector<InstanceInfo> discover_instances();

// Pings each instance (sets .alive) and best-effort deletes the lockfiles of any that don't
// answer - a stale file from a crashed process is just a discovery hint, safe to clean up.
void live_check_instances(std::vector<InstanceInfo>& instances, int timeout_seconds);

// Applies --port/--pid/--mode filters (0/empty = no filter on that field) against the live,
// discovered instance set and resolves to exactly one. On success returns true and fills
// out_port. On failure (none found, or more than one matches) prints an explanatory error to
// stderr and returns false - callers should exit with code 3 in that case.
bool resolve_instance(int filter_port, int filter_pid, const std::string& filter_mode, int timeout_seconds,
					   bool quiet, int& out_port);
