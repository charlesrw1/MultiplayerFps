#pragma once
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class ClassBase;
namespace luasharp {
class NativeRuntime;
class NativeModule;
struct ExternalType;
struct NativeSymbol;
} // namespace luasharp
namespace lsbind {
struct Scriptable;
}

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
	void* proxy_of(const ClassBase* object);
	// An object of a script class that derives from an engine class X: a ScriptImpl_X whose virtual functions
	// call the script overrides. The script object is its proxy. The module must stay loaded while the object lives.
	// Throws std::runtime_error when the class cannot be created.
	std::unique_ptr<ClassBase> create(const luasharp::NativeModule& module, std::string_view script_class);
	size_t collect();
	luasharp::NativeRuntime& runtime() { return *runtime_; }
	// All generated declarations as one source, for tools.
	const std::string& declarations() const { return declarations_; }

	// From ~ClassBase, when the object has a proxy.
	static void on_object_destroyed(ClassBase* object);

private:
	void release(ClassBase* object);
	void* const* slots_of(const void* script_object, const lsbind::Scriptable& scriptable);

	std::shared_ptr<luasharp::NativeRuntime> runtime_;
	std::unordered_map<std::string_view, const lsbind::Scriptable*> scriptables_;
	std::set<std::vector<void*>> slot_tables_; // ScriptImpl_X::ls_slots; elements do not move
	std::vector<luasharp::ExternalType> types_;
	std::vector<luasharp::NativeSymbol> symbols_;
	std::string declarations_;
	std::unordered_set<ClassBase*> proxied_;
};
