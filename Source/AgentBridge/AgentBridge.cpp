#include "AgentBridge.h"
#include "Framework/Config.h"
#include "Framework/SysPrint.h"
#include <unordered_map>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

// On by default (loopback only, never reachable off-box); disable with
// `-agentbridge.enabled 0` on the command line or `agentbridge.enabled 0` in vars.txt.
ConfigVar g_agentbridge_enabled("agentbridge.enabled", "1", CVAR_BOOL,
								"enable the local TCP bridge for external tools (Blender export/"
								"viewer addon, future automation/agent clients) to query the "
								"running engine (loopback only)");
ConfigVar g_agentbridge_port("agentbridge.port", "23456", CVAR_INTEGER,
							 "TCP port the local agent bridge listens on (127.0.0.1 only)", 0, 65535);

namespace {

std::unordered_map<std::string, AgentBridgeHandler>& registry() {
	static std::unordered_map<std::string, AgentBridgeHandler> map;
	return map;
}

#ifdef _WIN32
SOCKET listen_sock = INVALID_SOCKET;
SOCKET client_sock = INVALID_SOCKET;
std::string recv_buf;
bool wsa_started = false;
bool attempted_listen = false;

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

	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listen_sock == INVALID_SOCKET) {
		sys_print(Error, "agent_bridge: socket() failed\n");
		return;
	}

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // loopback only, by design
	addr.sin_port = htons((u_short)g_agentbridge_port.get_integer());

	if (bind(listen_sock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		sys_print(Error, "agent_bridge: bind() on 127.0.0.1:%d failed\n", g_agentbridge_port.get_integer());
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
		return;
	}
	if (listen(listen_sock, 1) == SOCKET_ERROR) {
		sys_print(Error, "agent_bridge: listen() failed\n");
		closesocket(listen_sock);
		listen_sock = INVALID_SOCKET;
		return;
	}
	set_nonblocking(listen_sock);
	sys_print(Info, "agent_bridge: listening on 127.0.0.1:%d\n", g_agentbridge_port.get_integer());
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
		response["result"] = it->second(args);
		return response;
	} catch (const std::exception& e) {
		return make_error(e.what());
	} catch (...) {
		return make_error("unknown exception in command '" + cmd + "'");
	}
}

} // namespace

void agent_bridge_register_command(const std::string& name, AgentBridgeHandler handler) {
	registry()[name] = std::move(handler);
}

AutoAgentBridgeCommand::AutoAgentBridgeCommand(const char* name, AgentBridgeHandler handler) {
	agent_bridge_register_command(name, std::move(handler));
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
			} catch (const std::exception& e) {
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
