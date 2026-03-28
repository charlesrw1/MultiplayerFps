// winsock2.h must come before any Windows headers
#pragma comment(lib, "ws2_32.lib")
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

// ClassBase.h must be included before ClassTypeInfo.h (circular include ordering)
#include "Framework/ClassBase.h"
#include "AgentREPL.h"
#include "Framework/Config.h" // Cmd_Manager
#include "Scripting/ScriptManager.h"
#include "Logging.h"

#include <string>
#include <vector>
#include <chrono>

extern "C"
{
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}

AgentREPL* AgentREPL::inst = nullptr;

// ---------------------------------------------------------------------------
// Socket log sink — forwards Error/Warning/Info/LtConsoleCommand to the client
// ---------------------------------------------------------------------------
class SocketLogSink : public LogSink
{
public:
	SOCKET sock = INVALID_SOCKET;

	void log(LogType type, const std::string& message) override {
		if (sock == INVALID_SOCKET)
			return;
		if (type == LogType::Debug)
			return; // too noisy
		send_raw(message);
	}

	void send_raw(const std::string& s) {
		if (sock == INVALID_SOCKET || s.empty())
			return;
		::send(sock, s.c_str(), (int)s.size(), 0);
	}
};

// ---------------------------------------------------------------------------
// Lua print capture
// ---------------------------------------------------------------------------
static SocketLogSink* s_active_sink = nullptr;
static const char* LUA_ORIG_PRINT_KEY = "_agent_repl_orig_print";

static int lua_print_to_socket(lua_State* L) {
	if (!s_active_sink)
		return 0;
	int n = lua_gettop(L);
	std::string out;
	for (int i = 1; i <= n; i++) {
		if (i > 1)
			out += '\t';
		size_t len = 0;
		const char* s = luaL_tolstring(L, i, &len);
		out.append(s, len);
		lua_pop(L, 1);
	}
	out += '\n';
	s_active_sink->send_raw(out);
	return 0;
}

static void redirect_lua_print(lua_State* L) {
	lua_getglobal(L, "print");
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_ORIG_PRINT_KEY);
	lua_pushcfunction(L, lua_print_to_socket);
	lua_setglobal(L, "print");
}

static void restore_lua_print(lua_State* L) {
	lua_getfield(L, LUA_REGISTRYINDEX, LUA_ORIG_PRINT_KEY);
	lua_setglobal(L, "print");
	lua_pushnil(L);
	lua_setfield(L, LUA_REGISTRYINDEX, LUA_ORIG_PRINT_KEY);
}

// ---------------------------------------------------------------------------
// Impl
// ---------------------------------------------------------------------------
struct AgentREPL::Impl
{
	SOCKET listen_sock = INVALID_SOCKET;
	SOCKET client_sock = INVALID_SOCKET;
	std::shared_ptr<SocketLogSink> sink;

	// Line buffer for partial reads
	std::string line_buf;

	// Wait state for wait_ticks / wait_time while blocked
	int wait_ticks_remaining = 0;
	bool waiting_ticks = false;
	double wait_time_end = 0.0;
	bool waiting_time = false;
	// Re-enter blocked loop after wait expires
	bool reblock_pending = false;
	// Set when agent sends "resume" at the top level (resumes debug_break coroutine)
	bool resume_requested = false;

	bool wsa_initialized = false;

	Impl() {
		WSADATA wsa{};
		wsa_initialized = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
	}

	~Impl() {
		close_client();
		if (listen_sock != INVALID_SOCKET) {
			closesocket(listen_sock);
			listen_sock = INVALID_SOCKET;
		}
		if (wsa_initialized)
			WSACleanup();
	}

	bool is_running() const { return listen_sock != INVALID_SOCKET; }
	bool has_client() const { return client_sock != INVALID_SOCKET; }

	void close_client() {
		if (client_sock != INVALID_SOCKET) {
			if (sink && Logger::inst)
				Logger::inst->remove_sink(sink.get());
			if (sink)
				sink->sock = INVALID_SOCKET;
			closesocket(client_sock);
			client_sock = INVALID_SOCKET;
		}
		waiting_ticks = false;
		waiting_time = false;
		reblock_pending = false;
		line_buf.clear();
	}

	void send_str(const char* s) {
		if (client_sock == INVALID_SOCKET)
			return;
		::send(client_sock, s, (int)strlen(s), 0);
	}

	// Try to accept a new connection (non-blocking).
	void try_accept() {
		if (has_client())
			return;
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(listen_sock, &fds);
		timeval tv{0, 0};
		if (select(0, &fds, nullptr, nullptr, &tv) > 0) {
			SOCKET s = accept(listen_sock, nullptr, nullptr);
			if (s != INVALID_SOCKET) {
				client_sock = s;
				// set non-blocking
				u_long mode = 1;
				ioctlsocket(client_sock, FIONBIO, &mode);

				sink = std::make_shared<SocketLogSink>();
				sink->sock = client_sock;
				if (Logger::inst)
					Logger::inst->add_sink(sink);

				send_str(">>CONNECTED\n");
			}
		}
	}

	// Read a line from the client. Returns true if a complete line is available.
	// Non-blocking.
	bool try_read_line(std::string& out) {
		char buf[512];
		int n = recv(client_sock, buf, sizeof(buf), 0);
		if (n == 0) { // disconnected
			close_client();
			return false;
		}
		if (n == SOCKET_ERROR) {
			int err = WSAGetLastError();
			if (err == WSAEWOULDBLOCK)
				return false;
			close_client();
			return false;
		}
		line_buf.append(buf, n);

		auto pos = line_buf.find('\n');
		if (pos == std::string::npos)
			return false;

		out = line_buf.substr(0, pos);
		// strip \r if present
		if (!out.empty() && out.back() == '\r')
			out.pop_back();
		line_buf.erase(0, pos + 1);
		return true;
	}

	// Blocking read — spins until a line arrives or client disconnects.
	bool read_line_blocking(std::string& out) {
		// set socket back to blocking temporarily
		u_long mode = 0;
		ioctlsocket(client_sock, FIONBIO, &mode);

		bool got = false;
		char buf[512];
		while (true) {
			// check existing buffer first
			auto pos = line_buf.find('\n');
			if (pos != std::string::npos) {
				out = line_buf.substr(0, pos);
				if (!out.empty() && out.back() == '\r')
					out.pop_back();
				line_buf.erase(0, pos + 1);
				got = true;
				break;
			}
			int n = recv(client_sock, buf, sizeof(buf), 0);
			if (n <= 0) {
				close_client();
				break;
			}
			line_buf.append(buf, n);
		}

		if (has_client()) {
			u_long nb = 1;
			ioctlsocket(client_sock, FIONBIO, &nb);
		}
		return got;
	}

	// Execute a cmd: or lua: line, flush log sink output, then send >>OK / >>ERROR.
	void dispatch_command(const std::string& line) {
		if (line.rfind("cmd:", 0) == 0) {
			std::string cmd = line.substr(4);
			if (Cmd_Manager::inst)
				Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, cmd.c_str());
			send_str(">>OK\n");
		} else if (line.rfind("lua:", 0) == 0) {
			std::string code = line.substr(4);
			if (ScriptManager::inst) {
				lua_State* L = ScriptManager::inst->get_lua_state();
				s_active_sink = sink.get();
				redirect_lua_print(L);
				try {
					ScriptManager::inst->reload_from_content(code, "agent_repl");
					send_str(">>OK\n");
				}
				catch (const LuaRuntimeError& e) {
					std::string err = std::string(">>ERROR: ") + e.what() + "\n";
					send_str(err.c_str());
				}
				catch (...) {
					send_str(">>ERROR: unknown lua error\n");
				}
				restore_lua_print(L);
				s_active_sink = nullptr;
			} else {
				send_str(">>ERROR: ScriptManager not available\n");
			}
		} else {
			std::string err = ">>ERROR: unknown command: " + line + "\n";
			send_str(err.c_str());
		}
	}

	// Entered when "block" is received. Halts game loop, reads commands until
	// "continue" or a wait_ticks/wait_time command.
	void run_blocked_loop() {
		send_str(">>BLOCKED\n");
		while (has_client()) {
			std::string line;
			if (!read_line_blocking(line))
				break;

			if (line == "continue") {
				send_str(">>OK\n");
				break;
			} else if (line.rfind("wait_ticks:", 0) == 0) {
				int n = std::stoi(line.substr(11));
				wait_ticks_remaining = n;
				waiting_ticks = true;
				reblock_pending = true;
				send_str(">>OK\n");
				break; // return to game loop
			} else if (line.rfind("wait_time:", 0) == 0) {
				float secs = std::stof(line.substr(10));
				auto now = std::chrono::steady_clock::now();
				wait_time_end = std::chrono::duration<double>(now.time_since_epoch()).count() + secs;
				waiting_time = true;
				reblock_pending = true;
				send_str(">>OK\n");
				break; // return to game loop
			} else {
				dispatch_command(line);
			}
		}
	}

	void poll() {
		if (!is_running())
			return;
		try_accept();
		if (!has_client())
			return;

		// If a wait is in progress, check if it has expired
		if (waiting_ticks) {
			if (--wait_ticks_remaining <= 0) {
				waiting_ticks = false;
				if (reblock_pending) {
					reblock_pending = false;
					run_blocked_loop();
				}
			}
			return;
		}
		if (waiting_time) {
			auto now = std::chrono::steady_clock::now();
			double t = std::chrono::duration<double>(now.time_since_epoch()).count();
			if (t >= wait_time_end) {
				waiting_time = false;
				if (reblock_pending) {
					reblock_pending = false;
					run_blocked_loop();
				}
			}
			return;
		}

		// Normal (non-blocked) polling — drain available commands
		std::string line;
		while (has_client() && try_read_line(line)) {
			if (line == "block") {
				run_blocked_loop();
				return;
			}
			if (line == "resume") {
				resume_requested = true;
				send_str(">>OK\n");
				return;
			}
			dispatch_command(line);
		}
	}
};

// ---------------------------------------------------------------------------
// AgentREPL public API
// ---------------------------------------------------------------------------
AgentREPL::AgentREPL() : impl_(std::make_unique<Impl>()) {}
AgentREPL::~AgentREPL() = default;

void AgentREPL::start(int port) {
	if (impl_->is_running())
		return;
	if (!impl_->wsa_initialized) {
		sys_print(Error, "[AgentREPL] WSAStartup failed\n");
		return;
	}

	SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s == INVALID_SOCKET) {
		sys_print(Error, "[AgentREPL] socket() failed: %d\n", WSAGetLastError());
		return;
	}

	// Allow quick restart
	int yes = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short)port);
	inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

	if (bind(s, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		sys_print(Error, "[AgentREPL] bind() failed: %d\n", WSAGetLastError());
		closesocket(s);
		return;
	}
	if (listen(s, 1) == SOCKET_ERROR) {
		sys_print(Error, "[AgentREPL] listen() failed: %d\n", WSAGetLastError());
		closesocket(s);
		return;
	}

	// Non-blocking listen socket
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);

	impl_->listen_sock = s;
	sys_print(Info, "[AgentREPL] Listening on 127.0.0.1:%d\n", port);
}

void AgentREPL::stop() {
	impl_->close_client();
	if (impl_->listen_sock != INVALID_SOCKET) {
		closesocket(impl_->listen_sock);
		impl_->listen_sock = INVALID_SOCKET;
	}
}

void AgentREPL::poll() {
	impl_->poll();
}

bool AgentREPL::is_running() const {
	return impl_->is_running();
}

bool AgentREPL::take_resume_requested() {
	if (impl_->resume_requested) {
		impl_->resume_requested = false;
		return true;
	}
	return false;
}
