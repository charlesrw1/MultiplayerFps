#include "NativeDebugger.h"
#include "NativeAbi.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cassert>
#include <charconv>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <optional>

namespace luasharp {
namespace {

using namespace abi;

constexpr int64_t UnknownOffset = INT64_MIN;
constexpr uint64_t NoStatement = UINT64_MAX;
constexpr size_t MaxFrames = 256;
constexpr size_t MaxChildren = 1000;
constexpr size_t MaxStringShown = 256;

constexpr size_t MaxSummaryFields = 6;
constexpr uint64_t NullValue = 255; // the slot kind of an `any` that holds null; Unwrap gives it

enum class Mode { Run, Pause, StepIn, StepOver, StepOut };

// One stored value: its address and how to read it (the descriptor of an LS_FieldInfo).
struct Slot {
    const std::byte* address{};
    uint64_t kind{}, elementKind{};
    const NativeTypeInfo* type{};
    const NativeEnumInfo* enumInfo{};
};
Slot SlotOf(const std::byte* base, const NativeFieldInfo& field) { return {base + field.offset, field.kind, field.elementKind, field.type, field.enumInfo}; }

bool IsKind(uint64_t kind, NativeValueKind value) { return kind == KindOf(value); }

// File names match without case and slash differences, and when one is a path that ends with the other.
std::string NormalizedSource(std::string_view source) {
    std::string result(source);
    for (auto& c : result) c = c == '\\' ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return result;
}
bool SameSource(std::string_view a, std::string_view b) {
    const auto left = NormalizedSource(a), right = NormalizedSource(b);
    const auto& longer = left.size() >= right.size() ? left : right;
    const auto& shorter = left.size() >= right.size() ? right : left;
    if (shorter.empty() || !longer.ends_with(shorter)) return false;
    return longer.size() == shorter.size() || longer[longer.size() - shorter.size() - 1] == '/';
}

template <typename T> std::string Number(T value) {
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value);
    return std::string(buffer, result.ptr);
}

std::string Quoted(const RuntimeString* string) {
    std::string result = "\"";
    const auto length = static_cast<size_t>(std::max<int64_t>(string->length, 0));
    for (size_t i{}; i < std::min(length, MaxStringShown); ++i) {
        const char c = string->data[i];
        if (c == '"' || c == '\\') result += '\\';
        if (c == '\n') result += "\\n"; else if (c == '\t') result += "\\t"; else result += c;
    }
    if (length > MaxStringShown) result += "...";
    return result + '"';
}

std::string EnumText(const NativeEnumInfo& info, int64_t value) {
    for (uint64_t i{}; i < info.count; ++i) if (info.values[i] == value) return std::string(info.name) + '.' + info.names[i];
    return std::string(info.name) + '(' + Number(value) + ')';
}

bool IsProxy(const NativeTypeInfo& type) { return (type.flags & (TypeNative | TypeNativeBase)) != 0; }

} // namespace

struct NativeDebugger::Impl {
    struct Module {
        Impl* debugger{};
        const NativeDebugModule* tables{};
        const NativeDebugInfo* info{};
        std::vector<size_t> firstStatement;                    // per function: its statement 0 in breakpoints
        std::unique_ptr<std::atomic<uint8_t>[]> breakpoints;    // per statement of the module
        std::vector<int64_t> frameOffsets;                      // per function: locals - rbp, learned from its hooks

        bool Contains(const void* address) const { return address >= tables->textBegin && address < tables->textEnd; }
        std::optional<size_t> FunctionAt(const void* address) const {
            for (size_t i{}; i < tables->functionCount; ++i)
                if (address >= tables->functions[i].begin && address < tables->functions[i].end) return i;
            return std::nullopt;
        }
        // The line that a breakpoint on line binds to: the first statement line at or after it; 0 when none.
        uint32_t BindLine(uint32_t line) const {
            uint32_t best = 0;
            for (const auto& function : info->functions)
                for (const auto statementLine : function.lines)
                    if (statementLine >= line && (best == 0 || statementLine < best)) best = statementLine;
            return best;
        }
    };
    // A script frame of the stopped thread. locals is null while the frame offset of the function is unknown.
    struct Frame {
        const Module* module{};
        size_t function{};
        const std::byte* locals{};
        uint64_t statement{NoStatement};
    };

    std::shared_ptr<NativeRuntime> runtime;
    mutable std::mutex mutex;
    mutable std::condition_variable stoppedSignal;
    std::condition_variable resumedSignal;
    std::vector<std::unique_ptr<Module>> modules;
    std::map<std::string, std::vector<uint32_t>> requestedBreakpoints; // by source, as the client gave it
    std::function<void(const NativeStopEvent&)> stopHandler;
    std::atomic<Mode> mode{Mode::Run};
    bool attached{};
    bool breakOnErrors{true};
    bool stopped{};
    const std::byte* stepLocals{}; // the locals of the frame a step started in
    std::vector<Frame> frames;     // while stopped
    std::vector<Slot> references;  // while stopped; reference i + 1
    std::deque<NativeValue> copies; // while stopped: table entries that references point into

    static void Hook(void* context, void* locals, void* frame, uint64_t function, uint64_t statement) {
        auto& module = *static_cast<Module*>(context);
        auto& current = *static_cast<uint64_t*>(locals);
        const auto previous = std::exchange(current, statement);
        module.frameOffsets[function] = static_cast<std::byte*>(locals) - static_cast<std::byte*>(frame);
        auto& self = *module.debugger;
        const auto mode = self.mode.load(std::memory_order_relaxed);
        const bool breakpoint = module.breakpoints[module.firstStatement[function] + statement].load(std::memory_order_relaxed) != 0;
        if (mode == Mode::Run && !breakpoint) return;
        self.OnStatement(module, function, statement, previous, static_cast<const std::byte*>(locals), static_cast<const std::byte*>(frame), mode, breakpoint);
    }

    void OnStatement(const Module& module, size_t function, uint64_t statement, uint64_t previous, const std::byte* locals, const std::byte* frame, Mode current, bool breakpoint) {
        const auto& lines = module.info->functions[function].lines;
        assert(statement < lines.size());
        // A step or breakpoint stops once per line: not again for the next statement of the same line.
        const bool sameLine = previous < lines.size() && previous != statement && lines[previous] == lines[statement];
        std::optional<NativeStopReason> reason;
        if (current == Mode::Pause) reason = NativeStopReason::Pause;
        else if (!sameLine) {
            const bool stepDone = current == Mode::StepIn || (current == Mode::StepOver && locals >= stepLocals) || (current == Mode::StepOut && locals > stepLocals);
            if (stepDone) reason = NativeStopReason::Step;
            else if (breakpoint) reason = NativeStopReason::Breakpoint;
        }
        if (!reason) return;
        std::unique_lock lock(mutex);
        auto walked = Walk(module.tables->functions[function].begin, frame);
        assert(!walked.empty() && walked.front().locals == locals);
        Stop(lock, {*reason, {}}, std::move(walked));
    }

    void OnError(std::string_view message) {
        std::unique_lock lock(mutex);
        if (!attached || !breakOnErrors || stopped) return;
        auto walked = ErrorFrames();
        if (walked.empty()) return; // not reported from script code
        Stop(lock, {NativeStopReason::Exception, std::string(message)}, std::move(walked));
    }

    // Runs on the script thread: calls the stop handler, then waits for a resume command.
    void Stop(std::unique_lock<std::mutex>& lock, const NativeStopEvent& event, std::vector<Frame> walked) {
        assert(!stopped);
        stopped = true;
        frames = std::move(walked);
        mode = Mode::Run;
        stoppedSignal.notify_all();
        const auto handler = stopHandler;
        lock.unlock();
        if (handler) handler(event);
        lock.lock();
        resumedSignal.wait(lock, [&] { return !stopped; });
    }

    void Resume(Mode next) {
        std::scoped_lock lock(mutex);
        if (!stopped) return;
        stepLocals = frames.empty() ? nullptr : frames.front().locals;
        mode = next;
        stopped = false;
        frames.clear();
        references.clear();
        copies.clear();
        resumedSignal.notify_all();
    }

    const Module* FindModule(const void* address) const {
        for (const auto& module : modules) if (module->Contains(address)) return module.get();
        return nullptr;
    }

    Frame FrameAt(const Module& module, size_t function, const std::byte* rbp) const {
        const auto offset = module.frameOffsets[function];
        if (offset == UnknownOffset) return {&module, function};
        const auto* locals = rbp + offset;
        return {&module, function, locals, Load<uint64_t>(locals)};
    }

    // The script frames from a frame of module code: rip lies in the function whose frame pointer is rbp. It follows the
    // rbp chain while the return addresses lie in module code. Runtime helpers in the module are not script frames.
    std::vector<Frame> Walk(const void* rip, const std::byte* rbp) const {
        std::vector<Frame> result;
        while (const auto* module = FindModule(rip)) {
            if (const auto function = module->FunctionAt(rip)) result.push_back(FrameAt(*module, *function, rbp));
            const auto* next = Load<const std::byte*>(rbp);
            rip = Load<const void*>(rbp + sizeof(void*));
            if (result.size() >= MaxFrames || next <= rbp || reinterpret_cast<uintptr_t>(next) % alignof(void*) != 0) break;
            rbp = next;
        }
        return result;
    }

    // The script frames below the C++ frames of a ReportError. Unwinds the C++ frames to the first return address in
    // module code; its rbp is then the frame pointer of that module function.
    std::vector<Frame> ErrorFrames() const {
        CONTEXT context{};
        RtlCaptureContext(&context);
        for (int depth = 0; depth < 64 && context.Rip; ++depth) {
            const auto* rip = reinterpret_cast<const void*>(context.Rip);
            if (FindModule(rip)) return Walk(rip, reinterpret_cast<const std::byte*>(context.Rbp));
            DWORD64 imageBase{};
            auto* entry = RtlLookupFunctionEntry(context.Rip, &imageBase, nullptr);
            if (!entry) return {};
            void* handlerData{};
            DWORD64 establisher{};
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context.Rip, entry, &context, &handlerData, &establisher, nullptr);
        }
        return {};
    }

    void Activate(const Module& module) const {
        auto& state = *module.tables->state;
        state.hook = Hook;
        state.context = const_cast<Module*>(&module);
        state.active = attached ? 1 : 0;
    }

    void ApplyBreakpoints(Module& module) const {
        size_t count = module.firstStatement.empty() ? 0 : module.firstStatement.back() + module.info->functions.back().lines.size();
        for (size_t i{}; i < count; ++i) module.breakpoints[i] = 0;
        for (const auto& [source, lines] : requestedBreakpoints) {
            if (!SameSource(source, module.info->sourceName)) continue;
            for (const auto line : lines) {
                const auto bound = module.BindLine(line);
                if (!bound) continue;
                for (size_t function{}; function < module.info->functions.size(); ++function) {
                    const auto& statementLines = module.info->functions[function].lines;
                    for (size_t statement{}; statement < statementLines.size(); ++statement)
                        if (statementLines[statement] == bound) module.breakpoints[module.firstStatement[function] + statement] = 1;
                }
            }
        }
    }

    // ---- values ----
    static bool IsInline(uint64_t kind) { return kind == SlotValue || kind == SlotFloat32 || kind == SlotInt32 || kind == SlotUnknown || kind == NullValue; }
    static std::string TypeName(uint64_t kind, const NativeTypeInfo* type, const NativeEnumInfo* enumInfo) {
        switch (kind) {
        case SlotValue: return "any";
        case SlotFloat32: return "float";
        case SlotInt32: return "int";
        case NullValue: return "null";
        case SlotUnknown: return "?";
        }
        switch (static_cast<NativeValueKind>(kind)) {
        case NativeValueKind::Int: return enumInfo ? enumInfo->name : "int";
        case NativeValueKind::Float: return "float";
        case NativeValueKind::Bool: return "bool";
        case NativeValueKind::String: return "string";
        case NativeValueKind::Table: return "table";
        case NativeValueKind::Closure: return "function";
        case NativeValueKind::Object: case NativeValueKind::Struct: return type ? type->name : "object";
        case NativeValueKind::List: return type ? type->name : "list"; // a list type info
        default: return "?";
        }
    }
    // The type of a value: the class of an object, and the element type of a list.
    static std::string SlotTypeName(const Slot& slot) {
        if (IsKind(slot.kind, NativeValueKind::Object)) if (const auto* object = Load<const std::byte*>(slot.address)) return Load<const NativeTypeInfo*>(object)->name;
        if (IsKind(slot.kind, NativeValueKind::List)) {
            if (const auto* list = Load<const NativeList*>(slot.address)) return TypeName(static_cast<uint64_t>(list->elementKind), static_cast<const NativeTypeInfo*>(list->elementType), nullptr) + "[]";
            return TypeName(slot.elementKind, slot.type, nullptr) + "[]";
        }
        return TypeName(slot.kind, slot.type, slot.enumInfo);
    }

    // The value an `any` slot holds, as a slot of its own kind.
    static Slot Unwrap(const Slot& slot) {
        if (slot.kind != SlotValue) return slot;
        const auto value = Load<NativeValue>(slot.address);
        if (value.kind == NativeValueKind::Null) return {slot.address, NullValue};
        if (value.kind == NativeValueKind::Struct) return {BoxValue(value.as.pointer), KindOf(value.kind), 0, &BoxType(value.as.pointer)};
        return {slot.address + offsetof(NativeValue, as), KindOf(value.kind)};
    }

    static std::string Summary(const Slot& slot) {
        switch (slot.kind) {
        case SlotFloat32: return Number(Load<float>(slot.address));
        case SlotInt32: return Number(Load<int32_t>(slot.address));
        case NullValue: return "null";
        case SlotUnknown: return "?";
        case SlotValue: return Summary(Unwrap(slot));
        }
        const auto* reference = Load<const std::byte*>(slot.address);
        switch (static_cast<NativeValueKind>(slot.kind)) {
        case NativeValueKind::Int: return slot.enumInfo ? EnumText(*slot.enumInfo, Load<int64_t>(slot.address)) : Number(Load<int64_t>(slot.address));
        case NativeValueKind::Float: return Number(Load<double>(slot.address));
        case NativeValueKind::Bool: return Load<unsigned char>(slot.address) ? "true" : "false";
        case NativeValueKind::String: return reference ? Quoted(reinterpret_cast<const RuntimeString*>(reference)) : "null";
        case NativeValueKind::Table: return reference ? "table" : "null";
        case NativeValueKind::Closure: return reference ? "function" : "null";
        case NativeValueKind::List: {
            if (!reference) return "null";
            const auto name = SlotTypeName(slot);
            return name.substr(0, name.size() - 2) + '[' + Number(reinterpret_cast<const NativeList*>(reference)->count) + ']';
        }
        case NativeValueKind::Object:
            if (!reference) return "null";
            if (IsProxy(*Load<const NativeTypeInfo*>(reference)) && !Load<NativeProxy>(reference).native) return "null (destroyed)";
            return SlotTypeName(slot);
        case NativeValueKind::Struct: {
            assert(slot.type);
            std::string result = std::string(slot.type->name) + '(';
            for (size_t i{}; i < slot.type->fieldCount; ++i) {
                if (i == MaxSummaryFields) { result += ", ..."; break; }
                const auto& field = slot.type->fields[i];
                if (i) result += ", ";
                result += IsKind(field.kind, NativeValueKind::Struct) ? std::string(field.type->name) : Summary({FieldSlot(slot.address, field), field.kind, field.elementKind, field.type, field.enumInfo});
            }
            return result + ')';
        }
        default: return "?";
        }
    }

    static bool HasChildren(const Slot& slot) {
        if (IsInline(slot.kind)) return false;
        const auto* reference = Load<const std::byte*>(slot.address);
        switch (static_cast<NativeValueKind>(slot.kind)) {
        case NativeValueKind::Struct: return slot.type->fieldCount > 0;
        case NativeValueKind::Object: {
            if (!reference) return false;
            const auto& type = *Load<const NativeTypeInfo*>(reference);
            return type.fieldCount > 0 && (!IsProxy(type) || Load<NativeProxy>(reference).native);
        }
        case NativeValueKind::List: return reference && reinterpret_cast<const NativeList*>(reference)->count > 0;
        case NativeValueKind::Table: return reference != nullptr;
        default: return false;
        }
    }

    std::vector<std::pair<std::string, Slot>> Children(const Slot& slot) {
        std::vector<std::pair<std::string, Slot>> result;
        if (!HasChildren(slot)) return result;
        switch (static_cast<NativeValueKind>(slot.kind)) {
        case NativeValueKind::Struct:
            for (const auto& field : Fields(*slot.type)) result.emplace_back(field.name, Slot{FieldSlot(slot.address, field), field.kind, field.elementKind, field.type, field.enumInfo});
            break;
        case NativeValueKind::Object: {
            const auto* object = Load<const std::byte*>(slot.address);
            for (const auto& field : Fields(*Load<const NativeTypeInfo*>(object))) result.emplace_back(field.name, SlotOf(object, field));
            break;
        }
        case NativeValueKind::List: {
            const auto& list = *Load<const NativeList*>(slot.address);
            const auto count = std::min<size_t>(static_cast<size_t>(list.count), MaxChildren);
            for (size_t i{}; i < count; ++i)
                result.emplace_back('[' + Number(i) + ']', Slot{list.data + i * list.elementSize, static_cast<uint64_t>(list.elementKind), 0, static_cast<const NativeTypeInfo*>(list.elementType)});
            break;
        }
        case NativeValueKind::Table: {
            auto* table = Load<void*>(slot.address);
            auto& abi = *static_cast<RuntimeAbi*>(runtime->Abi());
            unsigned long long cursor{};
            NativeValue key, value;
            while (result.size() < MaxChildren && abi.tableNext(abi.context, table, &cursor, &key, &value)) {
                const auto keySlot = Unwrap({reinterpret_cast<const std::byte*>(&copies.emplace_back(key)), SlotValue});
                std::string name;
                if (IsKind(keySlot.kind, NativeValueKind::String)) {
                    const auto* string = reinterpret_cast<const RuntimeString*>(Load<const void*>(keySlot.address));
                    name.assign(string->data, static_cast<size_t>(string->length));
                } else name = '[' + Summary(keySlot) + ']';
                result.emplace_back(std::move(name), Slot{reinterpret_cast<const std::byte*>(&copies.emplace_back(value)), SlotValue});
            }
            break;
        }
        default: break;
        }
        return result;
    }

    NativeDebugVariable Describe(std::string name, std::string type, const Slot& stored) {
        const auto slot = Unwrap(stored);
        // An `any` shows the type of its value; a function type has no readable spelling.
        if (stored.kind == SlotValue) type = "any: " + SlotTypeName(slot);
        else if (type.empty() || type.starts_with('$')) type = SlotTypeName(slot);
        NativeDebugVariable variable{std::move(name), std::move(type), Summary(slot), 0};
        if (HasChildren(slot)) {
            references.push_back(slot);
            variable.children = references.size();
        }
        return variable;
    }
};

NativeDebugger::NativeDebugger(std::shared_ptr<NativeRuntime> runtime) : impl_(std::make_unique<Impl>()) {
    assert(runtime);
    impl_->runtime = std::move(runtime);
    impl_->runtime->SetErrorHandler([impl = impl_.get()](std::string_view message) { impl->OnError(message); });
}

NativeDebugger::~NativeDebugger() {
    Detach();
    impl_->runtime->SetErrorHandler({});
    std::scoped_lock lock(impl_->mutex);
    for (const auto& module : impl_->modules) *module->tables->state = {};
}

void NativeDebugger::AddModule(const NativeModule& module) {
    const auto* tables = module.Debug();
    assert(tables && "the module is not instrumented");
    assert(module.Runtime() == impl_->runtime);
    std::scoped_lock lock(impl_->mutex);
    auto record = std::make_unique<Impl::Module>();
    record->debugger = impl_.get();
    record->tables = tables;
    record->info = &module.Generated().debug;
    size_t statements{};
    for (const auto& function : record->info->functions) { record->firstStatement.push_back(statements); statements += function.lines.size(); }
    record->breakpoints = std::make_unique<std::atomic<uint8_t>[]>(statements);
    record->frameOffsets.assign(record->info->functions.size(), UnknownOffset);
    impl_->ApplyBreakpoints(*record);
    impl_->Activate(*record);
    impl_->modules.push_back(std::move(record));
}

void NativeDebugger::RemoveModule(const NativeModule& module) {
    std::scoped_lock lock(impl_->mutex);
    const auto found = std::ranges::find(impl_->modules, module.Debug(), [](const auto& record) { return record->tables; });
    assert(found != impl_->modules.end());
    assert(!impl_->stopped || std::ranges::none_of(impl_->frames, [&](const Impl::Frame& frame) { return frame.module == found->get(); }));
    *(*found)->tables->state = {};
    impl_->modules.erase(found);
}

void NativeDebugger::Attach() {
    std::scoped_lock lock(impl_->mutex);
    impl_->attached = true;
    for (const auto& module : impl_->modules) module->tables->state->active = 1;
}

void NativeDebugger::Detach() {
    {
        std::scoped_lock lock(impl_->mutex);
        impl_->attached = false;
        for (const auto& module : impl_->modules) module->tables->state->active = 0;
    }
    impl_->Resume(Mode::Run);
    impl_->mode = Mode::Run;
}

bool NativeDebugger::Attached() const { std::scoped_lock lock(impl_->mutex); return impl_->attached; }
void NativeDebugger::SetBreakOnErrors(bool enabled) { std::scoped_lock lock(impl_->mutex); impl_->breakOnErrors = enabled; }
void NativeDebugger::SetStopHandler(std::function<void(const NativeStopEvent&)> handler) { std::scoped_lock lock(impl_->mutex); impl_->stopHandler = std::move(handler); }

std::vector<uint32_t> NativeDebugger::SetBreakpoints(std::string_view source, const std::vector<uint32_t>& lines) {
    std::scoped_lock lock(impl_->mutex);
    impl_->requestedBreakpoints[std::string(source)] = lines;
    std::vector<uint32_t> bound(lines.size());
    for (const auto& module : impl_->modules) {
        impl_->ApplyBreakpoints(*module);
        if (!SameSource(source, module->info->sourceName)) continue;
        for (size_t i{}; i < lines.size(); ++i)
            if (const auto line = module->BindLine(lines[i]); line && (!bound[i] || line < bound[i])) bound[i] = line;
    }
    return bound;
}

void NativeDebugger::Pause() {
    std::scoped_lock lock(impl_->mutex);
    if (!impl_->stopped) impl_->mode = Mode::Pause;
}
void NativeDebugger::Continue() { impl_->Resume(Mode::Run); }
void NativeDebugger::StepIn() { impl_->Resume(Mode::StepIn); }
void NativeDebugger::StepOver() { impl_->Resume(Mode::StepOver); }
void NativeDebugger::StepOut() { impl_->Resume(Mode::StepOut); }

bool NativeDebugger::Stopped() const { std::scoped_lock lock(impl_->mutex); return impl_->stopped; }

bool NativeDebugger::WaitUntilStopped(std::chrono::milliseconds timeout) const {
    std::unique_lock lock(impl_->mutex);
    return impl_->stoppedSignal.wait_for(lock, timeout, [&] { return impl_->stopped; });
}

std::vector<NativeDebugFrame> NativeDebugger::StackTrace() const {
    std::scoped_lock lock(impl_->mutex);
    std::vector<NativeDebugFrame> result;
    for (const auto& frame : impl_->frames) {
        const auto& function = frame.module->info->functions[frame.function];
        const uint32_t line = frame.statement < function.lines.size() ? function.lines[frame.statement] : 0;
        result.push_back({function.name, frame.module->info->sourceName, line});
    }
    return result;
}

std::vector<NativeDebugVariable> NativeDebugger::Locals(size_t frameIndex) {
    std::scoped_lock lock(impl_->mutex);
    std::vector<NativeDebugVariable> result;
    if (frameIndex >= impl_->frames.size()) return result;
    const auto& frame = impl_->frames[frameIndex];
    if (!frame.locals || frame.statement == NoStatement) return result;
    const auto& locals = frame.module->info->functions[frame.function].locals;
    const auto& range = frame.module->tables->functions[frame.function];
    assert(range.localCount == locals.size());
    for (size_t i{}; i < locals.size(); ++i) {
        const auto& local = locals[i];
        if (frame.statement < local.visibleFrom || frame.statement >= local.visibleTo) continue;
        auto slot = SlotOf(frame.locals, range.locals[i]);
        if (range.locals[i].flags & FieldIndirect) {
            slot.address = Load<const std::byte*>(slot.address);
            if (!slot.address) { result.push_back({local.name, local.type, "null", 0}); continue; }
        }
        result.push_back(impl_->Describe(local.name, local.type, slot));
    }
    return result;
}

std::vector<NativeDebugVariable> NativeDebugger::Variables(uint64_t reference) {
    std::scoped_lock lock(impl_->mutex);
    std::vector<NativeDebugVariable> result;
    if (!impl_->stopped || reference == 0 || reference > impl_->references.size()) return result;
    const auto slot = impl_->references[reference - 1];
    for (auto& [name, child] : impl_->Children(slot)) result.push_back(impl_->Describe(std::move(name), {}, child));
    return result;
}

} // namespace luasharp
