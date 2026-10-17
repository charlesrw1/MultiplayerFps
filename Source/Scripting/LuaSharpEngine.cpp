#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "Framework/ClassBase.h"
#include "Framework/ClassTypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Util.h"
#include <limits>
#include <stdexcept>
#include <unordered_map>

LuaSharpEngine* LuaSharpEngine::inst = nullptr;

// The ClassTypeInfo of a script class that derives from a scriptable engine class (LuaSharpEngine::load).
// Its objects are ScriptImpl_X objects whose get_type() is this type.
class LuaSharpClassTypeInfo final : public ClassTypeInfo
{
public:
	LuaSharpClassTypeInfo(const luasharp::NativeModule& module, const void* script_type, const ClassTypeInfo& super,
						  const ClassTypeInfo& engine_class, bool can_create)
		: ClassTypeInfo("", &super, nullptr, can_create ? &allocate_object : nullptr, false, nullptr, 0, nullptr,
						true /* LuaSharpEngine registers it */, can_create),
		  module(module), script_type(script_type), engine_class(engine_class),
		  name(luasharp::NativeRuntime::TypeName(script_type)) {
		classname = name.c_str();
		make_props();
	}

	const luasharp::NativeModule& module;
	const void* const script_type; // LS_TypeInfo
	const ClassTypeInfo& engine_class; // the nearest C++ class

private:
	static ClassBase* allocate_object(const ClassTypeInfo* type) {
		ASSERT(LuaSharpEngine::inst);
		const auto& self = static_cast<const LuaSharpClassTypeInfo&>(*type);
		try {
			return LuaSharpEngine::inst->make(self.module, self.name, &self).release();
		} catch (const std::exception& error) {
			sys_print(Error, "LuaSharp: cannot create %s: %s\n", self.classname, error.what());
			return nullptr;
		}
	}
	void make_props() {
		using luasharp::NativeValueKind;
		const auto fields = luasharp::NativeRuntime::EditableFields(script_type);
		prop_names.reserve(fields.size()); // PropertyInfo::name points into these
		for (const auto& field : fields) {
			ASSERT(field.offset <= std::numeric_limits<uint16_t>::max());
			const char* field_name = prop_names.emplace_back(field.name).c_str();
			const auto offset = static_cast<uint16_t>(field.offset);
			constexpr uint32_t flags = PROP_DEFAULT | PROP_SCRIPT_FIELD;
			PropertyInfo prop(field_name, offset, flags);
			switch (field.kind) {
			case NativeValueKind::Bool: prop = make_bool_property(field_name, offset, flags); break;
			case NativeValueKind::Int: prop = make_integer_property(field_name, offset, flags, sizeof(int64_t)); break;
			case NativeValueKind::Float: prop.type = core_type_id::Double; break;
			case NativeValueKind::String: prop.type = core_type_id::ScriptString; break;
			case NativeValueKind::Struct:
				if (field.structName == "Vec2") prop.type = core_type_id::Vec2;
				else if (field.structName == "Vec3") prop.type = core_type_id::Vec3;
				else if (field.structName == "Quat") prop.type = core_type_id::Quat;
				else ASSERT(!"the checker allows only Vec2, Vec3 and Quat structs");
				break;
			default: ASSERT(!"the checker allows only these [Editable] types");
			}
			prop_storage.push_back(prop);
		}
		ASSERT(prop_names.size() == fields.size()); // no reallocation moved a name
		if (prop_storage.empty())
			return;
		prop_list.list = prop_storage.data();
		prop_list.count = static_cast<int>(prop_storage.size());
		prop_list.type_name = classname;
		props = &prop_list;
	}

	std::string name;
	std::vector<std::string> prop_names;
	std::vector<PropertyInfo> prop_storage;
	PropertyInfoList prop_list;
};

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
	ASSERT(!g_script_strings.read && !g_script_strings.write);
	g_script_strings.read = [](const void* slot) { return std::string(lsbind::view(*static_cast<void* const*>(slot))); };
	g_script_strings.write = [](void* slot, std::string_view text) { *static_cast<void**>(slot) = lsbind::string(text); };
	inst = this;
}

LuaSharpEngine::~LuaSharpEngine() {
	ASSERT(inst == this);
	for ([[maybe_unused]] ClassBase* object : proxied_)
		ASSERT(!dynamic_cast<const LuaSharpClassTypeInfo*>(&object->get_type()) && "an object of a loaded script class outlives the engine");
	while (!proxied_.empty())
		release(*proxied_.begin());
	for (auto& [name, loaded] : modules_)
		unregister_classes(loaded);
	g_script_strings = {};
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
	return make(module, script_class, find_type(module, script_class));
}

std::unique_ptr<ClassBase> LuaSharpEngine::make(const luasharp::NativeModule& module, std::string_view script_class,
												 const LuaSharpClassTypeInfo* type) {
	using luasharp::NativeRuntime;
	ASSERT(module.Runtime() == runtime_);
	const std::string prefix = "ls_" + std::string(script_class);
	const auto alloc = reinterpret_cast<void* (*)()>(module.FindSymbol(prefix + "__host_alloc"));
	const auto init = reinterpret_cast<void (*)(void*)>(module.FindSymbol(prefix + "__init"));
	if (!alloc || !init)
		throw std::runtime_error("'" + std::string(script_class) +
								 "' is not a script class that derives from an engine class, or it is abstract");
	const void* script_type = module.HostClassType(script_class);
	ASSERT(script_type && (!type || type->script_type == script_type));

	const std::string base(NativeRuntime::NativeBaseTypeName(script_type));
	const ClassTypeInfo* info = ClassBase::find_class(base.c_str());
	ASSERT(info); // every bound class is reflected
	ASSERT(!type || &type->engine_class == info);
	std::unique_ptr<ClassBase> object;
	if (const lsbind::Scriptable* scriptable = scriptable_of(*info)) {
		void* const* slots = slots_of(script_type, *scriptable);
		object.reset(info->scriptable_allocate(type ? type : info)); // ScriptImpl_X::get_type()
		scriptable->attach(object.get(), slots);
	} else {
		ASSERT(!type); // load registers only classes of scriptable engine classes
		if (NativeRuntime::TypeHostOverrideCount(script_type) != 0)
			throw std::runtime_error("'" + std::string(script_class) + "' overrides methods of '" + base +
									 "', which is not scriptable (CLASS_BODY(" + base + ", scriptable))");
		object.reset(info->allocate_this_type());
		if (!object)
			throw std::runtime_error("engine class '" + base + "' is abstract and not scriptable");
	}
	void* script = alloc();
	if (!script)
		throw std::runtime_error("LuaSharp could not allocate '" + std::string(script_class) + "': " + runtime_->LastError());
	ASSERT(!object->script_proxy);
	NativeRuntime::SetNative(script, object.get());
	object->script_proxy = script;
	runtime_->AddRootSlot(&object->script_proxy);
	proxied_.insert(object.get());
	init(script); // the script constructor can use the engine object
	return object;
}

const lsbind::Scriptable* LuaSharpEngine::scriptable_of(const ClassTypeInfo& engine_class) const {
	const auto found = scriptables_.find(engine_class.classname);
	return engine_class.scriptable_allocate && found != scriptables_.end() ? found->second : nullptr;
}

void* const* LuaSharpEngine::slots_of(const void* script_type, const lsbind::Scriptable& scriptable) {
	using luasharp::NativeRuntime;
	std::vector<void*> slots;
	size_t used = 0;
	for (const char* name : scriptable.slot_names) {
		slots.push_back(NativeRuntime::TypeHostOverride(script_type, name));
		used += slots.back() != nullptr;
	}
	if (used != NativeRuntime::TypeHostOverrideCount(script_type))
		throw std::runtime_error(std::string("a script class overrides a method of '") + scriptable.name +
								 "' that ScriptImpl cannot call (it returns a reference)");
	slots.push_back(nullptr); // so that data() is never null: a null ls_slots means a Lua object
	// Tables are shared by content, so a table never names code of an unloaded module that reused an address.
	return slot_tables_.insert(std::move(slots)).first->data();
}

const luasharp::NativeModule& LuaSharpEngine::load(const std::string& name, std::string_view source) {
	if (modules_.contains(name))
		throw std::runtime_error("LuaSharp module '" + name + "' is already loaded");
	LoadedModule loaded;
	loaded.module = std::make_unique<luasharp::NativeModule>(compile(source));
	register_classes(loaded); // throws before the module is kept
	return *modules_.emplace(name, std::move(loaded)).first->second.module;
}

void LuaSharpEngine::register_classes(LoadedModule& loaded) {
	using luasharp::NativeRuntime;
	const luasharp::NativeModule& module = *loaded.module;
	auto find_loaded = [&](const void* script_type) -> const LuaSharpClassTypeInfo* {
		for (const auto& type : loaded.classes)
			if (type->script_type == script_type)
				return type.get();
		return nullptr;
	};
	for (const std::string& name : module.Generated().hostClasses) {
		const void* script_type = module.HostClassType(name);
		ASSERT(script_type);
		if (ClassBase::find_class(name.c_str()))
			throw std::runtime_error("LuaSharp class '" + name + "' has the name of a class that already exists");
		const std::string base(NativeRuntime::NativeBaseTypeName(script_type));
		const ClassTypeInfo* engine_class = ClassBase::find_class(base.c_str());
		ASSERT(engine_class);
		const lsbind::Scriptable* scriptable = scriptable_of(*engine_class);
		if (!scriptable) {
			// create() still makes these objects, but they have the type of the engine class.
			if (!NativeRuntime::EditableFields(script_type).empty())
				throw std::runtime_error("LuaSharp class '" + name + "' has [Editable] fields, but engine class '" + base +
										 "' is not scriptable (CLASS_BODY(" + base + ", scriptable))");
			continue;
		}
		static_cast<void>(slots_of(script_type, *scriptable)); // throws for overrides that ScriptImpl cannot call
		const void* script_base = NativeRuntime::ScriptBaseType(script_type);
		const ClassTypeInfo* super = script_base ? find_loaded(script_base) : engine_class;
		ASSERT(super); // hostClasses lists base classes first
		const bool can_create = module.FindSymbol("ls_" + name + "__host_alloc") != nullptr; // not abstract
		loaded.classes.push_back(std::make_unique<LuaSharpClassTypeInfo>(module, script_type, *super, *engine_class, can_create));
	}
	for (const auto& type : loaded.classes)
		ClassBase::register_class(type.get());
	ClassBase::post_changes_class_init();
}

void LuaSharpEngine::unregister_classes(LoadedModule& loaded) {
	for (const auto& type : loaded.classes)
		ClassBase::unregister_class(type.get());
	ClassBase::post_changes_class_init();
	loaded.classes.clear();
}

const LuaSharpClassTypeInfo* LuaSharpEngine::find_type(const luasharp::NativeModule& module,
													   std::string_view script_class) const {
	for (const auto& [name, loaded] : modules_)
		if (loaded.module.get() == &module)
			for (const auto& type : loaded.classes)
				if (type->classname == script_class)
					return type.get();
	return nullptr;
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
