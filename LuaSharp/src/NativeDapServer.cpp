#include "NativeDapServer.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

namespace luasharp {
namespace {
using json = nlohmann::json;

constexpr int64_t ThreadId = 1;          // scripts run on one thread
constexpr uint64_t ScopeBase = 1ull << 30; // a frame's locals scope is ScopeBase + frame; value references are below it

struct Winsock {
    Winsock() { WSADATA data{}; if (WSAStartup(MAKEWORD(2, 2), &data) != 0) throw std::runtime_error("WSAStartup failed"); }
    ~Winsock() { WSACleanup(); }
    Winsock(const Winsock&) = delete;
    Winsock& operator=(const Winsock&) = delete;
};

const char* ReasonName(NativeStopReason reason) {
    switch (reason) {
    case NativeStopReason::Breakpoint: return "breakpoint";
    case NativeStopReason::Step: return "step";
    case NativeStopReason::Pause: return "pause";
    case NativeStopReason::Exception: return "exception";
    }
    return "pause";
}

json VariableJson(const NativeDebugVariable& variable) {
    assert(variable.children < ScopeBase);
    return {{"name", variable.name}, {"value", variable.value}, {"type", variable.type}, {"variablesReference", variable.children}};
}

// The next message body of the "Content-Length: N\r\n\r\n<body>" stream; nullopt when the client closes it.
std::optional<std::string> ReadMessage(SOCKET socket, std::string& buffer) {
    constexpr std::string_view LengthHeader = "Content-Length:";
    for (;;) {
        if (const auto headerEnd = buffer.find("\r\n\r\n"); headerEnd != std::string::npos) {
            const auto field = buffer.find(LengthHeader);
            if (field == std::string::npos || field > headerEnd) return std::nullopt; // not DAP
            const auto length = std::stoull(buffer.substr(field + LengthHeader.size(), headerEnd - field - LengthHeader.size()));
            const auto bodyStart = headerEnd + 4;
            if (buffer.size() >= bodyStart + length) {
                auto body = buffer.substr(bodyStart, length);
                buffer.erase(0, bodyStart + length);
                return body;
            }
        }
        char chunk[4096];
        const int received = recv(socket, chunk, sizeof(chunk), 0);
        if (received <= 0) return std::nullopt;
        buffer.append(chunk, static_cast<size_t>(received));
    }
}
} // namespace

struct NativeDapServer::Impl {
    NativeDebugger& debugger;
    Winsock winsock;
    SOCKET listener{INVALID_SOCKET};
    uint16_t port{};
    mutable std::mutex mutex; // the members below; it also keeps messages whole
    mutable std::condition_variable changed;
    SOCKET client{INVALID_SOCKET};
    bool configured{};
    bool closing{};
    int64_t sequence{1};
    std::thread thread;

    explicit Impl(NativeDebugger& debugger) : debugger(debugger) {}

    void Listen(uint16_t requestedPort) {
        listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listener == INVALID_SOCKET) throw std::runtime_error("DAP server: cannot create a socket");
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(requestedPort);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        int size = sizeof(address);
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listener, 1) != 0
            || getsockname(listener, reinterpret_cast<sockaddr*>(&address), &size) != 0) {
            closesocket(listener);
            throw std::runtime_error("DAP server: cannot listen on port " + std::to_string(requestedPort));
        }
        port = ntohs(address.sin_port);
    }

    // The server thread: one client session after the other, until the listener closes.
    void Run() {
        for (;;) {
            const SOCKET accepted = accept(listener, nullptr, nullptr);
            if (accepted == INVALID_SOCKET) return;
            {
                std::scoped_lock lock(mutex);
                if (closing) { closesocket(accepted); return; }
                const BOOL noDelay = TRUE;
                setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay), sizeof(noDelay));
                client = accepted;
                sequence = 1;
            }
            Serve(accepted);
            EndSession();
        }
    }

    void Serve(SOCKET socket) {
        std::string buffer;
        while (const auto message = ReadMessage(socket, buffer)) {
            const auto request = json::parse(*message, nullptr, false);
            if (request.is_discarded() || request.value("type", "") != "request") continue;
            if (!Handle(request)) return;
        }
    }

    void EndSession() {
        debugger.Detach();
        std::scoped_lock lock(mutex);
        closesocket(client);
        client = INVALID_SOCKET;
        configured = false;
        changed.notify_all();
    }

    void Send(json message) {
        std::scoped_lock lock(mutex);
        if (client == INVALID_SOCKET) return;
        message["seq"] = sequence++;
        const auto body = message.dump(-1, ' ', false, json::error_handler_t::replace);
        const auto data = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
        for (size_t sent{}; sent < data.size();) {
            const int count = send(client, data.data() + sent, static_cast<int>(data.size() - sent), 0);
            if (count <= 0) return; // the session ends at the next read
            sent += static_cast<size_t>(count);
        }
    }
    void Event(std::string_view name, json body = json::object()) { Send({{"type", "event"}, {"event", name}, {"body", std::move(body)}}); }
    void Respond(const json& request, json body = json::object()) {
        Send({{"type", "response"}, {"request_seq", request.value("seq", 0)}, {"success", true}, {"command", request.value("command", "")}, {"body", std::move(body)}});
    }
    void Fail(const json& request, std::string_view message) {
        Send({{"type", "response"}, {"request_seq", request.value("seq", 0)}, {"success", false}, {"command", request.value("command", "")}, {"message", message}});
    }

    // False ends the session.
    bool Handle(const json& request) {
        const auto command = request.value("command", "");
        const auto arguments = request.value("arguments", json::object());
        if (command == "initialize") {
            const json errors = {{"filter", "errors"}, {"label", "Runtime errors"}, {"default", true}};
            Respond(request, {{"supportsConfigurationDoneRequest", true}, {"supportsEvaluateForHovers", true}, {"exceptionBreakpointFilters", json::array({errors})}});
            Event("initialized");
        } else if (command == "attach") {
            debugger.Attach();
            Respond(request);
        } else if (command == "launch") {
            Fail(request, "LuaSharp starts no program: use \"request\": \"attach\"");
        } else if (command == "setBreakpoints") {
            SetBreakpoints(request, arguments);
        } else if (command == "setExceptionBreakpoints") {
            const auto filters = arguments.value("filters", json::array());
            debugger.SetBreakOnErrors(std::find(filters.begin(), filters.end(), "errors") != filters.end());
            Respond(request);
        } else if (command == "configurationDone") {
            { std::scoped_lock lock(mutex); configured = true; changed.notify_all(); }
            Respond(request);
        } else if (command == "threads") {
            const json scriptThread = {{"id", ThreadId}, {"name", "script"}};
            Respond(request, {{"threads", json::array({scriptThread})}});
        } else if (command == "stackTrace") {
            StackTrace(request, arguments);
        } else if (command == "scopes") {
            const auto frame = arguments.value("frameId", uint64_t{});
            const json locals = {{"name", "Locals"}, {"presentationHint", "locals"}, {"variablesReference", ScopeBase + frame}, {"expensive", false}};
            Respond(request, {{"scopes", json::array({locals})}});
        } else if (command == "evaluate") {
            Evaluate(request, arguments);
        } else if (command == "variables") {
            const auto reference = arguments.value("variablesReference", uint64_t{});
            const auto variables = reference >= ScopeBase ? debugger.Locals(reference - ScopeBase) : debugger.Variables(reference);
            auto result = json::array();
            for (const auto& variable : variables) result.push_back(VariableJson(variable));
            Respond(request, {{"variables", std::move(result)}});
        } else if (command == "continue") {
            Respond(request, {{"allThreadsContinued", true}});
            debugger.Continue();
        } else if (command == "next") {
            Respond(request);
            debugger.StepOver();
        } else if (command == "stepIn") {
            Respond(request);
            debugger.StepIn();
        } else if (command == "stepOut") {
            Respond(request);
            debugger.StepOut();
        } else if (command == "pause") {
            debugger.Pause();
            Respond(request);
        } else if (command == "disconnect") {
            Respond(request);
            return false;
        } else {
            Fail(request, "unsupported request: " + command);
        }
        return true;
    }

    void SetBreakpoints(const json& request, const json& arguments) {
        const auto source = arguments.value("source", json::object());
        const auto path = source.value("path", source.value("name", ""));
        std::vector<uint32_t> lines;
        for (const auto& breakpoint : arguments.value("breakpoints", json::array())) lines.push_back(breakpoint.value("line", 0u));
        const auto bound = debugger.SetBreakpoints(path, lines);
        assert(bound.size() == lines.size());
        auto result = json::array();
        for (size_t i{}; i < lines.size(); ++i) result.push_back({{"verified", bound[i] != 0}, {"line", bound[i] != 0 ? bound[i] : lines[i]}});
        Respond(request, {{"breakpoints", std::move(result)}});
    }

    // Watch, hover and debug console: a path such as `this.owner.get_ws_position()` (NativeDebugger::Evaluate).
    void Evaluate(const json& request, const json& arguments) {
        const auto result = debugger.Evaluate(arguments.value("frameId", size_t{}), arguments.value("expression", ""));
        if (!result.value) return Fail(request, result.error);
        assert(result.value->children < ScopeBase);
        Respond(request, {{"result", result.value->value}, {"type", result.value->type}, {"variablesReference", result.value->children}});
    }

    void StackTrace(const json& request, const json& arguments) {
        const auto frames = debugger.StackTrace();
        const auto start = std::min<size_t>(arguments.value("startFrame", size_t{}), frames.size());
        const auto levels = arguments.value("levels", size_t{});
        const auto end = levels == 0 ? frames.size() : std::min(frames.size(), start + levels);
        auto result = json::array();
        for (size_t i = start; i < end; ++i) {
            const auto& frame = frames[i];
            json item = {{"id", i}, {"name", frame.function}, {"line", frame.line}, {"column", frame.line != 0 ? 1 : 0}};
            if (!frame.source.empty()) item["source"] = {{"name", std::filesystem::path(frame.source).filename().string()}, {"path", frame.source}};
            result.push_back(std::move(item));
        }
        Respond(request, {{"stackFrames", std::move(result)}, {"totalFrames", frames.size()}});
    }

    void OnStop(const NativeStopEvent& event) {
        json body = {{"reason", ReasonName(event.reason)}, {"threadId", ThreadId}, {"allThreadsStopped", true}};
        if (!event.message.empty()) { body["description"] = event.message; body["text"] = event.message; }
        Event("stopped", std::move(body));
    }
};

NativeDapServer::NativeDapServer(NativeDebugger& debugger, uint16_t port) : impl_(std::make_unique<Impl>(debugger)) {
    impl_->Listen(port);
    debugger.SetStopHandler([impl = impl_.get()](const NativeStopEvent& event) { impl->OnStop(event); });
    impl_->thread = std::thread([impl = impl_.get()] { impl->Run(); });
}

NativeDapServer::~NativeDapServer() {
    {
        std::scoped_lock lock(impl_->mutex);
        impl_->closing = true;
        closesocket(impl_->listener);
        if (impl_->client != INVALID_SOCKET) shutdown(impl_->client, SD_BOTH); // the server thread closes it
    }
    impl_->thread.join();
    impl_->debugger.SetStopHandler({});
}

uint16_t NativeDapServer::Port() const { return impl_->port; }
bool NativeDapServer::Configured() const { std::scoped_lock lock(impl_->mutex); return impl_->configured; }
bool NativeDapServer::WaitUntilConfigured(std::chrono::milliseconds timeout) const {
    std::unique_lock lock(impl_->mutex);
    return impl_->changed.wait_for(lock, timeout, [&] { return impl_->configured; });
}
bool NativeDapServer::WaitUntilDisconnected(std::chrono::milliseconds timeout) const {
    std::unique_lock lock(impl_->mutex);
    return impl_->changed.wait_for(lock, timeout, [&] { return impl_->client == INVALID_SOCKET; });
}
void NativeDapServer::SendOutput(std::string_view text, std::string_view category) { impl_->Event("output", {{"category", category}, {"output", text}}); }
void NativeDapServer::SendTerminated() { impl_->Event("terminated"); }

} // namespace luasharp
