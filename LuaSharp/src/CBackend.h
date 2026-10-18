#pragma once

#include "Semantic.h"
#include "NativeAbi.h"
#include "NativeRuntime.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace luasharp {

struct NativeExport {
    std::string sourceName;
    std::string symbolName;
    std::string returnType;
    std::vector<std::string> parameterTypes;
};

struct NativeGlobal {
    std::string name;
    std::string symbolName;
    std::string type;
};

struct NativeFieldLayout {
    std::string name;
    std::string type;
    bool operator==(const NativeFieldLayout&) const = default;
};

struct NativeTypeLayout {
    std::string name;
    std::string baseName;
    std::vector<NativeFieldLayout> fields;
    bool operator==(const NativeTypeLayout&) const = default;
};

// An extern class the module uses. Load registers it with NativeRuntime::NativeClass(name, baseName),
// so all modules of a runtime agree on its base.
struct NativeClassBinding {
    std::string name;
    std::string baseName;
};

// One local of an instrumented function, at its offset in the function's locals struct.
// Visible before statements [visibleFrom, visibleTo). An indirect local holds a pointer to the value (cells, inout, struct `this`).
struct NativeDebugLocal {
    std::string name;
    std::string type;
    uint32_t visibleFrom{};
    uint32_t visibleTo{};
    bool indirect{};
};

struct NativeDebugFunction {
    std::string name;             // "Type.method", "Type.new", "<lambda>" or "<top-level>"
    std::vector<uint32_t> lines;  // 1-based source line of each statement id
    std::vector<NativeDebugLocal> locals;
};

// Debug tables of an instrumented module. Index i matches NativeDebugModule::functions[i].
struct NativeDebugInfo {
    std::string sourceName;
    std::vector<NativeDebugFunction> functions;
};

struct CCompilationResult {
    std::string c;
    std::vector<NativeExport> exports;
    std::vector<NativeGlobal> globals;
    std::vector<NativeTypeLayout> types;
    std::vector<NativeClassBinding> nativeClasses; // each after its base
    std::vector<std::string> hostClasses;          // script classes that derive from extern classes, each after its base
    std::vector<std::string> externalStructs;      // structs of the external types; each exports ls_<S>__type
    NativeDebugInfo debug;                         // empty unless CEmitOptions::debugHooks
};

// Mirrors of the runtime.h debug structs (LS_DEBUG_HOOKS).
using NativeDebugHook = void (*)(void* context, void* locals, void* frame, uint64_t function, uint64_t statement);
struct NativeDebugState {
    volatile uint64_t active;
    NativeDebugHook hook;
    void* context;
};
struct NativeDebugFunctionRange {
    const void* begin;
    const void* end;                // exclusive
    const abi::NativeFieldInfo* locals; // localCount entries; offsets in the locals struct, FieldIndirect for indirect locals
    uint64_t localCount;
};
struct NativeDebugProperty {
    abi::NativeFieldInfo value; // the result slot: offset 0 of the buffer that get fills
    void (*get)(void* proxy, void* out);
};
struct NativeDebugClass {
    const char* name;
    const NativeDebugProperty* properties;
    uint64_t propertyCount;
};
struct NativeDebugModule {
    NativeDebugState* state;
    const NativeDebugFunctionRange* functions;
    uint64_t functionCount;
    const void* textBegin;          // all generated code of the module lies in [textBegin, textEnd)
    const void* textEnd;
    const NativeDebugClass* classes; // the extern classes, each with its own getters
    uint64_t classCount;
    const void* staticsBase;             // the module state
    const abi::NativeFieldInfo* statics; // top-level variables and static fields ("Type.field"), at offsets from staticsBase
    uint64_t staticCount;
};

struct CEmitOptions {
    // Instrumented build: locals live in a struct, and a hook can run before each statement.
    bool debugHooks{};
    std::string_view source;  // debugHooks: the text that was analyzed, for line numbers
    std::string sourceName;   // debugHooks: the file name in debug info and #line
};

struct NativeSymbol {
    std::string name;
    const void* address{};
};

struct NativeCompileOptions {
    std::string libtccPath;
    std::vector<ExternalType> externalTypes;
    // Declaration-only sources, such as engine.ls: extern classes, field-only structs and enums. Each sees the ones before it.
    std::vector<std::string> declarations;
    std::vector<NativeSymbol> symbols;
    std::shared_ptr<NativeRuntime> runtime;
    bool initialize{true}; // false: caller runs NativeModule::Initialize() itself
    bool emitDebugHooks{};  // see CEmitOptions::debugHooks
    std::string sourceName;
};

// Emits C11 from the same checked bound tree used by the Lua backend.
// Throws when the source is invalid or uses a construct the native backend
// does not support yet; it never emits a knowingly incorrect approximation.
CCompilationResult CompileToC(std::string_view source);
CCompilationResult CompileToC(std::string_view source, const std::vector<ExternalType>& externalTypes);
// For an analysis the caller already has; externalTypes must match the ones it was analyzed with.
CCompilationResult CompileToC(const Analysis& analysis, const std::vector<ExternalType>& externalTypes = {}, const CEmitOptions& options = {});
// The types of declaration sources, after externalTypes. Throws on a diagnostic error, or when a source declares code.
std::vector<ExternalType> DeclarationTypes(const std::vector<std::string>& sources, std::vector<ExternalType> externalTypes = {});

class NativeModule {
public:
    NativeModule();
    ~NativeModule();
    NativeModule(NativeModule&&) noexcept;
    NativeModule& operator=(NativeModule&&) noexcept;
    NativeModule(const NativeModule&) = delete;
    NativeModule& operator=(const NativeModule&) = delete;

    // Compiles, relocates, and runs the module initializer entirely in memory.
    // libtccPath may name libtcc.dll or be empty to use normal discovery.
    static NativeModule Compile(std::string_view source, std::string_view libtccPath = {});
    static NativeModule Compile(std::string_view source, const NativeCompileOptions& options);
    // Same as Compile, for C already produced by CompileToC.
    static NativeModule Load(CCompilationResult generated, const NativeCompileOptions& options);
    static bool IsLibTccAvailable(std::string_view libtccPath = {});

    // Runs the module initializer (top-level code). Compile does this unless options.initialize is false.
    void Initialize();
    [[nodiscard]] void* FindSymbol(std::string_view symbol) const;
    [[nodiscard]] void* FindGlobal(std::string_view name) const;
    // The LS_TypeInfo of one of Generated().hostClasses; null for other names.
    [[nodiscard]] const void* HostClassType(std::string_view className) const;
    // The LS_TypeInfo of one of Generated().externalStructs; null for other names.
    [[nodiscard]] const void* ExternalStructType(std::string_view structName) const;
    // The LS_TypeInfo of a math struct (Vec2, Vec3, ...) or of an external struct; null for other names.
    [[nodiscard]] const void* StructType(std::string_view structName) const;
    [[nodiscard]] std::string LastError() const;
    void ClearError();
    [[nodiscard]] std::shared_ptr<NativeRuntime> Runtime() const;
    [[nodiscard]] const CCompilationResult& Generated() const;
    // The debug tables of an instrumented module; null for other modules.
    [[nodiscard]] const NativeDebugModule* Debug() const;

private:
    struct Impl;
    explicit NativeModule(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

class NativeFunctionSlot {
public:
    struct State;
    NativeFunctionSlot() = default;
    [[nodiscard]] explicit operator bool() const;
    [[nodiscard]] void* Load() const;
    [[nodiscard]] const std::string& Name() const;
    [[nodiscard]] const std::string& ReturnType() const;
    [[nodiscard]] const std::vector<std::string>& ParameterTypes() const;

private:
    explicit NativeFunctionSlot(std::shared_ptr<State> state);
    std::shared_ptr<State> state_;
    friend class ReloadableNativeProgram;
};

// Maintains stable function slots across validated module replacements. Old
// module generations remain alive until the program is destroyed, making an
// address loaded immediately before a reload safe to finish executing.
class ReloadableNativeProgram {
public:
    ReloadableNativeProgram();
    ~ReloadableNativeProgram();
    ReloadableNativeProgram(ReloadableNativeProgram&&) noexcept;
    ReloadableNativeProgram& operator=(ReloadableNativeProgram&&) noexcept;
    ReloadableNativeProgram(const ReloadableNativeProgram&) = delete;
    ReloadableNativeProgram& operator=(const ReloadableNativeProgram&) = delete;

    static ReloadableNativeProgram Compile(std::string_view source, const NativeCompileOptions& options = {});
    void Reload(std::string_view source, const NativeCompileOptions& options = {});
    [[nodiscard]] NativeFunctionSlot FindFunction(std::string_view sourceName) const;
    [[nodiscard]] void* FindGlobal(std::string_view name) const;
    [[nodiscard]] size_t Generation() const;

private:
    struct Impl;
    explicit ReloadableNativeProgram(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};

} // namespace luasharp
