#include "Discovery.h"
#include "BridgeClient.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <json.hpp>

namespace fs = std::filesystem;

std::vector<InstanceInfo> discover_instances() {
	std::vector<InstanceInfo> out;

	std::error_code ec;
	fs::path dir = fs::temp_directory_path(ec) / "cscli_instances";
	if (ec || !fs::exists(dir))
		return out;

	for (auto& entry : fs::directory_iterator(dir, ec)) {
		if (ec)
			break;
		if (!entry.is_regular_file() || entry.path().extension() != ".json")
			continue;

		std::ifstream f(entry.path());
		if (!f)
			continue;
		try {
			nlohmann::json j;
			f >> j;
			InstanceInfo info;
			info.pid = j.value("pid", 0);
			info.port = j.value("port", 0);
			info.mode = j.value("mode", std::string());
			info.project_base = j.value("project_base", std::string());
			info.started_at = j.value("started_at", (int64_t)0);
			info.lockfile_path = entry.path().string();
			out.push_back(std::move(info));
		} catch (...) {
			// Malformed/partially-written lockfile (e.g. read mid-write) - just skip it.
		}
	}
	return out;
}

void live_check_instances(std::vector<InstanceInfo>& instances, int timeout_seconds) {
	for (auto& inst : instances) {
		inst.alive = inst.port != 0 && bridge_ping(inst.port, timeout_seconds);
		if (!inst.alive) {
			std::error_code ec;
			fs::remove(inst.lockfile_path, ec);
		}
	}
}

bool resolve_instance(int filter_port, int filter_pid, const std::string& filter_mode, int timeout_seconds,
					   bool quiet, int& out_port) {
	auto instances = discover_instances();
	live_check_instances(instances, timeout_seconds);

	std::vector<InstanceInfo> candidates;
	for (auto& inst : instances) {
		if (!inst.alive)
			continue;
		if (filter_port != 0 && inst.port != filter_port)
			continue;
		if (filter_pid != 0 && inst.pid != filter_pid)
			continue;
		if (!filter_mode.empty() && inst.mode != filter_mode)
			continue;
		candidates.push_back(inst);
	}

	if (candidates.empty()) {
		std::cerr << "cscli: no running instance found";
		if (filter_port || filter_pid || !filter_mode.empty())
			std::cerr << " matching the given --port/--pid/--mode filter";
		std::cerr << " (start the editor/game with agentbridge.enabled 1, which is the default)\n";
		return false;
	}
	if (candidates.size() > 1) {
		std::cerr << "cscli: multiple running instances found - disambiguate with --port/--pid/--mode:\n";
		for (auto& c : candidates) {
			std::cerr << "  port=" << c.port << " pid=" << c.pid << " mode=" << c.mode
					   << " project_base=" << c.project_base << "\n";
		}
		return false;
	}

	out_port = candidates[0].port;
	if (!quiet)
		std::cerr << "cscli: connecting to " << candidates[0].mode << " instance on port " << out_port << "\n";
	return true;
}
