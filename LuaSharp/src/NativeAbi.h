#pragma once

// C++ mirrors of the runtime.h structs, for the runtime and the debugger. Internal to LuaSharpLib.
#include "NativeRuntime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace luasharp::abi {

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
    void* (*stringCreate)(void*, const char*, std::int64_t){};
};

// Mirrors LS_TypeInfo / LS_FieldInfo in runtime.h.
struct NativeTypeInfo;
struct NativeHostOverride { const char* name; void* function; }; // LS_HostOverride
struct NativeEnumInfo { const char* name; unsigned long long count; const char* const* names; const long long* values; }; // LS_EnumInfo
struct NativeFieldInfo { const char* name; unsigned long long offset, kind, elementKind; const NativeTypeInfo* type; unsigned long long flags; const char* editor; const char* former; const NativeEnumInfo* enumInfo; };
constexpr unsigned long long FieldEditable = 1; // LS_FIELD_EDITABLE
constexpr unsigned long long FieldIndirect = 2; // LS_FIELD_INDIRECT
struct NativeTypeInfo {
    const char* name; unsigned long long fieldCount; const NativeFieldInfo* fields; unsigned long long methodCount; const void* methods; unsigned long long size, kind;
    const NativeTypeInfo* base; void* const* vtable; unsigned long long flags;
    unsigned long long hostOverrideCount; const NativeHostOverride* hostOverrides;
    NativeTraceFunction trace;
};
constexpr unsigned long long TypeNative = 1;     // LS_TYPE_NATIVE
constexpr unsigned long long TypeNativeBase = 2; // LS_TYPE_NATIVE_BASE
// Mirrors LS_NativeObject.
struct NativeProxy { const NativeTypeInfo* type; void* native; };
constexpr unsigned long long SlotValue = 0, SlotFloat32 = 16, SlotInt32 = 17, SlotUnknown = 18, StructPayload = 16;
constexpr unsigned long long KindOf(NativeValueKind kind) { return static_cast<unsigned long long>(kind); }

inline std::span<const NativeFieldInfo> Fields(const NativeTypeInfo& type) { return {type.fields, static_cast<size_t>(type.fieldCount)}; }
inline const NativeTypeInfo& BoxType(const void* box) { return **static_cast<const NativeTypeInfo* const*>(box); }
inline const std::byte* BoxValue(const void* box) { return static_cast<const std::byte*>(box) + StructPayload; }
// Field offsets include the box header; value points at the struct itself.
inline const std::byte* FieldSlot(const std::byte* value, const NativeFieldInfo& field) { return value + field.offset - StructPayload; }
template <typename T> T Load(const std::byte* slot) { T value; std::memcpy(&value, slot, sizeof(value)); return value; }
inline bool IsReferenceSlot(unsigned long long kind) { return kind >= KindOf(NativeValueKind::String) && kind <= KindOf(NativeValueKind::List); }

} // namespace luasharp::abi
