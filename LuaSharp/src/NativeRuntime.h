#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace luasharp {

// Must equal LS_ABI_VERSION in runtime/luasharp/runtime.h.
constexpr unsigned long long NativeAbiVersion = 11;

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
    // The script implementation of an extern virtual method; null when the class does not override it.
    static void* HostOverride(const void* object, std::string_view method);
    static size_t HostOverrideCount(const void* object);
    // Links a new object to its host object, before its script constructor runs.
    static void SetNative(void* object, void* native);

    void* Intern(std::string_view utf8);
    [[nodiscard]] size_t AllocationCount() const;
    [[nodiscard]] size_t AllocatedBytes() const;

    void ReportError(std::string_view message);
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
