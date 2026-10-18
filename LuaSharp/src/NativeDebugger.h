#pragma once

#include "CBackend.h"

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace luasharp {

enum class NativeStopReason { Breakpoint, Step, Pause, Exception };
struct NativeStopEvent {
    NativeStopReason reason{};
    std::string message; // Exception: the runtime error
};
struct NativeDebugFrame {
    std::string function;
    std::string source;
    uint32_t line{}; // 0 when unknown
};
// One value as the debugger shows it. Give children to Variables for its fields or elements; 0 when it has none.
struct NativeDebugVariable {
    std::string name;
    std::string type;
    std::string value;
    uint64_t children{};
};

struct NativeEvaluation {
    std::optional<NativeDebugVariable> value;
    std::string error; // when there is no value
};

// Breakpoints, stepping and inspection for instrumented modules (NativeCompileOptions::emitDebugHooks).
// Scripts run on one thread. At a stop, that thread calls the stop handler and then waits for a resume command.
// A command that the handler gives resumes without a wait. Other threads can call all members.
// Inspection runs on the stopped script thread: a call from another thread waits for it. An extern object shows its fields
// and its [DebugView] methods; the debugger calls their host getters.
class NativeDebugger {
public:
    explicit NativeDebugger(std::shared_ptr<NativeRuntime> runtime);
    ~NativeDebugger();
    NativeDebugger(const NativeDebugger&) = delete;
    NativeDebugger& operator=(const NativeDebugger&) = delete;

    // The module must be instrumented, use the runtime of the debugger, and stay loaded until RemoveModule. It can move.
    void AddModule(const NativeModule& module);
    // Not while a script of the module is stopped.
    void RemoveModule(const NativeModule& module);
    // Hooks run only while attached. Detach resumes a stopped script.
    void Attach();
    void Detach();
    [[nodiscard]] bool Attached() const;
    // Runtime errors stop as exceptions (the default).
    void SetBreakOnErrors(bool enabled);
    // Replaces the breakpoints of a source, in the modules of the debugger and in modules added later. Gives the line
    // of each breakpoint: the first line with a statement at or after it, or 0 when there is none.
    std::vector<uint32_t> SetBreakpoints(std::string_view source, const std::vector<uint32_t>& lines);
    void SetStopHandler(std::function<void(const NativeStopEvent&)> handler);

    // Stops at the next statement.
    void Pause();
    // Resume commands; they do nothing while the script runs.
    void Continue();
    void StepIn();
    void StepOver();
    void StepOut();
    [[nodiscard]] bool Stopped() const;
    // False when the timeout ends first.
    bool WaitUntilStopped(std::chrono::milliseconds timeout) const;

    // Inspection of the stopped script; empty while it runs. References stay valid until it resumes.
    [[nodiscard]] std::vector<NativeDebugFrame> StackTrace() const;
    std::vector<NativeDebugVariable> Locals(size_t frame);
    std::vector<NativeDebugVariable> Variables(uint64_t reference);
    // A path in the frame: a local or a field of `this`, then `.member` (a field, a property or a [DebugView] method, with
    // or without "()") and `[index]`.
    NativeEvaluation Evaluate(size_t frame, std::string_view expression);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace luasharp
