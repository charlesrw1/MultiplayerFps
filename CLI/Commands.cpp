#include "Commands.h"
#include "BridgeClient.h"
#include "Discovery.h"
#include <functional>
#include <iostream>
#include <json.hpp>
#include <chrono>
#include <thread>

namespace {

// Shared tail end of every subcommand: applies the exit-code/stdout-vs-stderr contract in one
// place so each subcommand only has to describe its own success-path formatting.
//   - transport failure (couldn't connect at all)      -> exit 3
//   - bridge replied ok:false                          -> exit 1
//   - ok:true but force_error (e.g. run_command's       -> exit 1
//     had_error) says the command itself failed
//   - otherwise                                        -> exit 0
// --format json always dumps the raw bridge response (or a synthesized one for transport
// failures) to stdout, success or failure, so scripts can parse it unconditionally.
int emit(const BridgeResult& r, const GlobalOptions& g, bool force_error,
		 const std::function<void(const nlohmann::json&)>& print_human) {
	if (!r.connected) {
		if (g.format == "json") {
			nlohmann::json j;
			j["ok"] = false;
			j["error"] = r.connect_error;
			std::cout << j.dump() << "\n";
		} else {
			std::cerr << "cscli: " << r.connect_error << "\n";
		}
		return 3;
	}

	const bool ok = r.response.value("ok", false);
	if (g.format == "json")
		std::cout << r.response.dump() << "\n";

	if (!ok) {
		if (g.format != "json")
			std::cerr << "cscli: " << r.response.value("error", std::string("unknown error")) << "\n";
		return 1;
	}

	if (g.format != "json")
		print_human(r.response.value("result", nlohmann::json(nullptr)));

	return force_error ? 1 : 0;
}

} // namespace

int cmd_status(GlobalOptions& g) {
	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;

	BridgeResult r = bridge_call(port, "status", nlohmann::json::object(), g.timeout);
	return emit(r, g, false, [](const nlohmann::json& result) {
		std::cout << "mode:         " << result.value("mode", std::string()) << "\n";
		std::cout << "pid:          " << result.value("pid", 0) << "\n";
		std::cout << "project_base: " << result.value("project_base", std::string()) << "\n";
		std::cout << "project:      " << result.value("project_name", std::string()) << "\n";
		std::cout << "project_file: " << result.value("project_file", std::string()) << "\n";
		std::cout << "port:         " << result.value("port", 0) << "\n";
		std::cout << "state:        " << result.value("state", std::string()) << "\n";
		std::cout << "frame:        " << result.value("frame", 0ull) << "\n";
		if (result.contains("level") && result["level"].is_string())
			std::cout << "level:        " << result["level"].get<std::string>() << "\n";
		if (result.contains("document") && result["document"].is_string())
			std::cout << "document:     " << result["document"].get<std::string>()
					  << (result.value("document_dirty", false) ? " (dirty)" : "") << "\n";
	});
}

int cmd_wait_ready(GlobalOptions& g, int wait_timeout_seconds) {
	int port = 0;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(wait_timeout_seconds);
	while (std::chrono::steady_clock::now() < deadline &&
		   !resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, 1, true, port, false))
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	if (port == 0) {
		BridgeResult timeout;
		timeout.connected = true;
		timeout.response = {{"ok", false}, {"error", "timed out waiting for a matching engine instance"}};
		return emit(timeout, g, false, [](const nlohmann::json&) {});
	}
	if (!g.quiet)
		std::cerr << "cscli: connected to instance on port " << port << "\n";

	BridgeResult latest;
	do {
		latest = bridge_call(port, "status", nlohmann::json::object(), g.timeout);
		if (latest.connected && latest.response.value("ok", false)) {
			const auto result = latest.response.value("result", nlohmann::json::object());
			if (result.value("ready", false))
				return emit(latest, g, false, [](const nlohmann::json& ready) {
					std::cout << "ready: " << ready.value("mode", std::string())
							  << " project=" << ready.value("project_name", std::string())
							  << " frame=" << ready.value("frame", 0ull) << "\n";
				});
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	} while (std::chrono::steady_clock::now() < deadline);

	BridgeResult timeout;
	timeout.connected = true;
	timeout.response = {{"ok", false}, {"error", "timed out waiting for the engine to become ready"}};
	return emit(timeout, g, false, [](const nlohmann::json&) {});
}

int cmd_eval(GlobalOptions& g, const std::string& code) {
	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;

	nlohmann::json args;
	args["code"] = code;
	BridgeResult r = bridge_call(port, "eval_lua", args, g.timeout);
	return emit(r, g, false, [](const nlohmann::json& result) {
		nlohmann::json value = result.value("result", nlohmann::json(nullptr));
		if (value.is_string())
			std::cout << value.get<std::string>() << "\n";
		else if (value.is_null())
			std::cout << "nil\n";
		else
			std::cout << value.dump(2) << "\n";
	});
}

int cmd_call(GlobalOptions& g, const std::string& name, const std::string& json_args) {
	nlohmann::json args = nlohmann::json::object();
	if (!json_args.empty()) {
		try {
			args = nlohmann::json::parse(json_args);
		}
		catch (const std::exception& e) {
			std::cerr << "cscli: invalid --args JSON: " << e.what() << "\n";
			return 2;
		}
		if (!args.is_object()) {
			std::cerr << "cscli: --args must be a JSON object\n";
			return 2;
		}
	}

	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;
	BridgeResult r = bridge_call(port, name, args, g.timeout);
	return emit(r, g, false, [](const nlohmann::json& result) {
		if (result.is_string())
			std::cout << result.get<std::string>() << "\n";
		else
			std::cout << result.dump(2) << "\n";
	});
}

int cmd_tools(GlobalOptions& g, const std::string& name) {
	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;
	nlohmann::json args = nlohmann::json::object();
	const std::string command = name.empty() ? "list_tools" : "describe_tool";
	if (!name.empty())
		args["name"] = name;
	BridgeResult r = bridge_call(port, command, args, g.timeout);
	return emit(r, g, false, [&](const nlohmann::json& result) {
		nlohmann::json commands =
			name.empty() ? result.value("commands", nlohmann::json::array()) : nlohmann::json::array({result});
		for (const auto& item : commands) {
			if (g.format == "tsv") {
				std::cout << item.value("name", std::string()) << "\t" << item.value("availability", std::string())
						  << "\t" << item.value("effects", std::string()) << "\t"
						  << item.value("description", std::string()) << "\n";
			} else if (g.format == "ndjson") {
				std::cout << item.dump() << "\n";
			} else {
				std::cout << item.value("name", std::string()) << " [" << item.value("availability", std::string())
						  << ", " << item.value("effects", std::string()) << "]";
				const std::string description = item.value("description", std::string());
				if (!description.empty())
					std::cout << " - " << description;
				std::cout << "\n";
				if (!name.empty())
					std::cout << "  input:  " << item.value("input_schema", nlohmann::json::object()).dump() << "\n"
							  << "  output: " << item.value("output_schema", nlohmann::json::object()).dump() << "\n";
			}
		}
	});
}

int cmd_command(GlobalOptions& g, const std::string& name, bool help, const std::vector<std::string>& args) {
	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;

	if (name.empty()) {
		nlohmann::json a;
		a["filter"] = "";
		BridgeResult r = bridge_call(port, "list_commands", a, g.timeout);
		return emit(r, g, false, [&](const nlohmann::json& result) {
			nlohmann::json commands = result.value("commands", nlohmann::json::array());
			for (auto& c : commands) {
				const std::string cname = c.value("name", std::string());
				const std::string desc = c.value("description", std::string());
				const bool is_command = c.value("is_command", false);
				if (g.format == "tsv") {
					std::cout << cname << "\t" << (is_command ? "command" : "var") << "\t" << desc << "\n";
				} else if (g.format == "ndjson") {
					std::cout << c.dump() << "\n";
				} else {
					std::cout << (is_command ? "[cmd] " : "[var] ") << cname;
					if (!desc.empty())
						std::cout << " - " << desc;
					std::cout << "\n";
				}
			}
		});
	}

	if (help) {
		nlohmann::json a;
		a["name"] = name;
		BridgeResult r = bridge_call(port, "describe_command", a, g.timeout);
		return emit(r, g, false, [](const nlohmann::json& result) {
			std::cout << result.value("name", std::string()) << "\n";
			const std::string desc = result.value("description", std::string());
			std::cout << "  " << (desc.empty() ? "(no documentation)" : desc) << "\n";
			const std::string usage = result.value("usage", std::string());
			if (!usage.empty())
				std::cout << "usage: " << usage << "\n";
		});
	}

	std::string joined = name;
	for (auto& a : args) {
		joined += ' ';
		joined += a;
	}

	nlohmann::json a;
	a["command"] = joined;
	BridgeResult r = bridge_call(port, "run_command", a, g.timeout);
	const bool had_error = r.connected && r.response.value("ok", false) &&
						   r.response.value("result", nlohmann::json::object()).value("had_error", false);
	return emit(r, g, had_error,
				[](const nlohmann::json& result) { std::cout << result.value("output", std::string()); });
}

int cmd_log(GlobalOptions& g, int lines) {
	int port = 0;
	if (!resolve_instance(g.port_filter, g.pid_filter, g.mode_filter, g.project_filter, g.timeout, g.quiet, port))
		return 3;

	nlohmann::json a;
	a["lines"] = lines;
	BridgeResult r = bridge_call(port, "get_log", a, g.timeout);
	return emit(r, g, false, [&](const nlohmann::json& result) {
		nlohmann::json log_lines = result.value("lines", nlohmann::json::array());
		for (auto& line : log_lines)
			std::cout << line.get<std::string>() << "\n";
	});
}

int cmd_instances(GlobalOptions& g) {
	auto instances = discover_instances();
	live_check_instances(instances, g.timeout);

	if (g.format == "json") {
		nlohmann::json arr = nlohmann::json::array();
		for (auto& i : instances) {
			nlohmann::json e;
			e["pid"] = i.pid;
			e["port"] = i.port;
			e["mode"] = i.mode;
			e["project_base"] = i.project_base;
			e["project_name"] = i.project_name;
			e["project_file"] = i.project_file;
			e["alive"] = i.alive;
			arr.push_back(e);
		}
		std::cout << arr.dump() << "\n";
		return 0;
	}

	if (instances.empty()) {
		if (g.format != "tsv" && g.format != "ndjson")
			std::cout << "no cscli-discoverable instances found\n";
		return 0;
	}

	for (auto& i : instances) {
		if (g.format == "tsv") {
			std::cout << i.port << "\t" << i.pid << "\t" << i.mode << "\t" << (i.alive ? "alive" : "dead") << "\t"
					  << i.project_base << "\n";
		} else if (g.format == "ndjson") {
			nlohmann::json e;
			e["pid"] = i.pid;
			e["port"] = i.port;
			e["mode"] = i.mode;
			e["alive"] = i.alive;
			e["project_base"] = i.project_base;
			std::cout << e.dump() << "\n";
		} else {
			std::cout << (i.alive ? "[alive] " : "[dead]  ") << "port=" << i.port << " pid=" << i.pid
					  << " mode=" << i.mode << " project=" << i.project_name << " project_base=" << i.project_base
					  << "\n";
		}
	}
	return 0;
}
