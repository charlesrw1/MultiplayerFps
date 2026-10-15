#include "LspBridge.h"

#include <windows.h>

#include <nlohmann/json.hpp>

#include <array>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

using json = nlohmann::json;

namespace {
struct ChildProcess {
    HANDLE input{};
    HANDLE output{};
    PROCESS_INFORMATION process{};
    ChildProcess() = default;
    ChildProcess(HANDLE inputHandle, HANDLE outputHandle, PROCESS_INFORMATION processInfo) : input(inputHandle), output(outputHandle), process(processInfo) {}
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;
    ChildProcess(ChildProcess&& other) noexcept { *this = std::move(other); }
    ChildProcess& operator=(ChildProcess&& other) noexcept {
        if (this == &other) return *this;
        input = std::exchange(other.input, nullptr);
        output = std::exchange(other.output, nullptr);
        process = std::exchange(other.process, {});
        return *this;
    }
    ~ChildProcess() { if (input) CloseHandle(input); if (output) CloseHandle(output); if (process.hThread) CloseHandle(process.hThread); if (process.hProcess) { TerminateProcess(process.hProcess, 0); CloseHandle(process.hProcess); } }
};

bool ReadExact(HANDLE handle, char* buffer, DWORD count) {
    while (count != 0) { DWORD read{}; if (!ReadFile(handle, buffer, count, &read, nullptr) || read == 0) return false; buffer += read; count -= read; }
    return true;
}

std::optional<json> ReadMessage(HANDLE handle) {
    std::string header; char character{};
    while (header.size() < 8192 && ReadExact(handle, &character, 1)) { header += character; if (header.ends_with("\r\n\r\n")) break; }
    const auto prefix = header.find("Content-Length:");
    if (prefix == std::string::npos) return std::nullopt;
    const auto end = header.find("\r\n", prefix);
    const auto length = std::stoul(header.substr(prefix + 15, end - prefix - 15));
    std::string body(length, '\0');
    if (!ReadExact(handle, body.data(), static_cast<DWORD>(body.size()))) return std::nullopt;
    return json::parse(body, nullptr, false);
}

bool WriteMessage(HANDLE handle, const json& message) {
    const auto body = message.dump();
    const auto frame = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    DWORD written{};
    return WriteFile(handle, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr) && written == frame.size();
}

std::optional<ChildProcess> StartLuaLs(const std::string& executable) {
    SECURITY_ATTRIBUTES attributes{sizeof(attributes), nullptr, TRUE};
    HANDLE childInputRead{}, childInputWrite{}, childOutputRead{}, childOutputWrite{};
    if (!CreatePipe(&childInputRead, &childInputWrite, &attributes, 0) || !CreatePipe(&childOutputRead, &childOutputWrite, &attributes, 0)) return std::nullopt;
    SetHandleInformation(childInputWrite, HANDLE_FLAG_INHERIT, 0); SetHandleInformation(childOutputRead, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOA startup{sizeof(startup)}; startup.dwFlags = STARTF_USESTDHANDLES; startup.hStdInput = childInputRead; startup.hStdOutput = childOutputWrite; startup.hStdError = childOutputWrite;
    PROCESS_INFORMATION process{}; std::string command = '"' + executable + "\" --stdio";
    const auto workingDirectory = std::filesystem::path(executable).parent_path().string();
    if (!CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, workingDirectory.c_str(), &startup, &process)) return std::nullopt;
    CloseHandle(childInputRead); CloseHandle(childOutputWrite);
    return ChildProcess{childInputWrite, childOutputRead, process};
}

class Proxy {
public:
    void ToLuaLs(json& message) {
        const auto method = message.value("method", "");
        if (method != "textDocument/didOpen" && method != "textDocument/didChange") return;
        auto& document = method == "textDocument/didOpen" ? message["params"]["textDocument"] : message["params"]["textDocument"];
        const auto sourceUri = document.value("uri", "");
        std::string source;
        if (method == "textDocument/didOpen") source = document.value("text", "");
        else { const auto& changes = message["params"].value("contentChanges", json::array()); if (changes.empty() || !changes.back().contains("text")) return; source = changes.back()["text"]; }
        auto virtualDocument = luasharp::CreateVirtualDocument(sourceUri, source);
        const auto virtualUri = sourceUri + ".luasharp.lua";
        virtualDocument.virtualUri = virtualUri;
        std::scoped_lock lock(mutex_); documents_[virtualUri] = std::move(virtualDocument);
        document["uri"] = virtualUri;
        if (method == "textDocument/didOpen") { document["languageId"] = "lua"; document["text"] = documents_[virtualUri].compilation.lua; }
        else message["params"]["contentChanges"] = json::array({{{"text", documents_[virtualUri].compilation.lua}}});
    }

    void ToClient(json& message) {
        if (message.value("method", "") != "textDocument/publishDiagnostics") return;
        auto& params = message["params"]; const auto virtualUri = params.value("uri", "");
        std::scoped_lock lock(mutex_); const auto document = documents_.find(virtualUri); if (document == documents_.end()) return;
        params["uri"] = document->second.sourceUri;
        for (auto& diagnostic : params["diagnostics"]) for (const auto endpoint : {"start", "end"}) {
            const auto& position = diagnostic["range"][endpoint];
            const auto mapped = luasharp::ToSourcePosition(document->second, {position.value("line", 0U), position.value("character", 0U)});
            diagnostic["range"][endpoint] = {{"line", mapped.line}, {"character", mapped.character}};
        }
    }
private:
    std::mutex mutex_;
    std::unordered_map<std::string, luasharp::VirtualDocument> documents_;
};
}

int main(int argc, char** argv) {
    if (argc != 2) { std::cerr << "Usage: LuaSharpLsp <lua-language-server.exe>\n"; return 2; }
    auto child = StartLuaLs(argv[1]); if (!child) { std::cerr << "Could not start LuaLS\n"; return 1; }
    Proxy proxy;
    std::thread serverToClient([&] { while (auto message = ReadMessage(child->output)) { proxy.ToClient(*message); if (!WriteMessage(GetStdHandle(STD_OUTPUT_HANDLE), *message)) break; } });
    while (auto message = ReadMessage(GetStdHandle(STD_INPUT_HANDLE))) { proxy.ToLuaLs(*message); if (!WriteMessage(child->input, *message)) break; }
    serverToClient.join();
}
