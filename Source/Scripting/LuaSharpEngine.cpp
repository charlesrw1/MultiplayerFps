#include "Scripting/LuaSharpEngine.h"
#include "Scripting/LuaSharpBinding.h"
#include "../../LuaSharp/src/CBackend.h"
#include "../../LuaSharp/src/Builtins.h"
#include "../../LuaSharp/src/NativeDapServer.h"
#include "../../LuaSharp/src/NativeDebugger.h"
#include "../../LuaSharp/src/NativeReload.h"
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
	case core_type_id::Int64:
	case core_type_id::Enum32: out = slot_as<int64_t>(slot); break; // an enum slot is an ls_int
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
	case core_type_id::Int64:
	case core_type_id::Enum32: slot_as<int64_t>(slot) = prop_value_get<int64_t>(in); break;
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
		  module(&module), script_type(script_type), engine_class(engine_class),
		  name(luasharp::NativeRuntime::TypeName(script_type)) {
		classname = name.c_str();
		is_luasharp_implemented = true;
		make_props();
	}

	// A hot reload keeps this type for the same class of the new module. Its [Editable] fields must be the same.
	void rebind(const luasharp::NativeModule& next, const void* next_type) {
		ASSERT(luasharp::NativeRuntime::TypeName(next_type) == name);
		module = &next;
		script_type = next_type;
	}

	const luasharp::NativeModule* module;
	const void* script_type; // LS_TypeInfo
	const ClassTypeInfo& engine_class; // the nearest C++ class

private:
	static ClassBase* allocate_object(const ClassTypeInfo* type) {
		ASSERT(LuaSharpEngine::inst);
		const auto& self = static_cast<const LuaSharpClassTypeInfo&>(*type);
		try {
			return LuaSharpEngine::inst->make(*self.module, self.name, &self).release();
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
			case NativeValueKind::Int:
				if (field.enumMembers.empty())
					prop.type = core_type_id::Int64;
				else {
					prop.type = core_type_id::Enum32;
					prop.enum_type = &enum_type_of(field);
				}
				break;
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

	// An enum field's type, with the LuaSharp values. Names point into the module, which outlives this type.
	const EnumTypeInfo& enum_type_of(const luasharp::NativeEditableField& field) {
		ASSERT(!field.enumMembers.empty()); // LuaSharp enums have members
		for (const auto& existing : enums)
			if (field.typeName == existing->info.name)
				return existing->info;
		auto entry = std::make_unique<ScriptEnum>();
		for (const auto& member : field.enumMembers)
			entry->pairs.emplace_back(member.name.data(), member.name.data(), member.value);
		entry->info = EnumTypeInfo::make_unregistered(field.typeName.data(), entry->pairs.data(), entry->pairs.size());
		return enums.emplace_back(std::move(entry))->info;
	}

	struct ScriptEnum
	{
		std::vector<EnumIntPair> pairs;
		EnumTypeInfo info = EnumTypeInfo::make_unregistered("", nullptr, 0);
	};

	std::string name;
	std::vector<std::string> prop_strings;
	std::vector<PropertyInfo> prop_storage;
	std::vector<std::unique_ptr<ScriptEnum>> enums;
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

std::string join_lines(const std::vector<std::string>& lines) {
	std::string text;
	for (const std::string& line : lines)
		text += (text.empty() ? "" : "\n") + line;
	return text;
}

// The PropertyInfos of a LuaSharpClassTypeInfo come from these, so a hot reload cannot change them.
bool same_editable_fields(const void* type, const void* other) {
	using luasharp::NativeEditableField;
	using luasharp::NativeEnumMember;
	const auto same_member = [](const NativeEnumMember& a, const NativeEnumMember& b) { return a.name == b.name && a.value == b.value; };
	const auto same = [&](const NativeEditableField& a, const NativeEditableField& b) {
		return a.name == b.name && a.offset == b.offset && a.kind == b.kind && a.typeName == b.typeName && a.editor == b.editor &&
			   a.former == b.former && std::ranges::equal(a.enumMembers, b.enumMembers, same_member);
	};
	return std::ranges::equal(luasharp::NativeRuntime::EditableFields(type), luasharp::NativeRuntime::EditableFields(other), same);
}
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
	if (debugger_) {
		runtime_->SetPrintHandler({});
		dap_server_.reset(); // detaches a client, so no script is stopped
		for (auto& [name, loaded] : modules_)
			if (loaded.module->Debug())
				debugger_->RemoveModule(*loaded.module);
		debugger_.reset();
	}
	for ([[maybe_unused]] ClassBase* object : proxied_)
		ASSERT(!dynamic_cast<const LuaSharpClassTypeInfo*>(&object->get_type()) && "an object of a loaded script class outlives the engine");
	while (!proxied_.empty())
		release(*proxied_.begin());
	for (auto& [name, loaded] : modules_)
		unregister_classes(loaded);
	inst = nullptr;
}

luasharp::NativeModule LuaSharpEngine::compile(std::string_view source) {
	const luasharp::SourceFile file{"module.ls", source};
	auto module = compile_module(std::span(&file, 1), false);
	module.Initialize();
	return module;
}

luasharp::NativeModule LuaSharpEngine::compile_module(std::span<const luasharp::SourceFile> files, bool debug) {
	luasharp::NativeCompileOptions options;
	options.externalTypes = types_;
	options.symbols = symbols_;
	options.runtime = runtime_;
	options.initialize = false; // top-level code can call thunks that need the adopted struct types
	options.emitDebugHooks = debug;
	auto module = luasharp::NativeModule::Compile(files, options);
	check_struct_layouts(module);
	adopt_struct_types(module);
	return module;
}

void LuaSharpEngine::start_debugger(uint16_t port) {
	ASSERT(!debugger_);
	debugger_ = std::make_unique<luasharp::NativeDebugger>(runtime_);
	try {
		dap_server_ = std::make_unique<luasharp::NativeDapServer>(*debugger_, port);
	} catch (...) {
		debugger_.reset();
		throw;
	}
	runtime_->SetPrintHandler([server = dap_server_.get()](std::string_view text) {
		fwrite(text.data(), 1, text.size(), stdout);
		server->SendOutput(text);
	});
	sys_print(Info, "LuaSharp: debugger listens on port %d\n", int(dap_server_->Port()));
}

void LuaSharpEngine::wait_for_debugger() {
	ASSERT(dap_server_);
	sys_print(Info, "LuaSharp: waiting for a debugger on port %d\n", int(dap_server_->Port()));
	while (!dap_server_->WaitUntilConfigured(std::chrono::hours(1))) {
	}
	sys_print(Info, "LuaSharp: debugger attached\n");
}

uint16_t LuaSharpEngine::debugger_port() const { return dap_server_ ? dap_server_->Port() : 0; }

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
	// A client sets breakpoints by the path of the file.
	const luasharp::SourceFile file{debugger_ ? FileSys::get_full_path_from_game_path(name) : name, source};
	return load(name, std::span(&file, 1));
}

const luasharp::NativeModule& LuaSharpEngine::load(const std::string& name, std::span<const luasharp::SourceFile> files) {
	ASSERT(!files.empty());
	if (modules_.contains(name))
		throw std::runtime_error("LuaSharp module '" + name + "' is already loaded");
	LoadedModule loaded;
	loaded.module = std::make_unique<luasharp::NativeModule>(compile_module(files, debugger_ != nullptr));
	if (const auto errors = check_classes(loaded, *loaded.module); !errors.empty())
		throw std::runtime_error(join_lines(errors));
	if (debugger_)
		debugger_->AddModule(*loaded.module);
	try {
		loaded.module->Initialize();
	} catch (...) {
		if (debugger_)
			debugger_->RemoveModule(*loaded.module);
		throw;
	}
	bind_classes(loaded, *loaded.module);
	return *modules_.emplace(name, std::move(loaded)).first->second.module;
}

LuaSharpEngine::ScriptSources LuaSharpEngine::read_script_files(const std::string& dir, std::span<const std::string> skip_dirs) const {
	auto skipped = [&](const std::string& path) {
		for (const std::string& skip : skip_dirs)
			if (path.find("/" + skip + "/") != std::string::npos)
				return true;
		return path.find("/.luasharp/") != std::string::npos; // declarations for tools; compile() has them already
	};
	std::vector<std::string> paths;
	for (const std::string& full_path : FileSys::find_game_files_path(dir)) {
		std::string path = FileSys::get_game_path_from_full_path(full_path);
		std::replace(path.begin(), path.end(), '\\', '/');
		if (StringUtils::get_extension_no_dot(path) == "ls" && !skipped(path))
			paths.push_back(std::move(path));
	}
	std::sort(paths.begin(), paths.end()); // top-level code runs in this order
	ScriptSources sources;
	for (const std::string& path : paths) {
		auto file = FileSys::open_read_game(path);
		if (!file) {
			sys_print(Error, "LuaSharp: cannot read %s\n", path.c_str());
			continue;
		}
		std::string& source = sources.texts.emplace_back(file->size(), ' ');
		file->read(source.data(), source.size());
		// A client sets breakpoints by the path of the file.
		sources.files.push_back({debugger_ ? FileSys::get_full_path_from_game_path(path) : path, {}});
	}
	for (size_t i = 0; i < sources.files.size(); i++)
		sources.files[i].text = sources.texts[i]; // texts no longer grows; moving the vector keeps its strings
	return sources;
}

int LuaSharpEngine::load_script_files(const std::string& dir, std::span<const std::string_view> skip_dirs) {
	auto& skips = script_folders_[dir];
	skips.assign(skip_dirs.begin(), skip_dirs.end());
	const ScriptSources sources = read_script_files(dir, skips);
	if (sources.files.empty())
		return 0;
	try {
		const auto& times = load(dir, sources.files).Times();
		sys_print(Debug, "LuaSharp: loaded %s (%d files; frontend %.1f ms, C %.1f ms, tcc %.1f ms)\n", dir.c_str(),
				  int(sources.files.size()), times.frontend, times.emitC, times.tcc);
		return static_cast<int>(sources.files.size());
	} catch (const std::exception& error) {
		sys_print(Error, "LuaSharp: cannot load %s:\n%s\n", dir.c_str(), error.what());
		return 0;
	}
}

bool LuaSharpEngine::reload_script_files(const std::string& dir) {
	const auto folder = script_folders_.find(dir);
	ASSERT(folder != script_folders_.end()); // load_script_files names the folders
	const auto found = modules_.find(dir);
	if (found == modules_.end()) {
		const std::vector<std::string_view> skips(folder->second.begin(), folder->second.end());
		return load_script_files(dir, skips) > 0;
	}
	const ScriptSources sources = read_script_files(dir, folder->second);
	if (sources.files.empty()) {
		sys_print(Error, "LuaSharp: cannot reload %s: the folder has no .ls files\n", dir.c_str());
		return false;
	}
	return reload(dir, sources.files);
}

bool LuaSharpEngine::reload(const std::string& name, std::span<const luasharp::SourceFile> files) {
	ASSERT(!files.empty());
	const auto found = modules_.find(name);
	ASSERT(found != modules_.end());
	LoadedModule& loaded = found->second;
	auto report = [&](const std::string& errors) {
		sys_print(Error, "LuaSharp: cannot reload %s; the scripts that run do not change:\n%s\n", name.c_str(), errors.c_str());
		return false;
	};
	std::unique_ptr<luasharp::NativeModule> next;
	try {
		next = std::make_unique<luasharp::NativeModule>(compile_module(files, debugger_ != nullptr));
	} catch (const std::exception& error) {
		return report(error.what());
	}
	const luasharp::NativeReloadPlan plan = luasharp::PlanReload(*loaded.module, *next);
	std::vector<std::string> errors = plan.errors;
	for (std::string& error : check_classes(loaded, *next))
		errors.push_back(std::move(error));
	if (!errors.empty())
		return report(join_lines(errors));
	for (const std::string& warning : plan.warnings)
		sys_print(Warning, "LuaSharp: %s\n", warning.c_str());

	luasharp::NativeReloadResult result;
	try {
		result = luasharp::ApplyReload(*loaded.module, *next, plan);
	} catch (const std::exception& error) {
		loaded.retired.push_back(std::move(next)); // its static initializers can have made objects
		return report(error.what());
	}
	if (debugger_) {
		if (loaded.module->Debug()) // a module that loaded before start_debugger has no debug tables
			debugger_->RemoveModule(*loaded.module);
		debugger_->AddModule(*next);
	}
	bind_classes(loaded, *next);
	loaded.retired.push_back(std::move(loaded.module));
	loaded.module = std::move(next);
	attach_slots();
	const auto& times = loaded.module->Times();
	sys_print(Info, "LuaSharp: reloaded %s (%d files; %d objects retyped, %d defaults updated; frontend %.1f ms, C %.1f ms, tcc %.1f ms)\n",
			  name.c_str(), int(files.size()), int(result.retypedObjects), int(result.updatedDefaults), times.frontend,
			  times.emitC, times.tcc);
	return true;
}

bool LuaSharpEngine::reload_if_requested() {
	if (!reload_requested_)
		return false;
	reload_requested_ = false;
	bool reloaded = false;
	for (const auto& [dir, skip_dirs] : script_folders_)
		reloaded |= reload_script_files(dir);
	return reloaded;
}

const luasharp::NativeModule* LuaSharpEngine::find_module(const std::string& name) const {
	const auto found = modules_.find(name);
	return found != modules_.end() ? found->second.module.get() : nullptr;
}

LuaSharpClassTypeInfo* LuaSharpEngine::class_named(const LoadedModule& loaded, std::string_view name) {
	for (const auto& type : loaded.classes)
		if (type && type->classname == name)
			return type.get();
	return nullptr;
}

std::vector<std::string> LuaSharpEngine::check_classes(const LoadedModule& loaded, const luasharp::NativeModule& next) {
	using luasharp::NativeRuntime;
	std::vector<std::string> errors;
	for (const std::string& name : next.Generated().hostClasses) {
		const void* script_type = next.HostClassType(name);
		ASSERT(script_type);
		const LuaSharpClassTypeInfo* existing = class_named(loaded, name);
		if (!existing && ClassBase::find_class(name.c_str())) {
			errors.push_back("LuaSharp class '" + name + "' has the name of a class that already exists");
			continue;
		}
		const std::string base(NativeRuntime::NativeBaseTypeName(script_type));
		const ClassTypeInfo* engine_class = ClassBase::find_class(base.c_str());
		ASSERT(engine_class);
		const lsbind::Scriptable* scriptable = scriptable_of(*engine_class);
		if (!scriptable) {
			// create() still makes these objects, but they have the type of the engine class.
			if (!NativeRuntime::EditableFields(script_type).empty())
				errors.push_back("LuaSharp class '" + name + "' has [Editable] fields, but engine class '" + base +
								 "' is not scriptable (CLASS_BODY(" + base + ", scriptable))");
			continue;
		}
		if (existing && !same_editable_fields(existing->script_type, script_type)) {
			errors.push_back("the [Editable] fields of LuaSharp class '" + name + "' changed; a restart applies this change");
			continue;
		}
		try {
			static_cast<void>(slots_of(script_type, *scriptable));
		} catch (const std::exception& error) {
			errors.push_back(error.what());
		}
	}
	return errors;
}

void LuaSharpEngine::bind_classes(LoadedModule& loaded, const luasharp::NativeModule& next) {
	using luasharp::NativeRuntime;
	std::vector<std::unique_ptr<LuaSharpClassTypeInfo>> classes;
	std::vector<LuaSharpClassTypeInfo*> added;
	auto bound = [&](const void* script_type) -> const LuaSharpClassTypeInfo* {
		for (const auto& type : classes)
			if (type->script_type == script_type)
				return type.get();
		return nullptr;
	};
	for (const std::string& name : next.Generated().hostClasses) {
		const void* script_type = next.HostClassType(name);
		const std::string base(NativeRuntime::NativeBaseTypeName(script_type));
		const ClassTypeInfo* engine_class = ClassBase::find_class(base.c_str());
		ASSERT(engine_class);
		if (!scriptable_of(*engine_class))
			continue; // check_classes: the objects have the type of the engine class
		if (LuaSharpClassTypeInfo* existing = class_named(loaded, name)) {
			ASSERT(&existing->engine_class == engine_class); // the layout, with the base, is the same (PlanReload)
			existing->rebind(next, script_type);
			const auto old = std::ranges::find(loaded.classes, existing, &std::unique_ptr<LuaSharpClassTypeInfo>::get);
			classes.push_back(std::move(*old));
			continue;
		}
		const void* script_base = NativeRuntime::ScriptBaseType(script_type);
		const ClassTypeInfo* super = script_base ? bound(script_base) : engine_class;
		ASSERT(super); // hostClasses lists base classes first
		const bool can_create = next.FindSymbol("ls_" + name + "__host_alloc") != nullptr; // not abstract
		added.push_back(classes.emplace_back(std::make_unique<LuaSharpClassTypeInfo>(next, script_type, *super, *engine_class, can_create)).get());
	}
	std::erase(loaded.classes, nullptr);
	unregister_classes(loaded); // the classes that next removed
	loaded.classes = std::move(classes);
	for (LuaSharpClassTypeInfo* type : added)
		ClassBase::register_class(type);
	ClassBase::post_changes_class_init();
	if (ScriptManager::inst) {
		// Lua code can get these objects too (get_component and similar), so the classes need Lua prototypes.
		for (LuaSharpClassTypeInfo* type : added)
			ScriptManager::inst->init_this_class_type(type);
		for (LuaSharpClassTypeInfo* type : added)
			ScriptManager::inst->set_class_type_global(type);
	}
}

void LuaSharpEngine::attach_slots() {
	for (ClassBase* object : proxied_) {
		const auto* type = dynamic_cast<const LuaSharpClassTypeInfo*>(&object->get_type());
		if (!type)
			continue; // a proxy of an engine object
		const lsbind::Scriptable* scriptable = scriptable_of(type->engine_class);
		ASSERT(scriptable);
		scriptable->attach(object, slots_of(type->script_type, *scriptable));
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
