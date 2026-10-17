#include "NativeRuntime.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <new>
#include <span>
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

constexpr unsigned long long AbiVersion = NativeAbiVersion;

// Field order must match LS_RuntimeApi in the generated C preamble.
struct RuntimeAbi {
    unsigned long long abiVersion{AbiVersion};
    void* context{};
    void* (*allocate)(void*, unsigned long long, NativeTraceFunction){};
    void (*mark)(void*, void**){};
    void* (*intern)(void*, const char*){};
    void* (*tableCreate)(void*, unsigned long long, const NativeValue*){};
    void (*tableGet)(void*, void*, const NativeValue*, NativeValue*){};
    void (*tableSet)(void*, void*, const NativeValue*, const NativeValue*){};
    void (*print)(void*, void*, unsigned long long, const NativeValue*){};
    void (*reportError)(void*, const char*){};
    int (*tableNext)(void*, void*, unsigned long long*, NativeValue*, NativeValue*){};
    void* (*listCreate)(void*, unsigned long long, unsigned long long, const void*, unsigned long long){};
    void (*listReserve)(void*, void*, unsigned long long){};
    void* (*listSlice)(void*, void*, std::int64_t, std::int64_t){};
    void* (*stringConcat)(void*, void*, void*){};
    void* (*stringSubstring)(void*, void*, std::int64_t, std::int64_t, int){};
    void* (*stringCase)(void*, void*, int){};
    void* (*toString)(void*, const NativeValue*){};
    void (*toNumber)(void*, void*, NativeValue*){};
};

// Mirrors LS_TypeInfo / LS_FieldInfo in runtime.h.
struct NativeTypeInfo;
struct NativeFieldInfo { const char* name; unsigned long long offset, kind, elementKind; const NativeTypeInfo* type; };
struct NativeTypeInfo {
    const char* name; unsigned long long fieldCount; const NativeFieldInfo* fields; unsigned long long methodCount; const void* methods; unsigned long long size, kind;
    const NativeTypeInfo* base; void* const* vtable; unsigned long long flags;
};
constexpr unsigned long long TypeNative = 1; // LS_TYPE_NATIVE
// Mirrors LS_NativeObject.
struct NativeProxy { const NativeTypeInfo* type; void* native; };
// An extern class type info and the name it points to.
struct NativeClassEntry { std::string name; NativeTypeInfo info{}; };
constexpr unsigned long long SlotValue = 0, SlotFloat32 = 16, SlotInt32 = 17, StructPayload = 16;
constexpr unsigned long long KindOf(NativeValueKind kind) { return static_cast<unsigned long long>(kind); }

std::span<const NativeFieldInfo> Fields(const NativeTypeInfo& type) { return {type.fields, static_cast<size_t>(type.fieldCount)}; }
const NativeTypeInfo& BoxType(const void* box) { return **static_cast<const NativeTypeInfo* const*>(box); }
const std::byte* BoxValue(const void* box) { return static_cast<const std::byte*>(box) + StructPayload; }
// Field offsets include the box header; value points at the struct itself.
const std::byte* FieldSlot(const std::byte* value, const NativeFieldInfo& field) { return value + field.offset - StructPayload; }
template <typename T> T Load(const std::byte* slot) { T value; std::memcpy(&value, slot, sizeof(value)); return value; }
bool IsReferenceSlot(unsigned long long kind) { return kind >= KindOf(NativeValueKind::String) && kind <= KindOf(NativeValueKind::List); }

// Same rule as ls_type_same: identity, or structs and list types that match field by field.
bool TypeSame(const NativeTypeInfo* left, const NativeTypeInfo* right, int depth = 0) {
    if (left == right) return true;
    if (!left || !right || left->kind != right->kind || left->kind == KindOf(NativeValueKind::Object) || depth > 8) return false;
    if (left->size != right->size || left->fieldCount != right->fieldCount || std::strcmp(left->name, right->name) != 0) return false;
    for (unsigned long long i{}; i < left->fieldCount; ++i) {
        const auto& a = left->fields[i]; const auto& b = right->fields[i];
        if (a.offset != b.offset || a.kind != b.kind || a.elementKind != b.elementKind || std::strcmp(a.name, b.name) != 0 || !TypeSame(a.type, b.type, depth + 1)) return false;
    }
    return true;
}

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

bool ValueEqual(const NativeValue& left, const NativeValue& right);
size_t ValueHash(const NativeValue& value);

// Struct keys compare and hash by field values, like ls_struct_equal; references by identity.
bool StructEqual(const NativeTypeInfo& type, const std::byte* left, const std::byte* right) {
    for (const auto& field : Fields(type)) {
        const auto* a = FieldSlot(left, field); const auto* b = FieldSlot(right, field);
        bool same{};
        switch (field.kind) {
        case SlotValue: same = ValueEqual(NormalizeKey(Load<NativeValue>(a)), NormalizeKey(Load<NativeValue>(b))); break;
        case KindOf(NativeValueKind::Int): same = Load<std::int64_t>(a) == Load<std::int64_t>(b); break;
        case KindOf(NativeValueKind::Float): same = Load<double>(a) == Load<double>(b); break;
        case KindOf(NativeValueKind::Bool): same = (Load<unsigned char>(a) != 0) == (Load<unsigned char>(b) != 0); break;
        case SlotFloat32: same = Load<float>(a) == Load<float>(b); break;
        case SlotInt32: same = Load<int>(a) == Load<int>(b); break;
        case KindOf(NativeValueKind::Struct): same = StructEqual(*field.type, a, b); break;
        default: same = Load<void*>(a) == Load<void*>(b); break;
        }
        if (!same) return false;
    }
    return true;
}
size_t HashCombine(size_t seed, size_t value) { return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2)); }
size_t FloatHash(double value) { return std::hash<double>{}(value == 0.0 ? 0.0 : value); } // -0.0 equals 0.0
size_t StructHash(const NativeTypeInfo& type, const std::byte* value) {
    size_t hash = std::hash<std::string_view>{}(type.name);
    for (const auto& field : Fields(type)) {
        const auto* slot = FieldSlot(value, field);
        switch (field.kind) {
        case SlotValue: hash = HashCombine(hash, ValueHash(NormalizeKey(Load<NativeValue>(slot)))); break;
        case KindOf(NativeValueKind::Int): hash = HashCombine(hash, std::hash<std::int64_t>{}(Load<std::int64_t>(slot))); break;
        case KindOf(NativeValueKind::Float): hash = HashCombine(hash, FloatHash(Load<double>(slot))); break;
        case KindOf(NativeValueKind::Bool): hash = HashCombine(hash, Load<unsigned char>(slot) != 0); break;
        case SlotFloat32: hash = HashCombine(hash, FloatHash(Load<float>(slot))); break;
        case SlotInt32: hash = HashCombine(hash, std::hash<int>{}(Load<int>(slot))); break;
        case KindOf(NativeValueKind::Struct): hash = HashCombine(hash, StructHash(*field.type, slot)); break;
        default: hash = HashCombine(hash, std::hash<void*>{}(Load<void*>(slot))); break;
        }
    }
    return hash;
}
// A NaN field makes a struct unequal to itself, so it cannot be a table key.
bool StructHasNaN(const NativeTypeInfo& type, const std::byte* value) {
    for (const auto& field : Fields(type)) {
        const auto* slot = FieldSlot(value, field);
        if (field.kind == KindOf(NativeValueKind::Float) && std::isnan(Load<double>(slot))) return true;
        if (field.kind == SlotFloat32 && std::isnan(Load<float>(slot))) return true;
        if (field.kind == KindOf(NativeValueKind::Struct) && StructHasNaN(*field.type, slot)) return true;
        if (field.kind == SlotValue) { const auto item = Load<NativeValue>(slot); if (item.kind == NativeValueKind::Float && std::isnan(item.as.number)) return true; }
    }
    return false;
}

bool ValueEqual(const NativeValue& left, const NativeValue& right) {
    if (left.kind != right.kind) return false;
    switch (left.kind) {
    case NativeValueKind::Null: return true;
    case NativeValueKind::Int: return left.as.integer == right.as.integer;
    case NativeValueKind::Float: return left.as.number == right.as.number;
    case NativeValueKind::Bool: return left.as.boolean == right.as.boolean;
    case NativeValueKind::Struct:
        return left.as.pointer == right.as.pointer || (TypeSame(&BoxType(left.as.pointer), &BoxType(right.as.pointer)) && StructEqual(BoxType(left.as.pointer), BoxValue(left.as.pointer), BoxValue(right.as.pointer)));
    default: return left.as.pointer == right.as.pointer;
    }
}
size_t ValueHash(const NativeValue& value) {
    if (value.kind == NativeValueKind::Struct) return StructHash(BoxType(value.as.pointer), BoxValue(value.as.pointer));
    if (value.kind == NativeValueKind::Float) return FloatHash(value.as.number);
    std::uint64_t bits; std::memcpy(&bits, &value.as, sizeof(bits));
    return std::hash<std::uint64_t>{}(bits ^ (static_cast<std::uint64_t>(value.kind) << 59));
}

struct KeyHash { size_t operator()(const NativeValue& key) const { return ValueHash(key); } };
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

bool IsManaged(const NativeValue& value) {
    return value.kind >= NativeValueKind::String && value.kind <= NativeValueKind::Struct;
}

void* Payload(Allocation* allocation) { return allocation + 1; }
Allocation* Header(void* payload) { return static_cast<Allocation*>(payload) - 1; }

std::string_view View(const void* string) {
    if (!string) return {};
    const auto* value = static_cast<const RuntimeString*>(string);
    return {value->data, static_cast<size_t>(value->length)};
}

// Lua 5.4 tostring() spelling of a float.
std::string FormatFloat(double value, int digits = 14) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*g", digits, value);
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
    std::unordered_set<void**> rootSlots;
    std::unordered_map<std::string, std::unique_ptr<NativeClassEntry>> nativeClasses;
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
void AbiMark(void* context, void** slot) { assert(slot); static_cast<NativeRuntime*>(context)->Mark(*slot); }
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
    if (key->kind == NativeValueKind::Struct && StructHasNaN(BoxType(key->as.pointer), BoxValue(key->as.pointer))) { runtime.ReportError("table index is a struct with a NaN field"); return; }
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

// Same walk as ls_trace_struct_value.
void TraceStructValue(NativeRuntime& runtime, const NativeTypeInfo& type, const std::byte* value) {
    for (const auto& field : Fields(type)) {
        const auto* slot = FieldSlot(value, field);
        if (field.kind == SlotValue) { const auto item = Load<NativeValue>(slot); if (IsManaged(item)) runtime.Mark(item.as.pointer); }
        else if (field.kind == KindOf(NativeValueKind::Struct)) TraceStructValue(runtime, *field.type, slot);
        else if (IsReferenceSlot(field.kind)) runtime.Mark(Load<void*>(slot));
    }
}
void TraceList(void* context, void* object) {
    auto& runtime = *static_cast<NativeRuntime*>(context);
    const auto& list = static_cast<RuntimeList*>(object)->header;
    const auto kind = static_cast<unsigned long long>(list.elementKind);
    if (kind != SlotValue && kind != KindOf(NativeValueKind::Struct) && !IsReferenceSlot(kind)) return;
    const auto* structType = kind == KindOf(NativeValueKind::Struct) ? static_cast<const NativeTypeInfo*>(list.elementType) : nullptr;
    assert(kind != KindOf(NativeValueKind::Struct) || structType);
    for (std::int64_t i{}; i < list.count; ++i) {
        const std::byte* slot = list.data + i * list.elementSize;
        if (kind == SlotValue) { const auto item = Load<NativeValue>(slot); if (IsManaged(item)) runtime.Mark(item.as.pointer); }
        else if (structType) TraceStructValue(runtime, *structType, slot);
        else runtime.Mark(Load<void*>(slot));
    }
}
void FinalizeList(void* object) { static_cast<RuntimeList*>(object)->~RuntimeList(); }
void ReserveList(RuntimeList& list, std::int64_t capacity) {
    if (capacity <= list.header.capacity) return;
    list.storage.resize(static_cast<size_t>(capacity) * list.header.elementSize);
    list.header.data = list.storage.data();
    list.header.capacity = capacity;
}
void* AbiListCreate(void* context, unsigned long long elementSize, unsigned long long elementKind, const void* elementType, unsigned long long capacity) {
    assert(elementSize > 0 && elementKind <= static_cast<unsigned long long>(NativeElementKind::Struct));
    assert(elementKind != KindOf(NativeValueKind::Struct) || elementType);
    auto& runtime = *static_cast<NativeRuntime*>(context);
    auto* list = static_cast<RuntimeList*>(runtime.Allocate(sizeof(RuntimeList), TraceList, FinalizeList));
    if (!list) return nullptr;
    new (list) RuntimeList();
    list->header.elementSize = elementSize;
    list->header.elementKind = static_cast<NativeElementKind>(elementKind);
    list->header.elementType = elementType;
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
    auto* result = static_cast<RuntimeList*>(AbiListCreate(context, source.elementSize, static_cast<unsigned long long>(source.elementKind), source.elementType, static_cast<unsigned long long>(count)));
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
std::string FormatStruct(const NativeTypeInfo& type, const std::byte* value);

// Lua 5.4 tostring(); a struct prints as "Vec3(1.0, 2.0, 3.0)".
std::string FormatValue(const NativeValue& value) {
    const auto reference = [&](const char* prefix) { char buffer[64]; std::snprintf(buffer, sizeof(buffer), "%s: %p", prefix, value.as.pointer); return std::string(buffer); };
    switch (value.kind) {
    case NativeValueKind::Null: return "nil";
    case NativeValueKind::Int: return std::to_string(value.as.integer);
    case NativeValueKind::Float: return FormatFloat(value.as.number);
    case NativeValueKind::Bool: return value.as.boolean ? "true" : "false";
    case NativeValueKind::String: return std::string(View(value.as.pointer));
    case NativeValueKind::Closure: return reference("function");
    case NativeValueKind::Struct: return FormatStruct(BoxType(value.as.pointer), BoxValue(value.as.pointer));
    case NativeValueKind::Object: {
        const auto& proxy = *static_cast<const NativeProxy*>(value.as.pointer);
        if ((proxy.type->flags & TypeNative) && !proxy.native) return "nil"; // destroyed
        return reference("table");
    }
    default: return reference("table");
    }
}

// Fields in declaration order, nested structs recursively.
std::string FormatStruct(const NativeTypeInfo& type, const std::byte* value) {
    std::string text = type.name; text += '(';
    for (unsigned long long i{}; i < type.fieldCount; ++i) {
        const auto& field = type.fields[i];
        const auto* slot = FieldSlot(value, field);
        if (i) text += ", ";
        switch (field.kind) {
        case SlotValue: text += FormatValue(Load<NativeValue>(slot)); break;
        case KindOf(NativeValueKind::Int): text += std::to_string(Load<std::int64_t>(slot)); break;
        case KindOf(NativeValueKind::Float): text += FormatFloat(Load<double>(slot)); break;
        case KindOf(NativeValueKind::Bool): text += Load<unsigned char>(slot) ? "true" : "false"; break;
        case SlotFloat32: text += FormatFloat(Load<float>(slot), 7); break;
        case SlotInt32: text += std::to_string(Load<int>(slot)); break;
        case KindOf(NativeValueKind::Struct): text += FormatStruct(*field.type, slot); break;
        default: {
            NativeValue reference; reference.kind = Load<void*>(slot) ? static_cast<NativeValueKind>(field.kind) : NativeValueKind::Null; reference.as.pointer = Load<void*>(slot);
            text += FormatValue(reference); break;
        }
        }
    }
    return text + ')';
}

void* AbiToString(void* context, const NativeValue* value) {
    assert(value);
    auto& runtime = *static_cast<NativeRuntime*>(context);
    if (value->kind == NativeValueKind::String) return value->as.pointer;
    return runtime.Intern(FormatValue(*value));
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
    for (auto* slot : impl_->rootSlots) Mark(*slot);
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

void NativeRuntime::AddRootSlot(void** slot) {
    assert(slot);
    std::scoped_lock lock(impl_->mutex);
    [[maybe_unused]] const bool added = impl_->rootSlots.insert(slot).second;
    assert(added && "root slot registered twice");
}
void NativeRuntime::RemoveRootSlot(void** slot) {
    std::scoped_lock lock(impl_->mutex);
    [[maybe_unused]] const auto removed = impl_->rootSlots.erase(slot);
    assert(removed == 1 && "root slot was not registered");
}

const void* NativeRuntime::NativeClass(std::string_view name, std::string_view baseName) {
    std::scoped_lock lock(impl_->mutex);
    const auto* base = static_cast<const NativeTypeInfo*>(baseName.empty() ? nullptr : FindNativeClass(baseName));
    if (!baseName.empty() && !base) throw std::runtime_error("Native class '" + std::string(name) + "' needs its base '" + std::string(baseName) + "' first");
    if (const auto found = impl_->nativeClasses.find(std::string(name)); found != impl_->nativeClasses.end()) {
        if (found->second->info.base != base) throw std::runtime_error("Native class '" + std::string(name) + "' was registered with a different base");
        return &found->second->info;
    }
    auto entry = std::make_unique<NativeClassEntry>();
    entry->name = name;
    entry->info.name = entry->name.c_str(); entry->info.kind = KindOf(NativeValueKind::Object); entry->info.base = base; entry->info.flags = TypeNative;
    const auto* info = &entry->info;
    impl_->nativeClasses.emplace(entry->name, std::move(entry));
    return info;
}
const void* NativeRuntime::FindNativeClass(std::string_view name) const {
    std::scoped_lock lock(impl_->mutex);
    const auto found = impl_->nativeClasses.find(std::string(name));
    return found == impl_->nativeClasses.end() ? nullptr : &found->second->info;
}
void* NativeRuntime::NewNativeProxy(const void* nativeClass, void* native, NativeFinalizeFunction finalize) {
    const auto* type = static_cast<const NativeTypeInfo*>(nativeClass);
    assert(type && (type->flags & TypeNative) && "NewNativeProxy needs a type from NativeClass");
    assert(native);
    auto* proxy = static_cast<NativeProxy*>(Allocate(sizeof(NativeProxy), nullptr, finalize));
    if (proxy) { proxy->type = type; proxy->native = native; }
    return proxy;
}
void* NativeRuntime::NativeOf(const void* proxy) { return proxy ? static_cast<const NativeProxy*>(proxy)->native : nullptr; }
void NativeRuntime::DestroyNative(void* proxy) { assert(proxy); static_cast<NativeProxy*>(proxy)->native = nullptr; }

NativeRef::NativeRef(std::shared_ptr<NativeRuntime> runtime, void* object) : runtime_(std::move(runtime)), object_(object) {
    assert(runtime_);
    runtime_->AddRootSlot(&object_);
}
NativeRef::~NativeRef() { Reset(); }
NativeRef::NativeRef(NativeRef&& other) noexcept { *this = std::move(other); }
NativeRef& NativeRef::operator=(NativeRef&& other) noexcept {
    if (this == &other) return *this;
    Reset();
    if (!other.runtime_) return *this;
    runtime_ = std::move(other.runtime_); object_ = other.object_;
    runtime_->RemoveRootSlot(&other.object_); other.object_ = nullptr;
    runtime_->AddRootSlot(&object_);
    return *this;
}
void NativeRef::Reset() {
    if (runtime_) runtime_->RemoveRootSlot(&object_);
    runtime_.reset(); object_ = nullptr;
}

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
