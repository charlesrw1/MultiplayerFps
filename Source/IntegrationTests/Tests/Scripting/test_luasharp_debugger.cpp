// Source/IntegrationTests/Tests/Scripting/test_luasharp_debugger.cpp
//
// The LuaSharp script debugger in the running engine: a DAP client on a thread breaks in a component update.

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "GameEnginePublic.h"
#include "Level.h"
#include "Game/Entity.h"
#include "Game/EntityComponent.h"
#include "Framework/ClassBase.h"
#include "Framework/Files.h"
#include "Scripting/LuaSharpEngine.h"
#include "../LuaSharp/src/CBackend.h"
#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <map>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

namespace {
using json = nlohmann::json;

constexpr const char* kProbeModule = "scripts/tests/luasharp_debug_probe.ls";
constexpr uint32_t kBreakLine = 7;
constexpr const char* kProbeSource = R"(class LsDebugProbe : Component {
    int ticks = 0;
    override void start() { set_ticking(true); }
    override void update() {
        int doubled = ticks * 2;
        string label = "probe";
        ticks = ticks + 1;
    }
}
)";

// A blocking DAP client. The script thread stops at the breakpoint, so the client runs on its own thread.
class DapProbe
{
public:
	struct Result
	{
		std::string error; // empty on success
		std::string reason;
		std::string function;
		uint32_t line = 0;
		std::map<std::string, std::string> locals;
	};

	DapProbe(uint16_t port, std::string source) : thread_([this, port, source = std::move(source)] { run(port, source); }) {}
	~DapProbe() { thread_.join(); }
	DapProbe(const DapProbe&) = delete;
	DapProbe& operator=(const DapProbe&) = delete;

	std::atomic<bool> configured{false};
	std::atomic<bool> done{false};
	// Valid after done.
	Result result;

private:
	void run(uint16_t port, const std::string& source) {
		WSADATA data{};
		WSAStartup(MAKEWORD(2, 2), &data);
		try {
			connect_to(port);
			session(source);
		} catch (const std::exception& error) {
			result.error = error.what();
		}
		if (socket_ != INVALID_SOCKET)
			closesocket(socket_); // the server detaches the debugger, so no script stays stopped
		WSACleanup();
		configured = true;
		done = true;
	}

	void session(const std::string& source) {
		request("initialize", {{"adapterID", "luasharp"}});
		wait_event("initialized");
		request("attach", json::object());
		const json breakpoint = {{"line", kBreakLine}};
		const json bound = request("setBreakpoints", {{"source", {{"path", source}}}, {"breakpoints", json::array({breakpoint})}});
		if (!bound["breakpoints"][0].value("verified", false))
			throw std::runtime_error("the breakpoint did not bind: " + bound.dump());
		request("configurationDone", json::object());
		configured = true;

		result.reason = wait_event("stopped")["body"].value("reason", "");
		const json frames = request("stackTrace", {{"threadId", 1}})["stackFrames"];
		if (frames.empty())
			throw std::runtime_error("no stack frames");
		result.function = frames[0].value("name", "");
		result.line = frames[0].value("line", 0u);
		const json scopes = request("scopes", {{"frameId", frames[0]["id"]}})["scopes"];
		const json variables = request("variables", {{"variablesReference", scopes[0]["variablesReference"]}})["variables"];
		for (const json& variable : variables)
			result.locals[variable.value("name", "")] = variable.value("value", "");
		request("setBreakpoints", {{"source", {{"path", source}}}, {"breakpoints", json::array()}});
		request("continue", {{"threadId", 1}});
		request("disconnect", json::object());
	}

	void connect_to(uint16_t port) {
		socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		const DWORD timeout_ms = 10000;
		setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout_ms), sizeof(timeout_ms));
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		if (connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
			throw std::runtime_error("cannot connect to the debugger");
	}

	json request(const std::string& command, json arguments) {
		const int seq = sequence_++;
		const std::string body = json{{"seq", seq}, {"type", "request"}, {"command", command}, {"arguments", std::move(arguments)}}.dump();
		const std::string data = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
		if (send(socket_, data.data(), static_cast<int>(data.size()), 0) != static_cast<int>(data.size()))
			throw std::runtime_error("send failed: " + command);
		const json response = wait([&](const json& m) { return m.value("type", "") == "response" && m.value("request_seq", 0) == seq; }, command);
		if (!response.value("success", false))
			throw std::runtime_error(command + " failed: " + response.value("message", ""));
		return response.value("body", json::object());
	}
	json wait_event(const std::string& name) {
		return wait([&](const json& m) { return m.value("type", "") == "event" && m.value("event", "") == name; }, name);
	}
	// Reads messages until one matches; the others are dropped.
	template <typename Match> json wait(Match&& match, const std::string& what) {
		for (;;) {
			json message = read_message(what);
			if (match(message))
				return message;
		}
	}
	json read_message(const std::string& what) {
		for (;;) {
			if (const size_t header_end = buffer_.find("\r\n\r\n"); header_end != std::string::npos) {
				const size_t length = std::stoul(buffer_.substr(buffer_.find(':') + 1, header_end));
				if (buffer_.size() >= header_end + 4 + length) {
					json message = json::parse(buffer_.substr(header_end + 4, length));
					buffer_.erase(0, header_end + 4 + length);
					return message;
				}
			}
			char chunk[4096];
			const int received = recv(socket_, chunk, sizeof(chunk), 0);
			if (received <= 0)
				throw std::runtime_error("connection closed while waiting for " + what);
			buffer_.append(chunk, static_cast<size_t>(received));
		}
	}

	SOCKET socket_ = INVALID_SOCKET;
	int sequence_ = 1;
	std::string buffer_;
	std::thread thread_; // last: it starts in the constructor and uses the members above
};

} // namespace

// Attach a DAP client, break in the update of a script component, read the stack and the locals, then continue.
static TestTask test_luasharp_debugger_break_in_update(TestContext& t) {
	t.require(LuaSharpEngine::inst != nullptr, "LuaSharpEngine is created at startup");
	LuaSharpEngine& engine = *LuaSharpEngine::inst;
	if (engine.debugger_port() == 0)
		engine.start_debugger(0);
	if (!engine.find_module(kProbeModule))
		engine.load(kProbeModule, kProbeSource);
	t.require(engine.find_module(kProbeModule)->Debug() != nullptr, "a module loaded with the debugger on is instrumented");
	eng->load_level("");
	co_await t.wait_ticks(1);

	auto probe = std::make_unique<DapProbe>(engine.debugger_port(), FileSys::get_full_path_from_game_path(kProbeModule));
	for (int tick = 0; !probe->configured && tick < 600; tick++)
		co_await t.wait_ticks(1);
	t.require(probe->configured, "the client configured the session");

	Entity* e = eng->get_level()->spawn_entity();
	Component* comp = e->create_component(ClassBase::find_class("LsDebugProbe"));
	t.require(comp != nullptr, "create_component made the probe component");
	// The update stops in the level tick; the client continues it.
	for (int tick = 0; !probe->done && tick < 600; tick++)
		co_await t.wait_ticks(1);
	t.require(probe->done, "the client finished");
	DapProbe::Result result = probe->result;
	probe.reset();

	t.check(result.error.empty(), ("the DAP session had no error: " + result.error).c_str());
	t.check(result.reason == "breakpoint", ("the script stopped at the breakpoint, not: " + result.reason).c_str());
	t.check(result.function.find("update") != std::string::npos, ("the top frame is update: " + result.function).c_str());
	t.check(result.line == kBreakLine, "the top frame is on the breakpoint line");
	t.check(result.locals.count("this") == 1, "the locals show this");
	t.check(result.locals["doubled"] == "0", ("doubled is 0 in the first update: " + result.locals["doubled"]).c_str());
	t.check(result.locals["label"] == "\"probe\"", ("label is the string: " + result.locals["label"]).c_str());
	e->destroy();
	co_await t.wait_ticks(1);
}
GAME_TEST("luasharp/debugger_break_in_update", 30.f, test_luasharp_debugger_break_in_update);
