#include "AgentBridge.h"
#include "Framework/Config.h"
#include "Framework/SysPrint.h"
#include "GameEnginePublic.h"
#include <unordered_map>
#include <string>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <algorithm>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

// On by default (loopback only, never reachable off-box); disable with
// `-agentbridge.enabled 0` on the command line or `agentbridge.enabled 0` in EngineVars.ini.
ConfigVar g_agentbridge_enabled("agentbridge.enabled", "1", CVAR_BOOL,
								"enable the local TCP bridge for external tools (Blender export/"
								"viewer addon, future automation/agent clients) to query the "
								"running engine (loopback only)");
ConfigVar g_agentbridge_port("agentbridge.port", "23456", CVAR_INTEGER,
							 "TCP port the local agent bridge listens on (127.0.0.1 only)", 0, 65535);

extern ConfigVar g_project_base;
extern ConfigVar g_project_name;

namespace {

struct RegisteredCommand
{
	AgentBridgeCommandDescriptor descriptor;
	AgentBridgeHandler handler;
};

std::unordered_map<std::string, RegisteredCommand>& registry() {
	static std::unordered_map<std::string, RegisteredCommand> map;
	return map;
}

uint64_t frame_index = 0;

#ifdef _WIN32
SOCKET listen_sock = INVALID_SOCKET;
SOCKET client_sock = INVALID_SOCKET;
std::string recv_buf;
bool wsa_started = false;
bool attempted_listen = false;
std::filesystem::path instance_lockfile_path;

// One small JSON file per running instance under a shared temp dir, so `cscli` can discover
// which port(s) are alive without guessing. Written once we know we're actually listening;
// removed on clean shutdown. A stale file left behind by a crash is harmless - cscli verifies
// liveness itself (connect + ping) before trusting any entry it finds here.
void write_instance_lockfile() {
	std::error_code ec;
	std::filesystem::path dir = std::filesystem::temp_directory_path(ec) / "cscli_instances";
	if (ec)
		return;
	std::filesystem::create_directories(dir, ec);
	if (ec)
		return;

	const unsigned long pid = GetCurrentProcessId();
	instance_lockfile_path = dir / (std::to_string(pid) + ".json");

	nlohmann::json info;
	info["pid"] = (int)pid;
	info["port"] = g_agentbridge_port.get_integer();
	info["mode"] = (eng && eng->is_editor_app()) ? "editor" : "game";
	info["project_base"] = g_project_base.get_string();
	info["project_name"] = g_project_name.get_string();
	info["project_file"] = get_effective_project_file();
	info["started_at"] =
		(int64_t)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
			.count();

	std::ofstream f(instance_lockfile_path, std::ios::trunc);
	if (f)
		f << info.dump();
}

void remove_instance_lockfile() {
	if (instance_lockfile_path.empty())
		return;
	std::error_code ec;
	std::filesystem::remove(instance_lockfile_path, ec);
	instance_lockfile_path.clear();
}

void set_nonblocking(SOCKET s) {
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

void close_client() {
	if (client_sock != INVALID_SOCKET) {
		closesocket(client_sock);
		client_sock = INVALID_SOCKET;
	}
	recv_buf.clear();
}

void start_listening() {
	if (attempted_listen)
		return;
	attempted_listen = true;

	const int requested_port = g_agentbridge_port.get_integer();
	for (int offset = 0; offset < 100; ++offset) {
		const int port = requested_port + offset;
		if (port > 65535)
			break;

		listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (listen_sock == INVALID_SOCKET) {
			sys_print(Error, "agent_bridge: socket() failed\n");
			return;
		}

		sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback only, by design
		addr.sin_port = htons((u_short)port);
		if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) != SOCKET_ERROR) {
			g_agentbridge_port.set_integer(port);
			break;
		}

		const int error = WSAGetLastError();
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
		if (error != WSAEADDRINUSE) {
			sys_print(Error, "agent_bridge: bind() on 127.0.0.1:%d failed\n", port);
			return;
		}
	}
	if (listen_sock == INVALID_SOCKET) {
		sys_print(Error, "agent_bridge: no available port in 127.0.0.1:%d-%d\n", requested_port, requested_port + 99);
		return;
	}
	if (g_agentbridge_port.get_integer() != requested_port)
		sys_print(Warning, "agent_bridge: port %d was in use; using 127.0.0.1:%d\n", requested_port,
			g_agentbridge_port.get_integer());
	if (listen(listen_sock, 1) == SOCKET_ERROR) {
		sys_print(Error, "agent_bridge: listen() failed\n");
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
		return;
	}
	set_nonblocking(listen_sock);
	sys_print(Info, "agent_bridge: listening on 127.0.0.1:%d\n", g_agentbridge_port.get_integer());
	write_instance_lockfile();
}
#endif

nlohmann::json make_error(const std::string& msg) {
	nlohmann::json response = nlohmann::json::object();
	response["ok"] = false;
	response["error"] = msg;
	return response;
}

nlohmann::json dispatch(const nlohmann::json& request) {
	if (!request.contains("cmd") || !request["cmd"].is_string())
		return make_error("request missing string 'cmd'");

	std::string cmd = request["cmd"];
	nlohmann::json args = request.contains("args") ? request["args"] : nlohmann::json::object();

	auto it = registry().find(cmd);
	if (it == registry().end())
		return make_error("unknown command '" + cmd + "'");

	try {
		nlohmann::json response = nlohmann::json::object();
		response["ok"] = true;
		response["result"] = it->second.handler(args);
		return response;
	}
	catch (const std::exception& e) {
		return make_error(e.what());
	}
	catch (...) {
		return make_error("unknown exception in command '" + cmd + "'");
	}
}

} // namespace

nlohmann::json AgentBridgeCommandDescriptor::to_json() const {
	nlohmann::json out;
	out["name"] = name;
	out["description"] = description;
	out["availability"] = availability;
	out["effects"] = effects;
	out["input_schema"] = input_schema;
	out["output_schema"] = output_schema;
	return out;
}

void agent_bridge_register_command(const std::string& name, AgentBridgeHandler handler) {
	AgentBridgeCommandDescriptor descriptor;
	descriptor.name = name;
	agent_bridge_register_command(std::move(descriptor), std::move(handler));
}

void agent_bridge_register_command(AgentBridgeCommandDescriptor descriptor, AgentBridgeHandler handler) {
	if (descriptor.name.empty())
		throw std::runtime_error("AgentBridge command name cannot be empty");
	const std::string name = descriptor.name;
	registry()[name] = RegisteredCommand{std::move(descriptor), std::move(handler)};
}

nlohmann::json agent_bridge_list_commands() {
	std::vector<std::string> names;
	names.reserve(registry().size());
	for (const auto& [name, command] : registry())
		names.push_back(name);
	std::sort(names.begin(), names.end());

	nlohmann::json out = nlohmann::json::array();
	for (const auto& name : names)
		out.push_back(registry().at(name).descriptor.to_json());
	return out;
}

nlohmann::json agent_bridge_describe_command(const std::string& name) {
	auto it = registry().find(name);
	if (it == registry().end())
		throw std::runtime_error("unknown command '" + name + "'");
	return it->second.descriptor.to_json();
}

uint64_t agent_bridge_frame_index() {
	return frame_index;
}

AutoAgentBridgeCommand::AutoAgentBridgeCommand(const char* name, AgentBridgeHandler handler) {
	agent_bridge_register_command(name, std::move(handler));
}

AutoAgentBridgeCommand::AutoAgentBridgeCommand(AgentBridgeCommandDescriptor descriptor, AgentBridgeHandler handler) {
	agent_bridge_register_command(std::move(descriptor), std::move(handler));
}

void agent_bridge_init() {
#ifdef _WIN32
	if (!g_agentbridge_enabled.get_bool())
		return;
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		sys_print(Error, "agent_bridge: WSAStartup failed\n");
		return;
	}
	wsa_started = true;
	start_listening();
#endif
}

void agent_bridge_update() {
	frame_index++;
#ifdef _WIN32
	if (!wsa_started || listen_sock == INVALID_SOCKET)
		return;

	if (client_sock == INVALID_SOCKET) {
		sockaddr_in from = {};
		int fromlen = sizeof(from);
		SOCKET s = accept(listen_sock, (sockaddr*)&from, &fromlen);
		if (s != INVALID_SOCKET) {
			set_nonblocking(s);
			client_sock = s;
			recv_buf.clear();
		}
		return;
	}

	char buf[4096];
	int n = recv(client_sock, buf, sizeof(buf), 0);
	if (n > 0) {
		recv_buf.append(buf, n);
		size_t nl = recv_buf.find('\n');
		if (nl != std::string::npos) {
			std::string line = recv_buf.substr(0, nl);
			nlohmann::json response;
			try {
				nlohmann::json request = nlohmann::json::parse(line);
				response = dispatch(request);
			}
			catch (const std::exception& e) {
				response = make_error(std::string("bad JSON request: ") + e.what());
			}
			std::string out = response.dump() + "\n";
			send(client_sock, out.data(), (int)out.size(), 0);
			close_client();
		}
	} else if (n == 0) {
		close_client();
	} else {
		int err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK)
			close_client();
	}
#endif
}

void agent_bridge_shutdown() {
#ifdef _WIN32
	close_client();
	remove_instance_lockfile();
	if (listen_sock != INVALID_SOCKET) {
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
	}
	if (wsa_started) {
		WSACleanup();
		wsa_started = false;
	}
#endif
}
