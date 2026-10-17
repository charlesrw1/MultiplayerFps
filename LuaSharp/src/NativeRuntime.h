#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace luasharp {

// Must equal LS_ABI_VERSION in runtime/luasharp/runtime.h.
constexpr unsigned long long NativeAbiVersion = 8;

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

} // namespace luasharp
