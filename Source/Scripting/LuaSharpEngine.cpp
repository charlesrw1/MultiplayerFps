#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "Framework/ClassBase.h"
#include "Framework/Util.h"
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
					 std::span<const Class> classes)
	: bindings{pass, declarations, symbols, classes} {
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
