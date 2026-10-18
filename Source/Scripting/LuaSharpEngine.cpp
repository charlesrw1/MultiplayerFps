#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "../../LuaSharp/src/Builtins.h"
#include "Framework/ClassBase.h"
#include "Framework/ClassTypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Util.h"
#include "Framework/Files.h"
#include "Framework/StringUtils.h"
#include "Scripting/ScriptManager.h"
#include "Assets/IAsset.h"
#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_map>

LuaSharpEngine* LuaSharpEngine::inst = nullptr;

namespace {
// PROP_SCRIPT_FIELD storage: offset bytes into the script object of the owner (LuaSharp/docs/native-abi.md).
uint8_t* script_slot(const PropertyInfo& p, const void* inst) {
	auto* script = static_cast<uint8_t*>(static_cast<const ClassBase*>(inst)->get_script_proxy());
	ASSERT(script && "a script field needs the owner's script object");
	return script + p.offset;
}
template <typename T> T& slot_as(uint8_t* slot) { return *reinterpret_cast<T*>(slot); }
// An extern-class field holds the proxy of the engine object, or null.
ClassBase* object_in(uint8_t* slot) {
	void* proxy = slot_as<void*>(slot);
	return proxy ? static_cast<ClassBase*>(luasharp::NativeRuntime::NativeOf(proxy)) : nullptr;
}
void store_object(uint8_t* slot, const PropertyInfo& p, ClassBase* object) {
	ASSERT(!object || object->get_type().is_a(*p.class_type));
	slot_as<void*>(slot) = lsbind::proxy_of(object);
}

void script_get(const PropertyInfo& p, const void* inst, PropValue& out) {
	uint8_t* slot = script_slot(p, inst);
	switch (p.type) {
	case core_type_id::Bool: out = slot_as<uint8_t>(slot) != 0; break;
	case core_type_id::Int64: out = slot_as<int64_t>(slot); break;
	case core_type_id::Double: out = slot_as<double>(slot); break;
	case core_type_id::ScriptString: out = std::string(lsbind::view(slot_as<void*>(slot))); break;
	case core_type_id::Vec2: out = lsbind::to_glm(slot_as<lsbind::Vec2>(slot)); break;
	case core_type_id::Vec3: out = lsbind::to_glm(slot_as<lsbind::Vec3>(slot)); break;
	case core_type_id::Quat: out = lsbind::to_glm(slot_as<lsbind::Quat>(slot)); break;
	case core_type_id::AssetPtr: out = AssetRef{static_cast<IAsset*>(object_in(slot)), p.class_type}; break;
	case core_type_id::ObjHandlePtr: {
		auto* object = static_cast<BaseUpdater*>(object_in(slot));
		out = EntityRef{object ? object->get_instance_id() : 0};
	} break;
	default: ASSERT(!"not a script field type");
	}
}

void script_set(const PropertyInfo& p, void* inst, const PropValue& in, SetMode) {
	uint8_t* slot = script_slot(p, inst);
	switch (p.type) {
	case core_type_id::Bool: slot_as<uint8_t>(slot) = prop_value_get<bool>(in) ? 1 : 0; break;
	case core_type_id::Int64: slot_as<int64_t>(slot) = prop_value_get<int64_t>(in); break;
	case core_type_id::Double: slot_as<double>(slot) = prop_value_get<double>(in); break;
	case core_type_id::ScriptString: slot_as<void*>(slot) = lsbind::string(prop_value_get<std::string>(in)); break;
	case core_type_id::Vec2: slot_as<lsbind::Vec2>(slot) = lsbind::to_ls(prop_value_get<glm::vec2>(in)); break;
	case core_type_id::Vec3: slot_as<lsbind::Vec3>(slot) = lsbind::to_ls(prop_value_get<glm::vec3>(in)); break;
	case core_type_id::Quat: slot_as<lsbind::Quat>(slot) = lsbind::to_ls(prop_value_get<glm::quat>(in)); break;
	case core_type_id::AssetPtr: store_object(slot, p, prop_value_get<AssetRef>(in).asset); break;
	case core_type_id::ObjHandlePtr: {
		const int64_t handle = prop_value_get<EntityRef>(in).handle;
		store_object(slot, p, handle && eng ? eng->get_object(handle) : nullptr);
	} break;
	default: ASSERT(!"not a script field type");
	}
}

const PropertyAccess script_field_access{script_get, script_set};
} // namespace

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
	// Throws std::runtime_error for an extern-class field that the editor cannot edit.
	void make_props() {
		using luasharp::NativeValueKind;
		const auto fields = luasharp::NativeRuntime::EditableFields(script_type);
		prop_strings.reserve(fields.size() * 3); // PropertyInfo strings point into these
		auto keep = [&](std::string_view text) { return text.empty() ? "" : prop_strings.emplace_back(text).c_str(); };
		for (const auto& field : fields) {
			ASSERT(field.offset <= std::numeric_limits<uint16_t>::max());
			PropertyInfo prop(keep(field.name), static_cast<uint16_t>(field.offset), PROP_DEFAULT | PROP_SCRIPT_FIELD);
			prop.access = &script_field_access;
			prop.custom_type_str = keep(field.editor);
			if (!field.former.empty())
				prop.former_name = keep(field.former);
			switch (field.kind) {
			case NativeValueKind::Bool: prop.type = core_type_id::Bool; break;
			case NativeValueKind::Int: prop.type = core_type_id::Int64; break;
			case NativeValueKind::Float: prop.type = core_type_id::Double; break;
			case NativeValueKind::String: prop.type = core_type_id::ScriptString; break;
			case NativeValueKind::Struct:
				if (field.typeName == "Vec2") prop.type = core_type_id::Vec2;
				else if (field.typeName == "Vec3") prop.type = core_type_id::Vec3;
				else if (field.typeName == "Quat") prop.type = core_type_id::Quat;
				else ASSERT(!"the checker allows only Vec2, Vec3 and Quat structs");
				break;
			case NativeValueKind::Object: set_object_type(prop, field.typeName); break;
			default: ASSERT(!"the checker allows only these [Editable] types");
			}
			prop_storage.push_back(prop);
		}
		ASSERT(prop_strings.size() <= fields.size() * 3); // no reallocation moved a string
		if (prop_storage.empty())
			return;
		prop_list.list = prop_storage.data();
		prop_list.count = static_cast<int>(prop_storage.size());
		prop_list.type_name = classname;
		set_props(&prop_list);
	}

	// An asset field is an AssetRef; an Entity field is an EntityRef.
	void set_object_type(PropertyInfo& prop, std::string_view extern_class) {
		const ClassTypeInfo* type = ClassBase::find_class(std::string(extern_class).c_str());
		ASSERT(type); // every extern class is a reflected engine class
		prop.class_type = type;
		if (type->is_a(IAsset::StaticType))
			prop.type = core_type_id::AssetPtr;
		else if (type->is_a(Entity::StaticType))
			prop.type = core_type_id::ObjHandlePtr;
		else
			throw std::runtime_error("[Editable] field '" + std::string(prop.name) + "' of '" + name + "' has type '" +
									 std::string(extern_class) + "'; an extern-class field must be an asset or an Entity");
	}

	std::string name;
	std::vector<std::string> prop_strings;
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
					 std::span<const Class> classes, std::span<const Scriptable> scriptables,
					 std::span<const StructLayout> structs)
	: bindings{pass, declarations, symbols, classes, scriptables, structs} {
	bindings_registry().push_back(&bindings);
}
std::span<const Bindings* const> all_bindings() { return bindings_registry(); }
std::string declaration_source() {
	std::string source;
	for (const Bindings* b : all_bindings())
		for (const char* chunk : b->declarations)
			source.append(chunk).append("\n");
	return source;
}

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
List* new_list(ElementKind kind, const char* type, size_t element_size, size_t count) {
	ASSERT(LuaSharpEngine::inst);
	auto& runtime = LuaSharpEngine::inst->runtime();
	const void* element_type = nullptr;
	if (kind == ElementKind::Object)
		element_type = runtime.FindNativeClass(type);
	else if (kind == ElementKind::Struct)
		element_type = runtime.FindStructType(type);
	ASSERT((kind != ElementKind::Object && kind != ElementKind::Struct) || element_type); // compile() adopts every struct type
	auto* list = static_cast<List*>(runtime.NewList(static_cast<luasharp::NativeElementKind>(kind), element_size, element_type, count));
	ASSERT(list);
	return list;
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
		for (const lsbind::Symbol& s : b->symbols)
			symbols_.push_back({s.name, s.address});
		for (const lsbind::Scriptable& s : b->scriptables) {
			[[maybe_unused]] const bool added = scriptables_.emplace(s.name, &s).second;
			ASSERT(added);
		}
		for (const lsbind::StructLayout& s : b->structs) {
			[[maybe_unused]] const bool added = struct_layouts_.emplace(s.name, &s).second;
			ASSERT(added); // one codegen pass owns each struct
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
	declarations_ = lsbind::declaration_source();
	types_ = luasharp::DeclarationTypes({declarations_});
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
	inst = nullptr;
}

luasharp::NativeModule LuaSharpEngine::compile(std::string_view source) {
	luasharp::NativeCompileOptions options;
	options.externalTypes = types_;
	options.symbols = symbols_;
	options.runtime = runtime_;
	options.initialize = false; // top-level code can call thunks that need the adopted struct types
	auto module = luasharp::NativeModule::Compile(source, options);
	check_struct_layouts(module);
	adopt_struct_types(module);
	module.Initialize();
	return module;
}

void LuaSharpEngine::check_struct_layouts(const luasharp::NativeModule& module) const {
	for (const std::string& name : module.Generated().externalStructs) {
		const auto found = struct_layouts_.find(name);
		ASSERT(found != struct_layouts_.end()); // every declared struct comes from a pass
		const lsbind::StructLayout& host = *found->second;
		const auto script = luasharp::NativeRuntime::StructLayout(module.ExternalStructType(name));
		const auto mismatch = [&](const std::string& what) {
			throw std::runtime_error("LuaSharp struct '" + name + "' does not match its C++ mirror: " + what);
		};
		if (script.size != host.size || script.fields.size() != host.fields.size())
			mismatch("size " + std::to_string(script.size) + " vs " + std::to_string(host.size));
		for (size_t i = 0; i < host.fields.size(); i++)
			if (script.fields[i].name != host.fields[i].name || script.fields[i].offset != host.fields[i].offset)
				mismatch("field '" + std::string(host.fields[i].name) + "'");
	}
}

void LuaSharpEngine::adopt_struct_types(const luasharp::NativeModule& module) {
	for (const std::string& name : luasharp::NativeStructTypes())
		runtime_->AdoptStructType(module.StructType(name));
	for (const std::string& name : module.Generated().externalStructs)
		runtime_->AdoptStructType(module.StructType(name));
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

int LuaSharpEngine::load_script_files(const std::string& dir, std::span<const std::string_view> skip_dirs) {
	auto skipped = [&](const std::string& path) {
		for (std::string_view skip : skip_dirs)
			if (path.find("/" + std::string(skip) + "/") != std::string::npos)
				return true;
		return path.find("/.luasharp/") != std::string::npos; // declarations for tools; compile() has them already
	};
	int loaded = 0;
	for (const std::string& full_path : FileSys::find_game_files_path(dir)) {
		std::string path = FileSys::get_game_path_from_full_path(full_path);
		std::replace(path.begin(), path.end(), '\\', '/');
		if (StringUtils::get_extension_no_dot(path) != "ls" || skipped(path))
			continue;
		auto file = FileSys::open_read_game(path);
		if (!file) {
			sys_print(Error, "LuaSharp: cannot read %s\n", path.c_str());
			continue;
		}
		std::string source(file->size(), ' ');
		file->read(source.data(), source.size());
		try {
			load(path, source);
			loaded++;
			sys_print(Debug, "LuaSharp: loaded %s\n", path.c_str());
		} catch (const std::exception& error) {
			sys_print(Error, "LuaSharp: cannot load %s:\n%s\n", path.c_str(), error.what());
		}
	}
	return loaded;
}

const luasharp::NativeModule* LuaSharpEngine::find_module(const std::string& name) const {
	const auto found = modules_.find(name);
	return found != modules_.end() ? found->second.module.get() : nullptr;
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
	if (ScriptManager::inst) {
		// Lua code can get these objects too (get_component and similar), so the classes need Lua prototypes.
		for (const auto& type : loaded.classes)
			ScriptManager::inst->init_this_class_type(type.get());
		for (const auto& type : loaded.classes)
			ScriptManager::inst->set_class_type_global(type.get());
	}
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

void LuaSharpEngine::collect_if_needed() {
	constexpr size_t MinThreshold = 4 * 1024 * 1024;
	if (runtime_->AllocatedBytes() < std::max(collect_threshold_, MinThreshold))
		return;
	collect();
	collect_threshold_ = runtime_->AllocatedBytes() * 2;
}

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
