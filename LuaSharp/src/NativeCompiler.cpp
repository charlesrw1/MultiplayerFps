#include "CBackend.h"
#include "Builtins.h"
#include "NativeRuntime.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace luasharp {
namespace {

struct TCCState;
using TccNew = TCCState* (*)();
using TccDelete = void (*)(TCCState*);
using TccSetErrorFunc = void (*)(TCCState*, void*, void (*)(void*, const char*));
using TccSetOutputType = int (*)(TCCState*, int);
using TccCompileString = int (*)(TCCState*, const char*);
using TccAddSymbol = int (*)(TCCState*, const char*, const void*);
// TinyCC 0.9.27 takes a relocation pointer; current mob allocates internally
// and ignores the extra argument on the supported x64 ABI. Passing AUTO keeps
// the loader compatible with the stable Windows binary as well.
using TccRelocate = int (*)(TCCState*, void*);
using TccGetSymbol = void* (*)(TCCState*, const char*);

void CaptureError(void* opaque, const char* message) {
    auto& errors = *static_cast<std::string*>(opaque);
    if (!errors.empty()) errors += '\n';
    errors += message ? message : "unknown libtcc error";
}

std::filesystem::path ModulePath(HMODULE module) {
    std::wstring path(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
        if (length == 0) return {};
        if (length < path.size()) { path.resize(length); return path; }
        path.resize(path.size() * 2);
    }
}

HMODULE LoadLibTcc(std::string_view requested) {
    if (!requested.empty()) return LoadLibraryA(std::string(requested).c_str());
    char environment[32768]{};
    const auto length = GetEnvironmentVariableA("LUASHARP_LIBTCC", environment, static_cast<DWORD>(std::size(environment)));
    if (length > 0 && length < std::size(environment)) return LoadLibraryA(environment);
    const auto bundled = ModulePath(nullptr).parent_path() / "tcc" / "libtcc.dll";
    return LoadLibraryExW(bundled.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
}

// libtcc resolves lib/ and include/ relative to its own DLL.
void RequireTccRuntime(HMODULE library) {
    const auto runtime = ModulePath(library).parent_path() / "lib" / "libtcc1-64.a";
    if (!std::filesystem::exists(runtime))
        throw std::runtime_error("TinyCC runtime not found at '" + runtime.string() + "'. libtcc.dll must sit in a full TinyCC install (run LuaSharp/scripts/install-libtcc.ps1 and rebuild)");
    const auto header = ModulePath(library).parent_path() / "include" / "luasharp" / "runtime.h";
    if (!std::filesystem::exists(header))
        throw std::runtime_error("LuaSharp C runtime header not found at '" + header.string() + "'. The build copies LuaSharp/runtime/ into tcc/include/");
}

// The official libtcc.dll passes its stack pointer to msvcrt's _setjmp as jmp_buf.Frame, so tcc_error's
// longjmp SEH-unwinds libtcc frames that have no unwind data and the host crashes (STATUS_BAD_STACK).
// With Frame == 0, longjmp restores registers without unwinding; libtcc is plain C, so nothing is skipped.
void* NonUnwindingSetjmpThunk(void* realSetjmp) {
    static void* thunk = [&] {
        // xor edx, edx; mov rax, realSetjmp; jmp rax -- a tail jump, so _setjmp still sees libtcc's frame.
        unsigned char code[] = {0x31, 0xD2, 0x48, 0xB8, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xE0};
        std::memcpy(code + 4, &realSetjmp, sizeof(realSetjmp));
        void* memory = VirtualAlloc(nullptr, sizeof(code), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!memory) throw std::runtime_error("Could not allocate the libtcc setjmp thunk");
        std::memcpy(memory, code, sizeof(code));
        DWORD previous{};
        if (!VirtualProtect(memory, sizeof(code), PAGE_EXECUTE_READ, &previous)) throw std::runtime_error("Could not protect the libtcc setjmp thunk");
        FlushInstructionCache(GetCurrentProcess(), memory, sizeof(code));
        return memory;
    }();
    return thunk;
}

void DisableLongjmpUnwinding(HMODULE library) {
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    const auto msvcrt = GetModuleHandleW(L"msvcrt.dll");
    void* const realSetjmp = msvcrt ? reinterpret_cast<void*>(GetProcAddress(msvcrt, "_setjmp")) : nullptr;
    if (!realSetjmp) return;
    void* const thunk = NonUnwindingSetjmpThunk(realSetjmp);

    auto* const base = reinterpret_cast<unsigned char*>(library);
    const auto* const nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const auto& imports = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!imports.VirtualAddress) return;
    for (auto* descriptor = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + imports.VirtualAddress); descriptor->Name; ++descriptor) {
        for (auto* slot = reinterpret_cast<void**>(base + descriptor->FirstThunk); *slot; ++slot) {
            if (*slot != realSetjmp) continue;
            DWORD previous{};
            if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &previous)) throw std::runtime_error("Could not patch libtcc's _setjmp import");
            *slot = thunk;
            VirtualProtect(slot, sizeof(*slot), previous, &previous);
        }
    }
}

template <class T> T Import(HMODULE library, const char* name) {
    const auto address = GetProcAddress(library, name);
    if (!address) throw std::runtime_error(std::string("libtcc.dll does not export '") + name + "'");
    return reinterpret_cast<T>(address);
}

} // namespace

struct NativeModule::Impl {
    HMODULE library{};
    TCCState* state{};
    TccDelete destroy{};
    TccGetSymbol getSymbol{};
    CCompilationResult generated;
    std::shared_ptr<NativeRuntime> runtime;
    size_t rootRegistration{};
    bool initialized{};

    ~Impl() {
        if (runtime && rootRegistration) runtime->UnregisterRoots(rootRegistration);
        if (state && destroy) destroy(state);
        if (library) FreeLibrary(library);
    }
};

NativeModule::NativeModule() = default;
NativeModule::NativeModule(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
NativeModule::~NativeModule() = default;
NativeModule::NativeModule(NativeModule&&) noexcept = default;
NativeModule& NativeModule::operator=(NativeModule&&) noexcept = default;

bool NativeModule::IsLibTccAvailable(std::string_view libtccPath) {
    if (auto library = LoadLibTcc(libtccPath)) { FreeLibrary(library); return true; }
    return false;
}

NativeModule NativeModule::Compile(std::string_view source, std::string_view libtccPath) {
    NativeCompileOptions options; options.libtccPath = libtccPath;
    return Compile(source, options);
}

NativeModule NativeModule::Compile(std::string_view source, const NativeCompileOptions& options) {
    return Load(CompileToC(source, DeclarationTypes(options.declarations, options.externalTypes)), options);
}

NativeModule NativeModule::Load(CCompilationResult generated, const NativeCompileOptions& options) {
    auto impl = std::make_unique<Impl>();
    impl->generated = std::move(generated);
    impl->runtime = options.runtime ? options.runtime : NativeRuntime::Create();
    impl->library = LoadLibTcc(options.libtccPath);
    if (!impl->library) {
        throw std::runtime_error("Could not load libtcc.dll. Expected it in the 'tcc' folder beside the executable, or set LUASHARP_LIBTCC to its full path");
    }
    RequireTccRuntime(impl->library);
    DisableLongjmpUnwinding(impl->library);

    const auto create = Import<TccNew>(impl->library, "tcc_new");
    impl->destroy = Import<TccDelete>(impl->library, "tcc_delete");
    const auto setError = Import<TccSetErrorFunc>(impl->library, "tcc_set_error_func");
    const auto setOutput = Import<TccSetOutputType>(impl->library, "tcc_set_output_type");
    const auto compile = Import<TccCompileString>(impl->library, "tcc_compile_string");
    const auto addSymbol = Import<TccAddSymbol>(impl->library, "tcc_add_symbol");
    const auto relocate = Import<TccRelocate>(impl->library, "tcc_relocate");
    impl->getSymbol = Import<TccGetSymbol>(impl->library, "tcc_get_symbol");

    impl->state = create();
    if (!impl->state) throw std::runtime_error("tcc_new failed");
    std::string errors;
    setError(impl->state, &errors, CaptureError);
    if (setOutput(impl->state, 1) < 0) throw std::runtime_error("tcc_set_output_type failed" + (errors.empty() ? std::string{} : ": " + errors));
    // Before compiling: a base that differs from an earlier module's throws here.
    for (const auto& binding : impl->generated.nativeClasses) static_cast<void>(impl->runtime->NativeClass(binding.name, binding.baseName));
    std::vector<std::string> registeredSymbols;
    for (const auto& symbol : options.symbols) {
        if (symbol.name.empty() || !symbol.address) throw std::runtime_error("Native symbols require a non-empty name and non-null address");
        if (symbol.name == "ls_alloc" || symbol.name.starts_with("ls_module_") || symbol.name.starts_with("ls_global_"))
            throw std::runtime_error("Native symbol '" + symbol.name + "' uses a reserved LuaSharp runtime name");
        if (std::find(registeredSymbols.begin(), registeredSymbols.end(), symbol.name) != registeredSymbols.end())
            throw std::runtime_error("Native symbol '" + symbol.name + "' is registered more than once");
        registeredSymbols.push_back(symbol.name);
        if (addSymbol(impl->state, symbol.name.c_str(), symbol.address) < 0)
            throw std::runtime_error("tcc_add_symbol(" + symbol.name + ") failed" + (errors.empty() ? std::string{} : ": " + errors));
    }
    if (compile(impl->state, impl->generated.c.c_str()) < 0)
        throw std::runtime_error("TinyCC compilation failed" + (errors.empty() ? std::string{} : ":\n" + errors));
    if (relocate(impl->state, reinterpret_cast<void*>(1)) < 0)
        throw std::runtime_error("TinyCC relocation failed" + (errors.empty() ? std::string{} : ":\n" + errors));

    const auto abiVersion = reinterpret_cast<long long (*)()>(impl->getSymbol(impl->state, "ls_module_abi_version"));
    if (!abiVersion || abiVersion() != static_cast<long long>(NativeAbiVersion)) throw std::runtime_error("LuaSharp native module ABI version mismatch");
    const auto bind = reinterpret_cast<int (*)(void*)>(impl->getSymbol(impl->state, "ls_module_bind"));
    if (!bind || bind(impl->runtime->Abi()) != 0) throw std::runtime_error("LuaSharp native module rejected the runtime ABI");
    const auto traceRoots = reinterpret_cast<NativeRootTraceFunction>(impl->getSymbol(impl->state, "ls_module_trace_roots"));
    if (!traceRoots) throw std::runtime_error("TinyCC output did not contain ls_module_trace_roots");
    impl->rootRegistration = impl->runtime->RegisterRoots(traceRoots);

    NativeModule module(std::move(impl));
    if (options.initialize) module.Initialize();
    return module;
}

void NativeModule::Initialize() {
    if (!impl_) throw std::runtime_error("Cannot initialize an empty native module");
    if (impl_->initialized) throw std::runtime_error("Native module is already initialized");
    const auto initializer = reinterpret_cast<int (*)()>(impl_->getSymbol(impl_->state, "ls_module_init"));
    if (!initializer) throw std::runtime_error("TinyCC output did not contain ls_module_init");
    impl_->initialized = true;
    const int result = initializer();
    if (result != 0) throw std::runtime_error("LuaSharp native module initializer returned " + std::to_string(result));
    if (const auto error = impl_->runtime->LastError(); !error.empty()) throw std::runtime_error("LuaSharp native runtime error: " + error);
}

void* NativeModule::FindSymbol(std::string_view symbol) const {
    if (!impl_) return nullptr;
    return impl_->getSymbol(impl_->state, std::string(symbol).c_str());
}

void* NativeModule::FindGlobal(std::string_view name) const {
    if (!impl_) return nullptr;
    const auto found = std::find_if(impl_->generated.globals.begin(), impl_->generated.globals.end(), [&](const auto& global) { return global.name == name; });
    if (found == impl_->generated.globals.end()) return nullptr;
    const auto getter = reinterpret_cast<void* (*)()>(impl_->getSymbol(impl_->state, found->symbolName.c_str()));
    return getter ? getter() : nullptr;
}

namespace {
// ls_<name>__type, which the C backend emits for each name in `listed`.
const void* ExportedType(const NativeModule& module, const std::vector<std::string>& listed, std::string_view name) {
    if (std::find(listed.begin(), listed.end(), name) == listed.end()) return nullptr;
    const auto type = reinterpret_cast<const void* (*)()>(module.FindSymbol("ls_" + std::string(name) + "__type"));
    assert(type && "the C backend emits ls_<T>__type for each listed type");
    return type ? type() : nullptr;
}
} // namespace

const void* NativeModule::HostClassType(std::string_view className) const {
    return impl_ ? ExportedType(*this, impl_->generated.hostClasses, className) : nullptr;
}
const void* NativeModule::ExternalStructType(std::string_view structName) const {
    return impl_ ? ExportedType(*this, impl_->generated.externalStructs, structName) : nullptr;
}
const void* NativeModule::StructType(std::string_view structName) const {
    if (!impl_) return nullptr;
    return IsNativeStruct(structName) ? ExportedType(*this, NativeStructTypes(), structName) : ExternalStructType(structName);
}

std::string NativeModule::LastError() const {
    return impl_ ? impl_->runtime->LastError() : std::string{};
}

void NativeModule::ClearError() {
    if (impl_) impl_->runtime->ClearError();
}

std::shared_ptr<NativeRuntime> NativeModule::Runtime() const { return impl_ ? impl_->runtime : nullptr; }

const CCompilationResult& NativeModule::Generated() const {
    if (!impl_) throw std::runtime_error("Native module is empty");
    return impl_->generated;
}

struct NativeFunctionSlot::State {
    std::atomic<void*> address{};
    std::string name;
    std::string returnType;
    std::vector<std::string> parameterTypes;
    std::shared_ptr<NativeModule> owner;
};

NativeFunctionSlot::NativeFunctionSlot(std::shared_ptr<State> state) : state_(std::move(state)) {}
NativeFunctionSlot::operator bool() const { return state_ && state_->address.load(std::memory_order_acquire); }
void* NativeFunctionSlot::Load() const { return state_ ? state_->address.load(std::memory_order_acquire) : nullptr; }
const std::string& NativeFunctionSlot::Name() const { static const std::string empty; return state_ ? state_->name : empty; }
const std::string& NativeFunctionSlot::ReturnType() const { static const std::string empty; return state_ ? state_->returnType : empty; }
const std::vector<std::string>& NativeFunctionSlot::ParameterTypes() const { static const std::vector<std::string> empty; return state_ ? state_->parameterTypes : empty; }

struct ReloadableNativeProgram::Impl {
    mutable std::mutex mutex;
    std::shared_ptr<NativeModule> current;
    std::vector<std::shared_ptr<NativeModule>> retired;
    std::unordered_map<std::string, std::shared_ptr<NativeFunctionSlot::State>> slots;
    size_t generation{1};
    std::shared_ptr<NativeRuntime> runtime;
};

namespace {
bool IsUserExport(const NativeExport& value) { return value.sourceName.empty() || value.sourceName.front() != '$'; }
}

ReloadableNativeProgram::ReloadableNativeProgram() = default;
ReloadableNativeProgram::ReloadableNativeProgram(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
ReloadableNativeProgram::~ReloadableNativeProgram() = default;
ReloadableNativeProgram::ReloadableNativeProgram(ReloadableNativeProgram&&) noexcept = default;
ReloadableNativeProgram& ReloadableNativeProgram::operator=(ReloadableNativeProgram&&) noexcept = default;

ReloadableNativeProgram ReloadableNativeProgram::Compile(std::string_view source, const NativeCompileOptions& options) {
    auto impl = std::make_unique<Impl>();
    impl->runtime = options.runtime ? options.runtime : NativeRuntime::Create();
    auto boundOptions = options; boundOptions.runtime = impl->runtime;
    impl->current = std::make_shared<NativeModule>(NativeModule::Compile(source, boundOptions));
    for (const auto& value : impl->current->Generated().exports) {
        if (!IsUserExport(value)) continue;
        auto state = std::make_shared<NativeFunctionSlot::State>();
        state->name = value.sourceName; state->returnType = value.returnType; state->parameterTypes = value.parameterTypes; state->owner = impl->current;
        state->address.store(impl->current->FindSymbol(value.symbolName), std::memory_order_release);
        if (!state->address.load(std::memory_order_acquire)) throw std::runtime_error("Native export '" + value.sourceName + "' has no relocated symbol");
        impl->slots.emplace(value.sourceName, std::move(state));
    }
    return ReloadableNativeProgram(std::move(impl));
}

void ReloadableNativeProgram::Reload(std::string_view source, const NativeCompileOptions& options) {
    if (!impl_) throw std::runtime_error("Reloadable native program is empty");
    if (options.runtime && options.runtime != impl_->runtime) throw std::runtime_error("Reload cannot replace the program's NativeRuntime");
    auto boundOptions = options; boundOptions.runtime = impl_->runtime;
    auto replacement = std::make_shared<NativeModule>(NativeModule::Compile(source, boundOptions));
    std::unordered_map<std::string, void*> replacementAddresses;
    for (const auto& value : replacement->Generated().exports) if (IsUserExport(value)) {
        const auto address = replacement->FindSymbol(value.symbolName);
        if (!address) throw std::runtime_error("Native export '" + value.sourceName + "' has no relocated symbol");
        replacementAddresses.emplace(value.sourceName, address);
    }
    std::scoped_lock lock(impl_->mutex);
    for (const auto& layout : impl_->current->Generated().types) {
        const auto found = std::find_if(replacement->Generated().types.begin(), replacement->Generated().types.end(), [&](const auto& value) { return value.name == layout.name; });
        if (found == replacement->Generated().types.end()) throw std::runtime_error("Reload removed native type '" + layout.name + "'");
        if (*found != layout) throw std::runtime_error("Reload changed native layout of type '" + layout.name + "'");
    }
    for (const auto& global : impl_->current->Generated().globals) {
        const auto found = std::find_if(replacement->Generated().globals.begin(), replacement->Generated().globals.end(), [&](const auto& value) { return value.name == global.name; });
        if (found == replacement->Generated().globals.end()) throw std::runtime_error("Reload removed native global '" + global.name + "'");
        if (found->type != global.type) throw std::runtime_error("Reload changed native type of global '" + global.name + "'");
    }
    for (const auto& [name, slot] : impl_->slots) {
        const auto found = std::find_if(replacement->Generated().exports.begin(), replacement->Generated().exports.end(), [&](const auto& value) { return value.sourceName == name; });
        if (found == replacement->Generated().exports.end()) throw std::runtime_error("Reload removed native export '" + name + "'");
        if (found->returnType != slot->returnType || found->parameterTypes != slot->parameterTypes) throw std::runtime_error("Reload changed native signature of '" + name + "'");
    }
    impl_->retired.push_back(impl_->current);
    for (const auto& value : replacement->Generated().exports) {
        if (!IsUserExport(value)) continue;
        const auto address = replacementAddresses.at(value.sourceName);
        const auto found = impl_->slots.find(value.sourceName);
        if (found == impl_->slots.end()) {
            auto state = std::make_shared<NativeFunctionSlot::State>();
            state->name = value.sourceName; state->returnType = value.returnType; state->parameterTypes = value.parameterTypes; state->owner = replacement;
            state->address.store(address, std::memory_order_release); impl_->slots.emplace(value.sourceName, std::move(state));
        } else {
            found->second->owner = replacement;
            found->second->address.store(address, std::memory_order_release);
        }
    }
    impl_->current = std::move(replacement); ++impl_->generation;
}

NativeFunctionSlot ReloadableNativeProgram::FindFunction(std::string_view sourceName) const {
    if (!impl_) return {};
    std::scoped_lock lock(impl_->mutex);
    const auto found = impl_->slots.find(std::string(sourceName));
    return found == impl_->slots.end() ? NativeFunctionSlot{} : NativeFunctionSlot(found->second);
}

void* ReloadableNativeProgram::FindGlobal(std::string_view name) const {
    if (!impl_) return nullptr;
    std::scoped_lock lock(impl_->mutex);
    return impl_->current->FindGlobal(name);
}

size_t ReloadableNativeProgram::Generation() const {
    if (!impl_) return 0;
    std::scoped_lock lock(impl_->mutex);
    return impl_->generation;
}

} // namespace luasharp
