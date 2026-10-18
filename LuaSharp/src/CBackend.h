#pragma once

#include "Semantic.h"
#include "NativeRuntime.h"

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

struct CCompilationResult {
    std::string c;
    std::vector<NativeExport> exports;
    std::vector<NativeGlobal> globals;
    std::vector<NativeTypeLayout> types;
    std::vector<NativeClassBinding> nativeClasses; // each after its base
    std::vector<std::string> hostClasses;          // script classes that derive from extern classes, each after its base
    std::vector<std::string> externalStructs;      // structs of the external types; each exports ls_<S>__type
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
};

// Emits C11 from the same checked bound tree used by the Lua backend.
// Throws when the source is invalid or uses a construct the native backend
// does not support yet; it never emits a knowingly incorrect approximation.
CCompilationResult CompileToC(std::string_view source);
CCompilationResult CompileToC(std::string_view source, const std::vector<ExternalType>& externalTypes);
// For an analysis the caller already has; externalTypes must match the ones it was analyzed with.
CCompilationResult CompileToC(const Analysis& analysis, const std::vector<ExternalType>& externalTypes = {});
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
