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
#include <thread>

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
// A named value: a local, a field, an element or a property. error: the property getter failed, and slot is not valid.
struct Child {
    std::string name;
    std::string type; // the declared type; empty to show the type of the value
    Slot slot;
    std::string error;
};

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
// The extern class of an object: the type of a proxy, or the nearest extern ancestor of a script class.
const NativeTypeInfo* NativeClassOf(const NativeTypeInfo* type) {
    while (type && !(type->flags & TypeNative)) type = type->base;
    return type;
}

bool IsIdentifier(char c) { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

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
        bool InSource(const NativeDebugFunction& function, std::string_view source) const {
            assert(function.source < info->sources.size());
            return SameSource(source, info->sources[function.source]);
        }
        // The line that a breakpoint on line of source binds to: the first statement line at or after it; 0 when none.
        uint32_t BindLine(std::string_view source, uint32_t line) const {
            uint32_t best = 0;
            for (const auto& function : info->functions)
                if (InSource(function, source))
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
    std::deque<std::vector<std::byte>> results; // while stopped: property values that references point into
    std::thread::id scriptThread;   // while stopped
    std::function<void()> task;     // inspection that another thread gives to the stopped script thread
    std::condition_variable taskSignal;
    std::atomic<bool> evaluating{}; // a property getter runs: hooks do not stop, and an error goes to evaluationError
    std::optional<std::string> evaluationError;

    static void Hook(void* context, void* locals, void* frame, uint64_t function, uint64_t statement) {
        auto& module = *static_cast<Module*>(context);
        auto& current = *static_cast<uint64_t*>(locals);
        const auto previous = std::exchange(current, statement);
        module.frameOffsets[function] = static_cast<std::byte*>(locals) - static_cast<std::byte*>(frame);
        auto& self = *module.debugger;
        if (self.evaluating.load(std::memory_order_relaxed)) return;
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
        // A getter runs on the stopped script thread, which holds the lock.
        if (evaluating) { evaluationError = std::string(message); return; }
        std::unique_lock lock(mutex);
        if (!attached || !breakOnErrors || stopped) return;
        auto walked = ErrorFrames();
        if (walked.empty()) return; // not reported from script code
        Stop(lock, {NativeStopReason::Exception, std::string(message)}, std::move(walked));
    }

    // Runs on the script thread: calls the stop handler, then runs inspection tasks until a resume command.
    void Stop(std::unique_lock<std::mutex>& lock, const NativeStopEvent& event, std::vector<Frame> walked) {
        assert(!stopped);
        stopped = true;
        scriptThread = std::this_thread::get_id();
        frames = std::move(walked);
        mode = Mode::Run;
        stoppedSignal.notify_all();
        const auto handler = stopHandler;
        lock.unlock();
        if (handler) handler(event);
        lock.lock();
        for (;;) {
            resumedSignal.wait(lock, [&] { return !stopped || task; });
            if (!stopped) break;
            std::exchange(task, {})();
            taskSignal.notify_all();
        }
    }

    // Runs inspect on the stopped script thread, under the lock, so that host getters run on the thread of the scripts.
    // Gives an empty result when the script resumes first.
    template <typename F> auto OnScriptThread(std::unique_lock<std::mutex>& lock, F inspect) -> decltype(inspect()) {
        if (!stopped || std::this_thread::get_id() == scriptThread) return inspect();
        taskSignal.wait(lock, [&] { return !task || !stopped; });
        if (!stopped) return {};
        std::optional<decltype(inspect())> result;
        task = [&] { result = inspect(); };
        resumedSignal.notify_all();
        taskSignal.wait(lock, [&] { return result || !stopped; });
        if (!result) return {};
        return std::move(*result);
    }

    void Resume(Mode next) {
        std::scoped_lock lock(mutex);
        if (!stopped) return;
        stepLocals = frames.empty() ? nullptr : frames.front().locals;
        mode = next;
        stopped = false;
        task = {};
        frames.clear();
        references.clear();
        copies.clear();
        results.clear();
        resumedSignal.notify_all();
        taskSignal.notify_all();
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
            for (const auto line : lines) {
                const auto bound = module.BindLine(source, line);
                if (!bound) continue;
                for (size_t function{}; function < module.info->functions.size(); ++function) {
                    if (!module.InSource(module.info->functions[function], source)) continue;
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

    const NativeDebugClass* FindDebugClass(std::string_view name) const {
        for (const auto& module : modules)
            for (uint64_t i{}; i < module->tables->classCount; ++i) if (name == module->tables->classes[i].name) return &module->tables->classes[i];
        return nullptr;
    }
    // The properties of the extern class of an object and of its extern bases; the most derived first.
    std::vector<const NativeDebugProperty*> Properties(const NativeTypeInfo& objectType) const {
        std::vector<const NativeDebugProperty*> result;
        for (const auto* type = NativeClassOf(&objectType); type; type = type->base)
            if (const auto* found = FindDebugClass(type->name)) for (uint64_t i{}; i < found->propertyCount; ++i) result.push_back(&found->properties[i]);
        return result;
    }
    // Calls the host getter of a live extern object. The value stays until resume; GC runs only between script calls.
    Child Read(const NativeDebugProperty& property, const std::byte* object) {
        assert(Load<NativeProxy>(object).native && "a getter needs a live object");
        const auto& value = property.value;
        const size_t size = std::max<size_t>(sizeof(NativeValue), IsKind(value.kind, NativeValueKind::Struct) ? static_cast<size_t>(value.type->size) : 0);
        auto& buffer = results.emplace_back(size);
        evaluationError.reset();
        const bool hadError = !runtime->LastError().empty(); // the runtime keeps its first error
        evaluating = true;
        property.get(const_cast<std::byte*>(object), buffer.data());
        evaluating = false;
        if (evaluationError && !hadError) runtime->ClearError(); // the script does not see the error of a getter
        Child child{value.name, {}, SlotOf(buffer.data(), value)};
        if (evaluationError) child.error = *std::exchange(evaluationError, std::nullopt);
        return child;
    }
    // `Entity "player"`: the name of an extern object, from a string property name, get_name() or get_editor_name().
    std::string ObjectSummary(const std::byte* object) {
        const auto& type = *Load<const NativeTypeInfo*>(object);
        if (!IsProxy(type)) return type.name;
        for (const auto* property : Properties(type)) {
            const std::string_view name = property->value.name;
            if (!IsKind(property->value.kind, NativeValueKind::String) || (name != "name" && name != "get_name()" && name != "get_editor_name()")) continue;
            const auto child = Read(*property, object);
            const auto* string = child.error.empty() ? Load<const RuntimeString*>(child.slot.address) : nullptr;
            if (string && string->length > 0) return std::string(type.name) + ' ' + Quoted(string);
        }
        return type.name;
    }

    std::string Summary(const Slot& slot) {
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
            return ObjectSummary(reference);
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

    bool HasChildren(const Slot& slot) const {
        if (IsInline(slot.kind)) return false;
        const auto* reference = Load<const std::byte*>(slot.address);
        switch (static_cast<NativeValueKind>(slot.kind)) {
        case NativeValueKind::Struct: return slot.type->fieldCount > 0;
        case NativeValueKind::Object: {
            if (!reference) return false;
            const auto& type = *Load<const NativeTypeInfo*>(reference);
            if (!IsProxy(type)) return type.fieldCount > 0;
            return Load<NativeProxy>(reference).native && (type.fieldCount > 0 || !Properties(type).empty());
        }
        case NativeValueKind::List: return reference && reinterpret_cast<const NativeList*>(reference)->count > 0;
        case NativeValueKind::Table: return reference != nullptr;
        default: return false;
        }
    }

    // An extern object: its script fields, then its properties.
    std::vector<Child> Children(const Slot& slot) {
        std::vector<Child> result;
        if (!HasChildren(slot)) return result;
        switch (static_cast<NativeValueKind>(slot.kind)) {
        case NativeValueKind::Struct:
            for (const auto& field : Fields(*slot.type)) result.push_back({field.name, {}, Slot{FieldSlot(slot.address, field), field.kind, field.elementKind, field.type, field.enumInfo}});
            break;
        case NativeValueKind::Object: {
            const auto* object = Load<const std::byte*>(slot.address);
            const auto& type = *Load<const NativeTypeInfo*>(object);
            for (const auto& field : Fields(type)) result.push_back({field.name, {}, SlotOf(object, field)});
            if (IsProxy(type)) for (const auto* property : Properties(type)) result.push_back(Read(*property, object));
            break;
        }
        case NativeValueKind::List: {
            const auto& list = *Load<const NativeList*>(slot.address);
            const auto count = std::min<size_t>(static_cast<size_t>(list.count), MaxChildren);
            for (size_t i{}; i < count; ++i)
                result.push_back({'[' + Number(i) + ']', {}, Slot{list.data + i * list.elementSize, static_cast<uint64_t>(list.elementKind), 0, static_cast<const NativeTypeInfo*>(list.elementType)}});
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
                result.push_back({std::move(name), {}, Slot{reinterpret_cast<const std::byte*>(&copies.emplace_back(value)), SlotValue}});
            }
            break;
        }
        default: break;
        }
        return result;
    }

    NativeDebugVariable Describe(Child child) {
        if (!child.error.empty()) return {std::move(child.name), std::move(child.type), "<error: " + child.error + '>', 0};
        return Describe(std::move(child.name), std::move(child.type), child.slot);
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

    // The visible locals of a stopped frame.
    std::vector<Child> LocalSlots(size_t frameIndex) const {
        std::vector<Child> result;
        if (frameIndex >= frames.size()) return result;
        const auto& frame = frames[frameIndex];
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
                if (!slot.address) slot = {nullptr, NullValue};
            }
            result.push_back({local.name, local.type, slot});
        }
        return result;
    }

    // The top-level variables and static fields ("Type.field") of the module of a stopped frame.
    std::vector<Child> StaticSlots(size_t frameIndex) const {
        std::vector<Child> result;
        if (frameIndex >= frames.size()) return result;
        const auto& tables = *frames[frameIndex].module->tables;
        assert(tables.staticCount == 0 || (tables.statics && tables.staticsBase));
        const auto* base = static_cast<const std::byte*>(tables.staticsBase);
        for (uint64_t i{}; i < tables.staticCount; ++i) result.push_back({tables.statics[i].name, {}, SlotOf(base, tables.statics[i])});
        return result;
    }
    // "Type" of a frame in a method "Type.name"; empty for top-level code and lambdas.
    std::string_view FrameOwner(size_t frameIndex) const {
        if (frameIndex >= frames.size()) return {};
        const std::string_view function = frames[frameIndex].module->info->functions[frames[frameIndex].function].name;
        const auto dot = function.find('.');
        return dot == std::string_view::npos ? std::string_view() : function.substr(0, dot);
    }

    // A child by name; a method matches without its "()".
    std::optional<Child> FindChild(const Slot& parent, std::string_view name) {
        for (auto& child : Children(Unwrap(parent)))
            if (child.name == name || (child.name.ends_with("()") && std::string_view(child.name).substr(0, child.name.size() - 2) == name)) return std::move(child);
        return std::nullopt;
    }

    NativeEvaluation Evaluate(size_t frameIndex, std::string_view expression) {
        size_t at{};
        const auto skipSpaces = [&] { while (at < expression.size() && expression[at] == ' ') ++at; };
        const auto name = [&] {
            skipSpaces();
            const auto start = at;
            while (at < expression.size() && IsIdentifier(expression[at])) ++at;
            auto result = std::string(expression.substr(start, at - start));
            if (expression.substr(at).starts_with("()")) { at += 2; result += "()"; }
            return result;
        };
        const auto fail = [](std::string message) { return NativeEvaluation{std::nullopt, std::move(message)}; };

        const auto first = name();
        if (first.empty()) return fail("expected a name");
        auto locals = LocalSlots(frameIndex);
        std::optional<Child> current;
        if (const auto local = std::ranges::find(locals, first, &Child::name); local != locals.end()) current = std::move(*local);
        else if (const auto self = std::ranges::find(locals, "this", &Child::name); self != locals.end()) current = FindChild(self->slot, first);
        if (!current) {
            // A static: "name", a static of the frame's class without "Type.", or "Type.field".
            const auto statics = StaticSlots(frameIndex);
            const auto findStatic = [&](std::string_view staticName) -> std::optional<Child> {
                const auto found = std::ranges::find(statics, staticName, &Child::name);
                return found == statics.end() ? std::nullopt : std::optional(*found);
            };
            current = findStatic(first);
            if (const auto owner = FrameOwner(frameIndex); !current && !owner.empty()) current = findStatic(std::string(owner) + '.' + first);
            if (!current && expression.substr(at).starts_with('.')) {
                const auto mark = at++;
                current = findStatic(first + '.' + name());
                if (!current) at = mark;
            }
        }
        if (!current) return fail("'" + first + "' is not a local, a member of this or a static");
        for (skipSpaces(); at < expression.size(); skipSpaces()) {
            if (!current->error.empty()) break;
            std::string member;
            if (expression[at] == '.') { ++at; member = name(); if (member.empty()) return fail("expected a member after '.'"); }
            else if (expression[at] == '[') {
                const auto close = expression.find(']', at);
                if (close == std::string_view::npos) return fail("expected ']'");
                member = '[' + std::string(expression.substr(at + 1, close - at - 1)) + ']';
                at = close + 1;
            } else return fail("unexpected '" + std::string(1, expression[at]) + "'");
            auto next = FindChild(current->slot, member);
            if (!next) return fail("'" + member + "' is not a member of " + SlotTypeName(Unwrap(current->slot)));
            current = std::move(next);
        }
        if (!current->error.empty()) return fail(current->error);
        current->name = std::string(expression);
        return {Describe(std::move(*current)), {}};
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
        for (size_t i{}; i < lines.size(); ++i)
            if (const auto line = module->BindLine(source, lines[i]); line && (!bound[i] || line < bound[i])) bound[i] = line;
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
        result.push_back({function.name, frame.module->info->sources[function.source], line});
    }
    return result;
}

std::vector<NativeDebugVariable> NativeDebugger::Locals(size_t frameIndex) {
    std::unique_lock lock(impl_->mutex);
    return impl_->OnScriptThread(lock, [&] {
        std::vector<NativeDebugVariable> result;
        for (auto& local : impl_->LocalSlots(frameIndex)) result.push_back(impl_->Describe(std::move(local)));
        return result;
    });
}

std::vector<NativeDebugVariable> NativeDebugger::Statics(size_t frameIndex) {
    std::unique_lock lock(impl_->mutex);
    return impl_->OnScriptThread(lock, [&] {
        std::vector<NativeDebugVariable> result;
        for (auto& value : impl_->StaticSlots(frameIndex)) result.push_back(impl_->Describe(std::move(value)));
        return result;
    });
}

std::vector<NativeDebugVariable> NativeDebugger::Variables(uint64_t reference) {
    std::unique_lock lock(impl_->mutex);
    return impl_->OnScriptThread(lock, [&] {
        std::vector<NativeDebugVariable> result;
        if (!impl_->stopped || reference == 0 || reference > impl_->references.size()) return result;
        const auto slot = impl_->references[reference - 1];
        for (auto& child : impl_->Children(slot)) result.push_back(impl_->Describe(std::move(child)));
        return result;
    });
}

NativeEvaluation NativeDebugger::Evaluate(size_t frame, std::string_view expression) {
    std::unique_lock lock(impl_->mutex);
    if (!impl_->stopped) return {std::nullopt, "the script is running"};
    auto result = impl_->OnScriptThread(lock, [&] { return impl_->Evaluate(frame, expression); });
    if (!result.value && result.error.empty()) result.error = "the script resumed";
    return result;
}

} // namespace luasharp
