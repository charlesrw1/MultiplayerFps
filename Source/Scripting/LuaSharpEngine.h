#pragma once
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
struct ExternalType;
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
	// Loads each .ls file in the game folder `dir` and its subfolders, except the declarations for tools
	// (.luasharp/) and files in a subfolder named in skip_dirs. The module name is the game path. Errors are logged.
	// Returns the number of modules loaded.
	int load_script_files(const std::string& dir, std::span<const std::string_view> skip_dirs = {});
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

	// From ~ClassBase, when the object has a proxy.
	static void on_object_destroyed(ClassBase* object);

private:
	struct LoadedModule
	{
		std::unique_ptr<luasharp::NativeModule> module;
		std::vector<std::unique_ptr<LuaSharpClassTypeInfo>> classes; // base classes first
	};
	friend class LuaSharpClassTypeInfo;

	std::unique_ptr<ClassBase> make(const luasharp::NativeModule& module, std::string_view script_class,
									const LuaSharpClassTypeInfo* type);
	void register_classes(LoadedModule& loaded);
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
	std::unordered_map<std::string_view, const lsbind::Scriptable*> scriptables_;
	std::unordered_map<std::string_view, const lsbind::StructLayout*> struct_layouts_;
	std::set<std::vector<void*>> slot_tables_; // ScriptImpl_X::ls_slots; elements do not move
	std::vector<luasharp::ExternalType> types_;
	std::vector<luasharp::NativeSymbol> symbols_;
	std::string declarations_;
	std::unordered_set<ClassBase*> proxied_;
	std::map<std::string, LoadedModule> modules_;
	size_t collect_threshold_ = 0;
};
