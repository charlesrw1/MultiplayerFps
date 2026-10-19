#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ClassBase;
class ClassTypeInfo;
class LuaSharpClassTypeInfo;
namespace luasharp {
class NativeRuntime;
class NativeModule;
class NativeDebugger;
class NativeDapServer;
struct ExternalType;
struct SourceFile;
struct NativeSymbol;
} // namespace luasharp
namespace lsbind {
struct Scriptable;
struct StructLayout;
} // namespace lsbind

// Compiles LuaSharp modules against the reflected engine classes (the generated lsbind::Bindings).
// Each engine object has at most one proxy. ClassBase::script_proxy holds it as a GC root slot, and
// ~ClassBase makes it null for script.
class LuaSharpEngine
{
public:
	static LuaSharpEngine* inst;
	LuaSharpEngine();
	~LuaSharpEngine();
	LuaSharpEngine(const LuaSharpEngine&) = delete;
	LuaSharpEngine& operator=(const LuaSharpEngine&) = delete;

	// Throws std::runtime_error with the diagnostics when the source does not compile.
	luasharp::NativeModule compile(std::string_view source);
	// Compiles a module that the engine keeps, and registers a ClassTypeInfo for each script class that derives
	// from a scriptable engine class. ClassBase::create_class, the editor and serialization then find the class
	// by name, and its [Editable] fields are its PropertyInfos (PROP_SCRIPT_FIELD).
	// Throws std::runtime_error when the source does not compile or a class cannot be registered.
	const luasharp::NativeModule& load(const std::string& name, std::string_view source);
	// One module from several files: each file sees the types of the others. With the debugger on, give full paths
	// as file names, so that breakpoints match.
	const luasharp::NativeModule& load(const std::string& name, std::span<const luasharp::SourceFile> files);
	// Loads the .ls files in the game folder `dir` and its subfolders as one module named `dir`, except the declarations
	// for tools (.luasharp/) and files in a subfolder named in skip_dirs. Errors are logged.
	// Returns the number of files loaded: 0 when the module does not compile.
	int load_script_files(const std::string& dir, std::span<const std::string_view> skip_dirs = {});
	// Hot reload: compiles the folder of load_script_files again and moves the running module to the new code
	// (LuaSharp/src/NativeReload.h). Loads the folder when it did not compile before. On errors it logs them, keeps
	// the running module and returns false. Call it when no script function is on the stack.
	bool reload_script_files(const std::string& dir);
	// The same for a module that load() loaded, with the new files.
	bool reload(const std::string& name, std::span<const luasharp::SourceFile> files);
	// From the file watcher. reload_if_requested then reloads each folder of load_script_files.
	void request_reload() { reload_requested_ = true; }
	// Call when no script function is on the stack (once per frame).
	void reload_if_requested();
	// The module of `name`; it changes on each reload_script_files. Older modules stay loaded.
	const luasharp::NativeModule* find_module(const std::string& name) const;
	void* proxy_of(const ClassBase* object);
	// An object of a script class that derives from an engine class X: a ScriptImpl_X whose virtual functions
	// call the script overrides. The script object is its proxy. The module must stay loaded while the object lives.
	// Throws std::runtime_error when the class cannot be created.
	std::unique_ptr<ClassBase> create(const luasharp::NativeModule& module, std::string_view script_class);
	size_t collect();
	// Call when no script function is on the stack (once per frame). Collects when the heap has doubled.
	void collect_if_needed();
	luasharp::NativeRuntime& runtime() { return *runtime_; }
	// All generated declarations as one source, for tools.
	const std::string& declarations() const { return declarations_; }

	// Serves the script debugger (DAP) on 127.0.0.1:port; 0 selects a free port. Modules that load() after it are
	// instrumented. Throws std::runtime_error when it cannot listen.
	void start_debugger(uint16_t port);
	// Blocks until a client has set its breakpoints (DAP configurationDone). Needs start_debugger.
	void wait_for_debugger();
	// 0 when the debugger is off.
	uint16_t debugger_port() const;

	// From ~ClassBase, when the object has a proxy.
	static void on_object_destroyed(ClassBase* object);

private:
	struct LoadedModule
	{
		std::unique_ptr<luasharp::NativeModule> module;
		std::vector<std::unique_ptr<luasharp::NativeModule>> retired; // replaced by reloads; objects and closures use their code
		std::vector<std::unique_ptr<LuaSharpClassTypeInfo>> classes; // base classes first
	};
	struct ScriptSources
	{
		std::vector<std::string> texts;
		std::vector<luasharp::SourceFile> files; // views texts
	};
	friend class LuaSharpClassTypeInfo;

	// Compiles without Initialize(). The debugger needs the module before its top-level code runs.
	luasharp::NativeModule compile_module(std::span<const luasharp::SourceFile> files, bool debug);
	ScriptSources read_script_files(const std::string& dir, std::span<const std::string> skip_dirs) const;
	static LuaSharpClassTypeInfo* class_named(const LoadedModule& loaded, std::string_view name);
	// Errors that stop bind_classes, for each class of `next` that derives from an engine class.
	std::vector<std::string> check_classes(const LoadedModule& loaded, const luasharp::NativeModule& next);
	// Gives the classes of `next` their ClassTypeInfos: a class that loaded has keeps its type, and the types of
	// removed classes are unregistered. Needs check_classes without errors.
	void bind_classes(LoadedModule& loaded, const luasharp::NativeModule& next);
	// Each object of a script class calls the overrides of its current type.
	void attach_slots();

	std::unique_ptr<ClassBase> make(const luasharp::NativeModule& module, std::string_view script_class,
									const LuaSharpClassTypeInfo* type);
	void unregister_classes(LoadedModule& loaded);
	const LuaSharpClassTypeInfo* find_type(const luasharp::NativeModule& module, std::string_view script_class) const;
	const lsbind::Scriptable* scriptable_of(const ClassTypeInfo& engine_class) const;
	void release(ClassBase* object);
	void* const* slots_of(const void* script_type, const lsbind::Scriptable& scriptable);
	// Throws std::runtime_error when a declared struct in the module differs from its C++ mirror.
	void check_struct_layouts(const luasharp::NativeModule& module) const;
	// Copies the math and external struct types into the runtime, for the lists that thunks make (lsbind::new_list).
	void adopt_struct_types(const luasharp::NativeModule& module);

	std::shared_ptr<luasharp::NativeRuntime> runtime_;
	std::unique_ptr<luasharp::NativeDebugger> debugger_;	// null unless start_debugger
	std::unique_ptr<luasharp::NativeDapServer> dap_server_;
	std::unordered_map<std::string_view, const lsbind::Scriptable*> scriptables_;
	std::unordered_map<std::string_view, const lsbind::StructLayout*> struct_layouts_;
	std::set<std::vector<void*>> slot_tables_; // ScriptImpl_X::ls_slots; elements do not move
	std::vector<luasharp::ExternalType> types_;
	std::vector<luasharp::NativeSymbol> symbols_;
	std::string declarations_;
	std::unordered_set<ClassBase*> proxied_;
	std::map<std::string, LoadedModule> modules_;
	std::map<std::string, std::vector<std::string>> script_folders_; // load_script_files: dir -> skip_dirs
	bool reload_requested_ = false;
	size_t collect_threshold_ = 0;
};
