#pragma once

#include "NativeDebugger.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>

namespace luasharp {

// A Debug Adapter Protocol server for a NativeDebugger, on TCP 127.0.0.1. One client at a time: a client attaches
// the debugger, and its disconnect detaches it. The server takes the stop handler of the debugger.
// See LuaSharp/docs/native-abi.md.
class NativeDapServer {
public:
    // Port 0 selects a free port. Throws std::runtime_error when it cannot listen.
    NativeDapServer(NativeDebugger& debugger, uint16_t port);
    ~NativeDapServer();
    NativeDapServer(const NativeDapServer&) = delete;
    NativeDapServer& operator=(const NativeDapServer&) = delete;

    [[nodiscard]] uint16_t Port() const;
    // From the configurationDone of a client until its disconnect.
    [[nodiscard]] bool Configured() const;
    bool WaitUntilConfigured(std::chrono::milliseconds timeout) const;
    // False when the timeout ends first.
    bool WaitUntilDisconnected(std::chrono::milliseconds timeout) const;
    // Events to the client; nothing when no client is connected.
    void SendOutput(std::string_view text, std::string_view category = "stdout");
    void SendTerminated();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace luasharp
