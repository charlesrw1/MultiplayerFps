#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "Framework/ClassBase.h"
#include "Framework/ClassTypeInfo.h"
#include "Framework/Util.h"
#include <stdexcept>
#include <unordered_map>

LuaSharpEngine* LuaSharpEngine::inst = nullptr;

namespace {
std::vector<const lsbind::Bindings*>& bindings_registry() {
	static std::vector<const lsbind::Bindings*> bindings;
	return bindings;
}

// LS_String in runtime/luasharp/runtime.h.
struct LsString
{
	int64_t length;
	char data[1];
};
} // namespace

namespace lsbind {
Registrar::Registrar(const char* pass, std::span<const char* const> declarations, std::span<const Symbol> symbols,
					 std::span<const Class> classes, std::span<const Scriptable> scriptables)
	: bindings{pass, declarations, symbols, classes, scriptables} {
	bindings_registry().push_back(&bindings);
}
std::span<const Bindings* const> all_bindings() { return bindings_registry(); }

std::string_view view(const void* string) {
	if (!string)
		return {};
	const auto* s = static_cast<const LsString*>(string);
	ASSERT(s->length >= 0);
	return {s->data, static_cast<size_t>(s->length)};
}
StringName name(const void* string) { return StringName(std::string(view(string)).c_str()); }
void* string(std::string_view text) {
	ASSERT(LuaSharpEngine::inst);
	return LuaSharpEngine::inst->runtime().Intern(text);
}
void* proxy_of(const ClassBase* object) {
	ASSERT(LuaSharpEngine::inst);
	return LuaSharpEngine::inst->proxy_of(object);
}
void* native_of(void* proxy) {
	// A destroyed proxy has a null host object, so it reads as null.
	return luasharp::NativeRuntime::NativeOf(proxy);
}
uint16_t field_offset(const PropertyInfoList* props, const char* name) {
	ASSERT(props);
	for (int i = 0; i < props->count; i++) {
		const PropertyInfo& p = props->list[i];
		if (std::strcmp(p.name, name) == 0) {
			ASSERT(!(p.flags & PROP_LUA_BACKED));
			return p.offset;
		}
	}
	ASSERT(!"bound field has no PropertyInfo");
	return 0;
}
int64_t enum_value(const int64_t* values, size_t count, int64_t ordinal) {
	if (ordinal >= 0 && static_cast<size_t>(ordinal) < count)
		return values[ordinal];
	report_error("enum value out of range");
	return values[0];
}
int64_t enum_ordinal(const int64_t* values, size_t count, int64_t value) {
	for (size_t i = 0; i < count; i++)
		if (values[i] == value)
			return static_cast<int64_t>(i);
	report_error("C++ enum value has no LuaSharp member");
	return 0;
}
void report_error(const char* message) {
	ASSERT(LuaSharpEngine::inst);
	LuaSharpEngine::inst->runtime().ReportError(message);
}
} // namespace lsbind

LuaSharpEngine::LuaSharpEngine() : runtime_(luasharp::NativeRuntime::Create()) {
	ASSERT(!inst);
	std::unordered_map<std::string_view, std::string_view> bases;
	for (const lsbind::Bindings* b : lsbind::all_bindings()) {
		for (const lsbind::Class& c : b->classes) {
			[[maybe_unused]] const bool added = bases.emplace(c.name, c.base).second;
			ASSERT(added); // one codegen pass owns each class
		}
		for (const char* chunk : b->declarations)
			declarations_.append(chunk).append("\n");
		for (const lsbind::Symbol& s : b->symbols)
			symbols_.push_back({s.name, s.address});
		for (const lsbind::Scriptable& s : b->scriptables) {
			[[maybe_unused]] const bool added = scriptables_.emplace(s.name, &s).second;
			ASSERT(added);
		}
	}
	// NativeClass needs the base first.
	auto register_class = [&](auto& self, std::string_view name) -> void {
		if (runtime_->FindNativeClass(name))
			return;
		const std::string_view base = bases.at(name);
		if (!base.empty())
			self(self, base);
		runtime_->NativeClass(name, base);
	};
	for (const auto& [name, base] : bases)
		register_class(register_class, name);
	types_ = luasharp::DeclarationTypes({declarations_});
	inst = this;
}

LuaSharpEngine::~LuaSharpEngine() {
	ASSERT(inst == this);
	while (!proxied_.empty())
		release(*proxied_.begin());
	inst = nullptr;
}

luasharp::NativeModule LuaSharpEngine::compile(std::string_view source) {
	luasharp::NativeCompileOptions options;
	options.externalTypes = types_;
	options.symbols = symbols_;
	options.runtime = runtime_;
	return luasharp::NativeModule::Compile(source, options);
}

void* LuaSharpEngine::proxy_of(const ClassBase* object) {
	if (!object)
		return nullptr;
	auto* self = const_cast<ClassBase*>(object);
	if (self->script_proxy)
		return self->script_proxy;
	const void* type = nullptr;
	for (const ClassTypeInfo* ti = &object->get_type(); ti && !type; ti = ti->super_typeinfo)
		type = runtime_->FindNativeClass(ti->classname);
	ASSERT(type); // ClassBase is always bound
	self->script_proxy = runtime_->NewNativeProxy(type, self);
	runtime_->AddRootSlot(&self->script_proxy);
	proxied_.insert(self);
	return self->script_proxy;
}

std::unique_ptr<ClassBase> LuaSharpEngine::create(const luasharp::NativeModule& module, std::string_view script_class) {
	using luasharp::NativeRuntime;
	ASSERT(module.Runtime() == runtime_);
	const std::string prefix = "ls_" + std::string(script_class);
	const auto alloc = reinterpret_cast<void* (*)()>(module.FindSymbol(prefix + "__host_alloc"));
	const auto init = reinterpret_cast<void (*)(void*)>(module.FindSymbol(prefix + "__init"));
	if (!alloc || !init)
		throw std::runtime_error("'" + std::string(script_class) +
								 "' is not a script class that derives from an engine class, or it is abstract");
	void* script = alloc();
	if (!script)
		throw std::runtime_error("LuaSharp could not allocate '" + std::string(script_class) + "': " + runtime_->LastError());

	const std::string base(NativeRuntime::NativeBaseName(script));
	const ClassTypeInfo* info = ClassBase::find_class(base.c_str());
	ASSERT(info); // every bound class is reflected
	const auto scriptable = scriptables_.find(info->classname);
	std::unique_ptr<ClassBase> object;
	if (info->scriptable_allocate && scriptable != scriptables_.end()) {
		void* const* slots = slots_of(script, *scriptable->second);
		object.reset(info->scriptable_allocate(info));
		scriptable->second->attach(object.get(), slots);
	} else {
		if (NativeRuntime::HostOverrideCount(script) != 0)
			throw std::runtime_error("'" + std::string(script_class) + "' overrides methods of '" + base +
									 "', which is not scriptable (CLASS_BODY(" + base + ", scriptable))");
		object.reset(info->allocate_this_type());
		if (!object)
			throw std::runtime_error("engine class '" + base + "' is abstract and not scriptable");
	}
	ASSERT(!object->script_proxy);
	NativeRuntime::SetNative(script, object.get());
	object->script_proxy = script;
	runtime_->AddRootSlot(&object->script_proxy);
	proxied_.insert(object.get());
	init(script); // the script constructor can use the engine object
	return object;
}

void* const* LuaSharpEngine::slots_of(const void* script, const lsbind::Scriptable& scriptable) {
	using luasharp::NativeRuntime;
	std::vector<void*> slots;
	size_t used = 0;
	for (const char* name : scriptable.slot_names) {
		slots.push_back(NativeRuntime::HostOverride(script, name));
		used += slots.back() != nullptr;
	}
	if (used != NativeRuntime::HostOverrideCount(script))
		throw std::runtime_error(std::string("a script class overrides a method of '") + scriptable.name +
								 "' that ScriptImpl cannot call (it returns a reference)");
	slots.push_back(nullptr); // so that data() is never null: a null ls_slots means a Lua object
	// Tables are shared by content, so a table never names code of an unloaded module that reused an address.
	return slot_tables_.insert(std::move(slots)).first->data();
}

size_t LuaSharpEngine::collect() { return runtime_->Collect(); }

void LuaSharpEngine::on_object_destroyed(ClassBase* object) {
	ASSERT(inst); // ~LuaSharpEngine releases every proxy
	inst->release(object);
}

void LuaSharpEngine::release(ClassBase* object) {
	ASSERT(object->script_proxy && proxied_.contains(object));
	luasharp::NativeRuntime::DestroyNative(object->script_proxy);
	runtime_->RemoveRootSlot(&object->script_proxy);
	object->script_proxy = nullptr;
	proxied_.erase(object);
}
