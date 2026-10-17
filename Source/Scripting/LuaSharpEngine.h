#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

class ClassBase;
namespace luasharp {
class NativeRuntime;
class NativeModule;
struct ExternalType;
struct NativeSymbol;
} // namespace luasharp

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
	size_t collect();
	luasharp::NativeRuntime& runtime() { return *runtime_; }
	// All generated declarations as one source, for tools.
	const std::string& declarations() const { return declarations_; }

	// From ~ClassBase, when the object has a proxy.
	static void on_object_destroyed(ClassBase* object);

private:
	void release(ClassBase* object);

	std::shared_ptr<luasharp::NativeRuntime> runtime_;
	std::vector<luasharp::ExternalType> types_;
	std::vector<luasharp::NativeSymbol> symbols_;
	std::string declarations_;
	std::unordered_set<ClassBase*> proxied_;
};
