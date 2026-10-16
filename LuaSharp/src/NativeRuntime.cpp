#include "NativeRuntime.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace luasharp {
namespace {

struct Allocation {
    Allocation* next{};
    size_t size{};
    NativeTraceFunction trace{};
    NativeFinalizeFunction finalize{};
    bool marked{};
};

struct RuntimeString {
    std::int64_t length{};
    char data[1]{};
};

struct RuntimeAbi {
    unsigned long long abiVersion{4};
    void* context{};
    void* (*allocate)(void*, unsigned long long, NativeTraceFunction){};
    void (*mark)(void*, void*){};
    void* (*intern)(void*, const char*){};
    void* (*tableCreate)(void*, unsigned long long, const NativeValue*){};
    void (*tableGet)(void*, void*, const NativeValue*, NativeValue*){};
    void (*tableSet)(void*, void*, const NativeValue*, const NativeValue*){};
    void (*print)(void*, void*, unsigned long long, const NativeValue*){};
    void (*reportError)(void*, const char*){};
};

struct RuntimeTable { std::vector<std::pair<NativeValue, NativeValue>> entries; };

bool ValueEqual(const NativeValue& left, const NativeValue& right) {
    if (left.kind != right.kind) return false;
    switch (left.kind) {
    case NativeValueKind::Null: return true;
    case NativeValueKind::Int: return left.as.integer == right.as.integer;
    case NativeValueKind::Float: return left.as.number == right.as.number;
    case NativeValueKind::Bool: return left.as.boolean == right.as.boolean;
    default: return left.as.pointer == right.as.pointer;
    }
}

bool IsManaged(const NativeValue& value) {
    return value.kind == NativeValueKind::String || value.kind == NativeValueKind::Object || value.kind == NativeValueKind::Table || value.kind == NativeValueKind::Closure;
}

void* Payload(Allocation* allocation) { return allocation + 1; }
Allocation* Header(void* payload) { return static_cast<Allocation*>(payload) - 1; }

} // namespace

struct NativeRuntime::Impl {
    mutable std::recursive_mutex mutex;
    Allocation* allocations{};
    size_t allocationCount{};
    size_t allocatedBytes{};
    std::unordered_map<void*, size_t> pins;
    std::unordered_map<size_t, NativeRootTraceFunction> roots;
    std::unordered_map<std::string, void*> strings;
    std::unordered_map<void*, std::string> stringKeys;
    size_t nextRoot{1};
    std::string error;
    RuntimeAbi abi;
};

namespace {

void* AbiAllocate(void* context, unsigned long long size, NativeTraceFunction trace) {
    return static_cast<NativeRuntime*>(context)->Allocate(static_cast<size_t>(size), trace);
}
void AbiMark(void* context, void* object) { static_cast<NativeRuntime*>(context)->Mark(object); }
void* AbiIntern(void* context, const char* string) { return static_cast<NativeRuntime*>(context)->Intern(string ? std::string_view(string) : std::string_view{}); }
void TraceTable(void* context, void* object) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    for (const auto& [key, value] : static_cast<RuntimeTable*>(object)->entries) {
        if (IsManaged(key)) runtime.Mark(key.as.pointer);
        if (IsManaged(value)) runtime.Mark(value.as.pointer);
    }
}
void FinalizeTable(void* object) { static_cast<RuntimeTable*>(object)->~RuntimeTable(); }
void* AbiTableCreate(void* context, unsigned long long count, const NativeValue* pairs) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    auto* table = static_cast<RuntimeTable*>(runtime.Allocate(sizeof(RuntimeTable), TraceTable, FinalizeTable));
    if (!table) return nullptr;
    new (table) RuntimeTable();
    for (unsigned long long i{}; i < count; ++i) table->entries.emplace_back(pairs[i * 2], pairs[i * 2 + 1]);
    return table;
}
void AbiTableGet(void*, void* object, const NativeValue* key, NativeValue* out) {
    *out = {};
    if (!object || !key) return;
    const auto& entries = static_cast<RuntimeTable*>(object)->entries;
    for (auto item = entries.rbegin(); item != entries.rend(); ++item) if (ValueEqual(item->first, *key)) { *out = item->second; return; }
}
void AbiTableSet(void*, void* object, const NativeValue* key, const NativeValue* value) {
    if (!object || !key || !value) return;
    auto& entries = static_cast<RuntimeTable*>(object)->entries;
    for (auto& item : entries) if (ValueEqual(item.first, *key)) { item.second = *value; return; }
    entries.emplace_back(*key, *value);
}
void AbiPrint(void* context, void* formatObject, unsigned long long count, const NativeValue* arguments) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    const auto* format = static_cast<const RuntimeString*>(formatObject);
    if (!format) { runtime.ReportError("printf format cannot be null"); return; }
    unsigned long long argument{};
    for (std::int64_t i{}; i < format->length; ++i) {
        const char character = format->data[i];
        if (character != '%' || i + 1 >= format->length) { std::cout.put(character); continue; }
        const char specifier = format->data[++i];
        if (specifier == '%') { std::cout.put('%'); continue; }
        if (argument >= count) { runtime.ReportError("printf has fewer values than format specifiers"); return; }
        const auto& value = arguments[argument++];
        switch (specifier) {
        case 'd': case 'i':
            if (value.kind != NativeValueKind::Int) { runtime.ReportError("printf %d expects an int"); return; }
            std::cout << value.as.integer; break;
        case 'f':
            if (value.kind == NativeValueKind::Int) std::cout << static_cast<double>(value.as.integer);
            else if (value.kind == NativeValueKind::Float) std::cout << value.as.number;
            else { runtime.ReportError("printf %f expects an int or float"); return; }
            break;
        case 's': {
            if (value.kind != NativeValueKind::String || !value.as.pointer) { runtime.ReportError("printf %s expects a string"); return; }
            const auto* string = static_cast<const RuntimeString*>(value.as.pointer); std::cout.write(string->data, string->length); break;
        }
        case 'b':
            if (value.kind != NativeValueKind::Bool) { runtime.ReportError("printf %b expects a bool"); return; }
            std::cout << (value.as.boolean ? "true" : "false"); break;
        case 'p':
            if (!IsManaged(value) && value.kind != NativeValueKind::Null) { runtime.ReportError("printf %p expects a reference"); return; }
            std::cout << value.as.pointer; break;
        default: runtime.ReportError(std::string("unsupported printf format specifier %") + specifier); return;
        }
    }
    if (argument != count) runtime.ReportError("printf has more values than format specifiers");
    std::cout.flush();
}
void AbiReportError(void* context, const char* message) { static_cast<NativeRuntime*>(context)->ReportError(message ? message : "unknown native runtime error"); }

} // namespace

NativeRuntime::NativeRuntime() : impl_(std::make_unique<Impl>()) {
    impl_->abi = {4, this, AbiAllocate, AbiMark, AbiIntern, AbiTableCreate, AbiTableGet, AbiTableSet, AbiPrint, AbiReportError};
}

NativeRuntime::~NativeRuntime() {
    auto* allocation = impl_->allocations;
    while (allocation) { auto* next = allocation->next; if (allocation->finalize) allocation->finalize(Payload(allocation)); std::free(allocation); allocation = next; }
}

std::shared_ptr<NativeRuntime> NativeRuntime::Create() { return std::make_shared<NativeRuntime>(); }

void* NativeRuntime::Allocate(size_t size, NativeTraceFunction trace, NativeFinalizeFunction finalize) {
    std::scoped_lock lock(impl_->mutex);
    const size_t actual = size == 0 ? 1 : size;
    auto* allocation = static_cast<Allocation*>(std::calloc(1, sizeof(Allocation) + actual));
    if (!allocation) { ReportError("LuaSharp allocation failed"); return nullptr; }
    allocation->next = impl_->allocations; allocation->size = actual; allocation->trace = trace; allocation->finalize = finalize;
    impl_->allocations = allocation; ++impl_->allocationCount; impl_->allocatedBytes += actual;
    return Payload(allocation);
}

void NativeRuntime::Mark(void* object) {
    if (!object) return;
    std::scoped_lock lock(impl_->mutex);
    auto* allocation = Header(object);
    bool found = false;
    for (auto* current = impl_->allocations; current; current = current->next) if (current == allocation) { found = true; break; }
    if (!found || allocation->marked) return;
    allocation->marked = true;
    if (allocation->trace) allocation->trace(this, object);
}

size_t NativeRuntime::Collect() {
    std::scoped_lock lock(impl_->mutex);
    for (auto* current = impl_->allocations; current; current = current->next) current->marked = false;
    for (const auto& [object, count] : impl_->pins) if (count) Mark(object);
    for (const auto& [id, trace] : impl_->roots) { static_cast<void>(id); if (trace) trace(this); }

    size_t reclaimed{};
    Allocation** link = &impl_->allocations;
    while (*link) {
        auto* current = *link;
        if (current->marked) { link = &current->next; continue; }
        *link = current->next;
        if (const auto key = impl_->stringKeys.find(Payload(current)); key != impl_->stringKeys.end()) {
            impl_->strings.erase(key->second);
            impl_->stringKeys.erase(key);
        }
        if (current->finalize) current->finalize(Payload(current));
        --impl_->allocationCount; impl_->allocatedBytes -= current->size; ++reclaimed; std::free(current);
    }
    return reclaimed;
}

void NativeRuntime::Pin(void* object) { if (!object) return; std::scoped_lock lock(impl_->mutex); ++impl_->pins[object]; }
void NativeRuntime::Unpin(void* object) {
    if (!object) return; std::scoped_lock lock(impl_->mutex); const auto found = impl_->pins.find(object);
    if (found == impl_->pins.end()) return; if (found->second <= 1) impl_->pins.erase(found); else --found->second;
}

size_t NativeRuntime::RegisterRoots(NativeRootTraceFunction trace) {
    if (!trace) throw std::runtime_error("Native root tracer cannot be null");
    std::scoped_lock lock(impl_->mutex); const auto id = impl_->nextRoot++; impl_->roots.emplace(id, trace); return id;
}
void NativeRuntime::UnregisterRoots(size_t registration) { std::scoped_lock lock(impl_->mutex); impl_->roots.erase(registration); }

void* NativeRuntime::Intern(std::string_view utf8) {
    std::scoped_lock lock(impl_->mutex);
    if (const auto found = impl_->strings.find(std::string(utf8)); found != impl_->strings.end()) return found->second;
    auto* value = static_cast<RuntimeString*>(Allocate(sizeof(RuntimeString) + utf8.size(), nullptr));
    if (!value) return nullptr;
    value->length = static_cast<std::int64_t>(utf8.size()); std::memcpy(value->data, utf8.data(), utf8.size()); value->data[utf8.size()] = '\0';
    auto key = std::string(utf8);
    impl_->strings.emplace(key, value);
    impl_->stringKeys.emplace(value, std::move(key));
    return value;
}

size_t NativeRuntime::AllocationCount() const { std::scoped_lock lock(impl_->mutex); return impl_->allocationCount; }
size_t NativeRuntime::AllocatedBytes() const { std::scoped_lock lock(impl_->mutex); return impl_->allocatedBytes; }
void NativeRuntime::ReportError(std::string_view message) { std::scoped_lock lock(impl_->mutex); if (impl_->error.empty()) impl_->error = message; }
std::string NativeRuntime::LastError() const { std::scoped_lock lock(impl_->mutex); return impl_->error; }
void NativeRuntime::ClearError() { std::scoped_lock lock(impl_->mutex); impl_->error.clear(); }
void* NativeRuntime::Abi() { return &impl_->abi; }

} // namespace luasharp
