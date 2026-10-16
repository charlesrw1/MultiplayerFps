#include "NativeRuntime.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
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

constexpr unsigned long long AbiVersion = 6;

// Field order must match LS_RuntimeApi in the generated C preamble.
struct RuntimeAbi {
    unsigned long long abiVersion{AbiVersion};
    void* context{};
    void* (*allocate)(void*, unsigned long long, NativeTraceFunction){};
    void (*mark)(void*, void*){};
    void* (*intern)(void*, const char*){};
    void* (*tableCreate)(void*, unsigned long long, const NativeValue*){};
    void (*tableGet)(void*, void*, const NativeValue*, NativeValue*){};
    void (*tableSet)(void*, void*, const NativeValue*, const NativeValue*){};
    void (*print)(void*, void*, unsigned long long, const NativeValue*){};
    void (*reportError)(void*, const char*){};
    int (*tableNext)(void*, void*, unsigned long long*, NativeValue*, NativeValue*){};
    void* (*listCreate)(void*, unsigned long long, unsigned long long, unsigned long long){};
    void (*listReserve)(void*, void*, unsigned long long){};
    void* (*listSlice)(void*, void*, std::int64_t, std::int64_t){};
    void* (*stringConcat)(void*, void*, void*){};
    void* (*stringSubstring)(void*, void*, std::int64_t, std::int64_t, int){};
    void* (*stringCase)(void*, void*, int){};
    void* (*toString)(void*, const NativeValue*){};
    void (*toNumber)(void*, void*, NativeValue*){};
};

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

struct KeyHash {
    size_t operator()(const NativeValue& key) const {
        std::uint64_t bits; std::memcpy(&bits, &key.as, sizeof(bits));
        return std::hash<std::uint64_t>{}(bits ^ (static_cast<std::uint64_t>(key.kind) << 59));
    }
};
struct KeyEqual { bool operator()(const NativeValue& left, const NativeValue& right) const { return ValueEqual(left, right); } };

// Entries keep insertion order for iteration. Writing nil leaves a tombstone, so cursors stay valid.
struct RuntimeTable {
    std::vector<std::pair<NativeValue, NativeValue>> entries;
    std::unordered_map<NativeValue, size_t, KeyHash, KeyEqual> slots; // key -> entries index
};
// header must stay first: generated code reads it as LS_List.
struct RuntimeList {
    NativeList header;
    std::vector<std::byte> storage;
};

// Lua keys: a float with an integer value is the same key as that integer.
NativeValue NormalizeKey(NativeValue key) {
    constexpr double limit = 9223372036854775808.0; // 2^63
    if (key.kind == NativeValueKind::Float && key.as.number >= -limit && key.as.number < limit) {
        const auto integer = static_cast<std::int64_t>(key.as.number);
        if (static_cast<double>(integer) == key.as.number) { key.kind = NativeValueKind::Int; key.as.integer = integer; }
    }
    if (key.kind == NativeValueKind::Bool) key.as.boolean = key.as.boolean != 0;
    return key;
}

bool IsManaged(const NativeValue& value) {
    return value.kind >= NativeValueKind::String && value.kind <= NativeValueKind::List;
}

void* Payload(Allocation* allocation) { return allocation + 1; }
Allocation* Header(void* payload) { return static_cast<Allocation*>(payload) - 1; }

std::string_view View(const void* string) {
    if (!string) return {};
    const auto* value = static_cast<const RuntimeString*>(string);
    return {value->data, static_cast<size_t>(value->length)};
}

// Lua 5.4 tostring() spelling of a float.
std::string FormatFloat(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.14g", value);
    std::string result = buffer;
    if (result.find_first_not_of("-0123456789") == std::string::npos) result += ".0";
    return result;
}

// Lua tonumber() on a string: optional surrounding space, decimal or 0x hex integer, or decimal float.
NativeValue ParseNumber(std::string_view text) {
    constexpr std::string_view space = " \f\n\r\t\v";
    const auto first = text.find_first_not_of(space);
    if (first == std::string_view::npos) return {};
    text = text.substr(first, text.find_last_not_of(space) - first + 1);
    const bool negative = text.front() == '-';
    auto magnitude = text.front() == '-' || text.front() == '+' ? text.substr(1) : text;
    if (magnitude.empty()) return {};
    NativeValue result;
    if (magnitude.size() > 2 && magnitude[0] == '0' && (magnitude[1] == 'x' || magnitude[1] == 'X')) {
        std::uint64_t bits{};
        const auto digits = magnitude.substr(2);
        const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), bits, 16);
        if (error != std::errc{} || end != digits.data() + digits.size()) return {};
        result.kind = NativeValueKind::Int; result.as.integer = static_cast<std::int64_t>(negative ? 0 - bits : bits);
        return result;
    }
    if (magnitude.find_first_of("iInN") != std::string_view::npos) return {}; // no inf/nan spellings
    std::int64_t integer{};
    if (const auto [end, error] = std::from_chars(text.data() + (text.front() == '+'), text.data() + text.size(), integer); error == std::errc{} && end == text.data() + text.size()) {
        result.kind = NativeValueKind::Int; result.as.integer = integer;
        return result;
    }
    double number{};
    if (const auto [end, error] = std::from_chars(magnitude.data(), magnitude.data() + magnitude.size(), number); error == std::errc{} && end == magnitude.data() + magnitude.size()) {
        result.kind = NativeValueKind::Float; result.as.number = negative ? -number : number;
        return result;
    }
    return {};
}

} // namespace

struct NativeRuntime::Impl {
    mutable std::recursive_mutex mutex;
    Allocation* allocations{};
    std::unordered_set<const Allocation*> live; // validates pointers passed to Mark
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
void AbiTableGet(void* context, void* object, const NativeValue* key, NativeValue* out) {
    assert(key && out);
    *out = {};
    if (!object) { static_cast<NativeRuntime*>(context)->ReportError("attempt to index a null table"); return; }
    const auto& table = *static_cast<RuntimeTable*>(object);
    if (const auto found = table.slots.find(NormalizeKey(*key)); found != table.slots.end()) *out = table.entries[found->second].second;
}
void AbiTableSet(void* context, void* object, const NativeValue* key, const NativeValue* value) {
    assert(key && value);
    auto& runtime = *static_cast<NativeRuntime*>(context);
    if (!object) { runtime.ReportError("attempt to index a null table"); return; }
    if (key->kind == NativeValueKind::Null) { runtime.ReportError("table index is nil"); return; }
    if (key->kind == NativeValueKind::Float && key->as.number != key->as.number) { runtime.ReportError("table index is NaN"); return; }
    auto& table = *static_cast<RuntimeTable*>(object);
    const auto normalized = NormalizeKey(*key);
    if (const auto found = table.slots.find(normalized); found != table.slots.end()) { table.entries[found->second].second = *value; return; }
    if (value->kind == NativeValueKind::Null) return;
    table.slots.emplace(normalized, table.entries.size());
    table.entries.emplace_back(normalized, *value);
}
void* AbiTableCreate(void* context, unsigned long long count, const NativeValue* pairs) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    auto* table = static_cast<RuntimeTable*>(runtime.Allocate(sizeof(RuntimeTable), TraceTable, FinalizeTable));
    if (!table) return nullptr;
    new (table) RuntimeTable();
    for (unsigned long long i{}; i < count; ++i) AbiTableSet(context, table, &pairs[i * 2], &pairs[i * 2 + 1]);
    return table;
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

// Yields the next non-null entry after *cursor in insertion order.
int AbiTableNext(void* context, void* object, unsigned long long* cursor, NativeValue* key, NativeValue* value) {
    assert(cursor && key && value);
    if (!object) { static_cast<NativeRuntime*>(context)->ReportError("attempt to iterate a null table"); return 0; }
    const auto& entries = static_cast<RuntimeTable*>(object)->entries;
    while (*cursor < entries.size()) {
        const auto& entry = entries[(*cursor)++];
        if (entry.second.kind == NativeValueKind::Null) continue;
        *key = entry.first; *value = entry.second;
        return 1;
    }
    return 0;
}

void TraceList(void* context, void* object) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    const auto& list = static_cast<RuntimeList*>(object)->header;
    const bool tagged = list.elementKind == NativeElementKind::Value;
    if (!tagged && list.elementKind < NativeElementKind::String) return;
    for (std::int64_t i{}; i < list.count; ++i) {
        const std::byte* slot = list.data + i * list.elementSize;
        if (tagged) { NativeValue item; std::memcpy(&item, slot, sizeof(item)); if (IsManaged(item)) runtime.Mark(item.as.pointer); }
        else { void* pointer; std::memcpy(&pointer, slot, sizeof(pointer)); runtime.Mark(pointer); }
    }
}
void FinalizeList(void* object) { static_cast<RuntimeList*>(object)->~RuntimeList(); }
void ReserveList(RuntimeList& list, std::int64_t capacity) {
    if (capacity <= list.header.capacity) return;
    list.storage.resize(static_cast<size_t>(capacity) * list.header.elementSize);
    list.header.data = list.storage.data();
    list.header.capacity = capacity;
}
void* AbiListCreate(void* context, unsigned long long elementSize, unsigned long long elementKind, unsigned long long capacity) {
    assert(elementSize > 0 && elementSize <= sizeof(NativeValue) && elementKind <= static_cast<unsigned long long>(NativeElementKind::List));
    auto& runtime = *static_cast<NativeRuntime*>(context);
    auto* list = static_cast<RuntimeList*>(runtime.Allocate(sizeof(RuntimeList), TraceList, FinalizeList));
    if (!list) return nullptr;
    new (list) RuntimeList();
    list->header.elementSize = elementSize;
    list->header.elementKind = static_cast<NativeElementKind>(elementKind);
    ReserveList(*list, static_cast<std::int64_t>(capacity));
    return list;
}
void AbiListReserve(void*, void* object, unsigned long long capacity) {
    assert(object);
    ReserveList(*static_cast<RuntimeList*>(object), static_cast<std::int64_t>(capacity));
}
// Elements [start, end); negative bounds count from the end, and out-of-range bounds clamp.
void* AbiListSlice(void* context, void* object, std::int64_t start, std::int64_t end) {
    if (!object) { static_cast<NativeRuntime*>(context)->ReportError("attempt to slice a null list"); return nullptr; }
    const auto& source = static_cast<RuntimeList*>(object)->header;
    if (start < 0) start += source.count;
    if (end < 0) end += source.count;
    start = (std::max)(start, std::int64_t{}); end = (std::min)(end, source.count);
    const auto count = (std::max)(end - start, std::int64_t{});
    auto* result = static_cast<RuntimeList*>(AbiListCreate(context, source.elementSize, static_cast<unsigned long long>(source.elementKind), static_cast<unsigned long long>(count)));
    if (!result) return nullptr;
    if (count) std::memcpy(result->header.data, source.data + start * source.elementSize, static_cast<size_t>(count) * source.elementSize);
    result->header.count = count;
    return result;
}

void* AbiStringConcat(void* context, void* left, void* right) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    if (!left || !right) runtime.ReportError("attempt to concatenate a null string");
    std::string joined;
    joined.reserve(View(left).size() + View(right).size());
    joined.append(View(left)).append(View(right));
    return runtime.Intern(joined);
}
// Lua string.sub(value, start + 1, hasCount ? start + count : -1).
void* AbiStringSubstring(void* context, void* value, std::int64_t start, std::int64_t count, int hasCount) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    if (!value) { runtime.ReportError("attempt to take a substring of a null string"); return runtime.Intern({}); }
    const auto text = View(value);
    const auto length = static_cast<std::int64_t>(text.size());
    std::int64_t first = start + 1, last = hasCount ? start + count : -1;
    if (first == 0 || first < -length) first = 1;
    else if (first < 0) first += length + 1;
    if (last > length) last = length;
    else if (last < -length) last = 0;
    else if (last < 0) last += length + 1;
    if (first > last) return runtime.Intern({});
    return runtime.Intern(text.substr(static_cast<size_t>(first - 1), static_cast<size_t>(last - first + 1)));
}
void* AbiStringCase(void* context, void* value, int upper) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    if (!value) { runtime.ReportError("attempt to change the case of a null string"); return runtime.Intern({}); }
    std::string text(View(value));
    for (char& character : text) {
        if (upper && character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
        else if (!upper && character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
    }
    return runtime.Intern(text);
}
void* AbiToString(void* context, const NativeValue* value) {
    assert(value);
    auto& runtime = *static_cast<NativeRuntime*>(context);
    const auto reference = [&](const char* prefix) { char buffer[64]; std::snprintf(buffer, sizeof(buffer), "%s: %p", prefix, value->as.pointer); return runtime.Intern(buffer); };
    switch (value->kind) {
    case NativeValueKind::Null: return runtime.Intern("nil");
    case NativeValueKind::Int: return runtime.Intern(std::to_string(value->as.integer));
    case NativeValueKind::Float: return runtime.Intern(FormatFloat(value->as.number));
    case NativeValueKind::Bool: return runtime.Intern(value->as.boolean ? "true" : "false");
    case NativeValueKind::String: return value->as.pointer;
    case NativeValueKind::Closure: return reference("function");
    default: return reference("table");
    }
}
void AbiToNumber(void* context, void* value, NativeValue* out) {
    assert(out);
    if (!value) { *out = {}; static_cast<NativeRuntime*>(context)->ReportError("attempt to convert a null string to a number"); return; }
    *out = ParseNumber(View(value));
}

} // namespace

NativeRuntime::NativeRuntime() : impl_(std::make_unique<Impl>()) {
    impl_->abi = {AbiVersion, this, AbiAllocate, AbiMark, AbiIntern, AbiTableCreate, AbiTableGet, AbiTableSet, AbiPrint, AbiReportError,
        AbiTableNext, AbiListCreate, AbiListReserve, AbiListSlice, AbiStringConcat, AbiStringSubstring, AbiStringCase, AbiToString, AbiToNumber};
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
    impl_->allocations = allocation; impl_->live.insert(allocation); ++impl_->allocationCount; impl_->allocatedBytes += actual;
    return Payload(allocation);
}

void NativeRuntime::Mark(void* object) {
    if (!object) return;
    std::scoped_lock lock(impl_->mutex);
    auto* allocation = Header(object);
    if (!impl_->live.contains(allocation) || allocation->marked) return;
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
        impl_->live.erase(current);
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
