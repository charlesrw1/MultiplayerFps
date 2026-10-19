#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace luasharp {

// Must equal LS_ABI_VERSION in runtime/luasharp/runtime.h.
constexpr unsigned long long NativeAbiVersion = 15;

using NativeTraceFunction =void (*)(void* runtimeContext, void* object);
using NativeFinalizeFunction = void (*)(void* object);
using NativeRootTraceFunction = void (*)(void* runtimeContext);

enum class NativeValueKind : std::uint64_t { Null, Int, Float, Bool, String, Object, Table, Closure, List, Struct };
struct NativeValue {
    NativeValueKind kind{NativeValueKind::Null};
    union { std::int64_t integer; double number; std::uint64_t boolean; void* pointer; } as{};
};

// The NativeValueKind of a list's elements; Value means the elements are tagged NativeValues.
// Struct elements are stored inline; their references are traced through elementType.
enum class NativeElementKind : std::uint64_t { Value, Int, Float, Bool, String, Object, Table, Closure, List, Struct };
// GC-managed dynamic array. Generated code reads data directly; only the runtime resizes it.
struct NativeList {
    std::int64_t count{};
    std::int64_t capacity{};
    std::uint64_t elementSize{};
    NativeElementKind elementKind{};
    const void* elementType{}; // LS_TypeInfo of object, struct and list elements
    std::byte* data{};
};

// An [Editable] field (LS_FIELD_EDITABLE) at offset in the objects of its class.
// A struct value as LS_TypeInfo describes it; offsets are into the value, without the box header.
struct NativeStructLayout {
    struct Field { std::string_view name; size_t offset{}; };
    size_t size{};
    std::vector<Field> fields;
};
struct NativeEnumMember { std::string_view name; std::int64_t value{}; };
struct NativeEditableField {
    std::string_view name;
    size_t offset{};
    NativeValueKind kind{};
    std::string_view typeName; // Struct fields: Vec2, Vec3 or Quat; Object fields: the extern class; enum fields: the enum
    std::string_view editor;   // [Editor("name")], or empty
    std::string_view former;   // [Former("old")], or empty
    std::vector<NativeEnumMember> enumMembers; // enum fields (kind Int); empty otherwise
};

// Shared, non-moving native heap. Collection is stop-the-world: callers must
// not execute generated code concurrently with Collect().
class NativeRuntime : public std::enable_shared_from_this<NativeRuntime> {
public:
    NativeRuntime();
    ~NativeRuntime();
    NativeRuntime(const NativeRuntime&) = delete;
    NativeRuntime& operator=(const NativeRuntime&) = delete;

    static std::shared_ptr<NativeRuntime> Create();

    void* Allocate(size_t size, NativeTraceFunction trace = nullptr, NativeFinalizeFunction finalize = nullptr);
    void Mark(void* object);
    size_t Collect();

    void Pin(void* object);
    void Unpin(void* object);
    size_t RegisterRoots(NativeRootTraceFunction trace);
    void UnregisterRoots(size_t registration);
    // A host variable that holds a GC object. Collect marks *slot. The slot must stay registered at most once.
    void AddRootSlot(void** slot);
    void RemoveRootSlot(void** slot);

    // The LS_TypeInfo of an extern class, owned by the runtime and stable for its lifetime. The first call
    // creates it; later calls must give the same base. The base must already exist.
    const void* NativeClass(std::string_view name, std::string_view baseName = {});
    [[nodiscard]] const void* FindNativeClass(std::string_view name) const;
    // A proxy (LS_NativeObject) for a host object. finalize, when given, runs when the GC frees the proxy:
    // script-owned host objects delete themselves there.
    void* NewNativeProxy(const void* nativeClass, void* native, NativeFinalizeFunction finalize = nullptr);
    // The host object behind a proxy; null once destroyed.
    static void* NativeOf(const void* proxy);
    // Script then sees the proxy as null.
    static void DestroyNative(void* proxy);
    // Objects of a script class that derives from an extern class (LS_TYPE_NATIVE_BASE). They are proxies too.
    // The nearest extern class of the object's class.
    static std::string_view NativeBaseName(const void* object);
    // The same for the classes, by LS_TypeInfo (NativeModule::HostClassType).
    static std::string_view TypeName(const void* type);
    static std::string_view NativeBaseTypeName(const void* type);
    // The base class when it is a script class too; null when the base is the extern class.
    static const void* ScriptBaseType(const void* type);
    // The [Editable] fields the class declares, without those of its base classes.
    static std::vector<NativeEditableField> EditableFields(const void* type);
    // A list of count zeroed elements. elementType is the LS_TypeInfo of object and struct elements: FindNativeClass
    // or FindStructType, so that the list does not depend on a module.
    void* NewList(NativeElementKind kind, size_t elementSize, const void* elementType, size_t count);
    // A runtime-owned copy of a struct type (NativeModule::StructType), found by name later. The first type of a name
    // wins; later calls return it. Object fields must have extern classes.
    const void* AdoptStructType(const void* type);
    [[nodiscard]] const void* FindStructType(std::string_view name) const;
    // The layout of a struct type (NativeModule::ExternalStructType).
    static NativeStructLayout StructLayout(const void* type);
    static void* TypeHostOverride(const void* type, std::string_view method);
    static size_t TypeHostOverrideCount(const void* type);
    // The script implementation of an extern virtual method; null when the class does not override it.
    static void* HostOverride(const void* object, std::string_view method);
    static size_t HostOverrideCount(const void* object);
    // Links a new object to its host object, before its script constructor runs.
    static void SetNative(void* object, void* native);

    // Hot reload. The live objects of a script class (by LS_TypeInfo), in no fixed order.
    [[nodiscard]] std::vector<void*> ObjectsOf(const void* type) const;
    // Gives the objects of each old class its new type info and trace function, and the lists of old elements the
    // new element type. The layouts must be the same. Returns the number of objects retyped.
    size_t Retype(const std::unordered_map<const void*, const void*>& types);

    void* Intern(std::string_view utf8);
    [[nodiscard]] size_t AllocationCount() const;
    [[nodiscard]] size_t AllocatedBytes() const;

    void ReportError(std::string_view message);
    // Runs on the reporting thread after each ReportError, outside the runtime lock. Empty removes it.
    void SetErrorHandler(std::function<void(std::string_view)> handler);
    // Script output (printf). It goes to the print handler, or to stdout when there is none.
    void Print(std::string_view text);
    void SetPrintHandler(std::function<void(std::string_view)> handler);
    [[nodiscard]] std::string LastError() const;
    void ClearError();

    // Internal ABI table passed to generated modules.
    [[nodiscard]] void* Abi();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// A move-only root slot: keeps one GC object alive while the NativeRef holds it.
// Host code keeps GC objects only through these, so a moving collector can update them.
class NativeRef {
public:
    NativeRef() = default;
    NativeRef(std::shared_ptr<NativeRuntime> runtime, void* object);
    ~NativeRef();
    NativeRef(NativeRef&& other) noexcept;
    NativeRef& operator=(NativeRef&& other) noexcept;
    NativeRef(const NativeRef&) = delete;
    NativeRef& operator=(const NativeRef&) = delete;

    [[nodiscard]] void* Get() const { return object_; }
    void Reset();

private:
    std::shared_ptr<NativeRuntime> runtime_;
    void* object_{};
};

} // namespace luasharp
