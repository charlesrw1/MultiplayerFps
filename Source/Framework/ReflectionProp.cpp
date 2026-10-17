#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"
#include "Framework/DictParser.h"
#include "Framework/DictWriter.h"
#include "Framework/Util.h"
#include <cassert>
#include "Framework/ArrayReflection.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

uint8_t* PropertyInfo::get_ptr(const void* inst) const {
	if (flags & PROP_LUA_BACKED) {
		auto* cb = const_cast<ClassBase*>(static_cast<const ClassBase*>(inst));
		cb->ensure_lua_shadow();
		uint8_t* shadow = cb->get_lua_field_shadow();
		ASSERT(shadow && "PROP_LUA_BACKED requires owner to provide a shadow buffer");
		return shadow + offset;
	}
	if (flags & PROP_SCRIPT_FIELD) {
		auto* script = static_cast<uint8_t*>(static_cast<const ClassBase*>(inst)->get_script_proxy());
		ASSERT(script && "PROP_SCRIPT_FIELD requires the owner to have a script object");
		return script + offset;
	}
	return (uint8_t*)inst + offset;
}

ScriptStringAccess g_script_strings;

namespace {
bool owns(const PropertyInfo& prop, const void* inst) {
	if (!prop.owner)
		return true;
	for (auto* t = &static_cast<const ClassBase*>(inst)->get_type(); t; t = t->super_typeinfo)
		if (t == prop.owner)
			return true;
	return false;
}

template <typename T> T clamp_to(const PropertyAttributes& a, T v) {
	if (a.has_min() && v < static_cast<T>(a.min))
		v = static_cast<T>(a.min);
	if (a.has_max() && v > static_cast<T>(a.max))
		v = static_cast<T>(a.max);
	return v;
}

// The storage at get_ptr(), by type: properties without an accessor.
void storage_get(const PropertyInfo& p, const void* inst, PropValue& out) {
	const uint8_t* ptr = p.get_ptr(inst);
	switch (p.type) {
	case core_type_id::Bool: out = *reinterpret_cast<const bool*>(ptr); break;
	case core_type_id::Int8: out = int64_t(*reinterpret_cast<const int8_t*>(ptr)); break;
	case core_type_id::Int16: out = int64_t(*reinterpret_cast<const int16_t*>(ptr)); break;
	case core_type_id::Int32: out = int64_t(*reinterpret_cast<const int32_t*>(ptr)); break;
	case core_type_id::Int64: out = *reinterpret_cast<const int64_t*>(ptr); break;
	case core_type_id::Enum8: out = int64_t(*reinterpret_cast<const uint8_t*>(ptr)); break;
	case core_type_id::Enum16: out = int64_t(*reinterpret_cast<const uint16_t*>(ptr)); break;
	case core_type_id::Enum32: out = int64_t(*reinterpret_cast<const int32_t*>(ptr)); break;
	case core_type_id::Float: out = double(*reinterpret_cast<const float*>(ptr)); break;
	case core_type_id::Double: out = *reinterpret_cast<const double*>(ptr); break;
	case core_type_id::Vec2: out = *reinterpret_cast<const glm::vec2*>(ptr); break;
	case core_type_id::Vec3: out = *reinterpret_cast<const glm::vec3*>(ptr); break;
	case core_type_id::Quat: out = *reinterpret_cast<const glm::quat*>(ptr); break;
	case core_type_id::StdString: out = *reinterpret_cast<const std::string*>(ptr); break;
	case core_type_id::ScriptString: out = p.get_script_string(inst); break;
	case core_type_id::StringName: {
		const char* s = reinterpret_cast<const StringName*>(ptr)->get_c_str();
		out = std::string(s ? s : "");
	} break;
	case core_type_id::AssetPtr: out = AssetRef{*reinterpret_cast<IAsset* const*>(ptr), p.class_type}; break;
	case core_type_id::ObjHandlePtr: out = EntityRef{*reinterpret_cast<const int64_t*>(ptr)}; break;
	case core_type_id::ClassTypeInfo: out = *reinterpret_cast<const ClassTypeInfo* const*>(ptr); break;
	default: ASSERT(!"the property has no value"); out = std::monostate{};
	}
}

template <typename T> void store(uint8_t* ptr, T v) {
	*reinterpret_cast<T*>(ptr) = v;
}

void storage_set(const PropertyInfo& p, void* inst, const PropValue& in, SetMode) {
	uint8_t* ptr = p.get_ptr(inst);
	switch (p.type) {
	case core_type_id::Bool: store(ptr, prop_value_get<bool>(in)); break;
	case core_type_id::Int8:
	case core_type_id::Enum8: store(ptr, static_cast<uint8_t>(prop_value_get<int64_t>(in))); break;
	case core_type_id::Int16:
	case core_type_id::Enum16: store(ptr, static_cast<uint16_t>(prop_value_get<int64_t>(in))); break;
	case core_type_id::Int32:
	case core_type_id::Enum32: store(ptr, static_cast<uint32_t>(prop_value_get<int64_t>(in))); break;
	case core_type_id::Int64: store(ptr, prop_value_get<int64_t>(in)); break;
	case core_type_id::Float: store(ptr, static_cast<float>(prop_value_get<double>(in))); break;
	case core_type_id::Double: store(ptr, prop_value_get<double>(in)); break;
	case core_type_id::Vec2: store(ptr, prop_value_get<glm::vec2>(in)); break;
	case core_type_id::Vec3: store(ptr, prop_value_get<glm::vec3>(in)); break;
	case core_type_id::Quat: store(ptr, prop_value_get<glm::quat>(in)); break;
	case core_type_id::StdString: *reinterpret_cast<std::string*>(ptr) = prop_value_get<std::string>(in); break;
	case core_type_id::ScriptString: p.set_script_string(inst, prop_value_get<std::string>(in)); break;
	case core_type_id::StringName:
		*reinterpret_cast<StringName*>(ptr) = StringName(prop_value_get<std::string>(in).c_str());
		break;
	case core_type_id::AssetPtr: store(ptr, prop_value_get<AssetRef>(in).asset); break;
	case core_type_id::ObjHandlePtr: store(ptr, prop_value_get<EntityRef>(in).handle); break;
	case core_type_id::ClassTypeInfo: store(ptr, prop_value_get<const ClassTypeInfo*>(in)); break;
	default: ASSERT(!"the property has no value");
	}
}

const PropertyAccess storage_access{storage_get, storage_set};
} // namespace

bool PropertyInfo::has_value() const {
	switch (type) {
	case core_type_id::List:
	case core_type_id::ActualStruct:
	case core_type_id::Struct:
	case core_type_id::StdUniquePtr: return false;
	default: return true;
	}
}

void PropertyInfo::get_value(const void* inst, PropValue& out) const {
	ASSERT(inst && has_value());
	ASSERT(owns(*this, inst) && "the object is not of the property's class");
	(access ? *access : storage_access).get(*this, inst, out);
}

PropValue PropertyInfo::get_value(const void* inst) const {
	PropValue v;
	get_value(inst, v);
	return v;
}

void PropertyInfo::set_value(void* inst, const PropValue& in, SetMode mode) const {
	ASSERT(inst && has_value());
	ASSERT(owns(*this, inst) && "the object is not of the property's class");
	ASSERT(!attrs.readonly || mode == SetMode::Load);
	if (const auto* d = std::get_if<double>(&in); d && (attrs.has_min() || attrs.has_max())) {
		(access ? *access : storage_access).set(*this, inst, clamp_to(attrs, *d), mode);
		return;
	}
	if (const auto* i = std::get_if<int64_t>(&in); i && (attrs.has_min() || attrs.has_max())) {
		(access ? *access : storage_access).set(*this, inst, clamp_to(attrs, *i), mode);
		return;
	}
	(access ? *access : storage_access).set(*this, inst, in, mode);
}

std::string PropertyInfo::get_script_string(const void* inst) const {
	ASSERT(type == core_type_id::ScriptString && g_script_strings.read);
	return g_script_strings.read(get_ptr(inst));
}

void PropertyInfo::set_script_string(void* inst, std::string_view text) const {
	ASSERT(type == core_type_id::ScriptString && g_script_strings.write);
	g_script_strings.write(get_ptr(inst), text);
}

inline std::string string_view_to_std_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}

static StringView delimit(const char* start, const char character = ',') {
	StringView s;
	s.str_start = start;
	s.str_len = 0;
	while (*start != 0 && *start != character) {
		s.str_len++;
		start++;
	}
	return s;
}

PropertyInfo make_vec3_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Vec3;
	prop.custom_type_str = customtype;
	return prop;
}
PropertyInfo make_quat_property(const char* name, uint16_t offset, uint32_t flags, const char* hint) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Quat;
	prop.range_hint = hint;
	return prop;
}
PropertyInfo make_bool_property(const char* name, uint16_t offset, uint32_t flags, const char* hint) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Bool;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_integer_property(const char* name, uint16_t offset, uint32_t flags, int bytes, const char* hint,
								   const char* customtype) {
	PropertyInfo prop(name, offset, flags);

	if (bytes == 1)
		prop.type = core_type_id::Int8;
	else if (bytes == 2)
		prop.type = core_type_id::Int16;
	else if (bytes == 4)
		prop.type = core_type_id::Int32;
	else if (bytes == 8)
		prop.type = core_type_id::Int64;
	else
		assert(0);

	prop.range_hint = hint;
	prop.custom_type_str = customtype;
	return prop;
}

PropertyInfo make_float_property(const char* name, uint16_t offset, uint32_t flags, const char* hint) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Float;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_enum_property(const char* name, uint16_t offset, uint32_t flags, int bytes,
								const EnumTypeInfo* enumtype, const char* hint) {
	// ASSERT(enumtype->name);
	PropertyInfo prop(name, offset, flags);
	prop.range_hint = hint;
	if (bytes == 1)
		prop.type = core_type_id::Enum8;
	else if (bytes == 2)
		prop.type = core_type_id::Enum16;
	else if (bytes == 4)
		prop.type = core_type_id::Enum32;
	else
		assert(0);
	prop.enum_type = enumtype;
	return prop;
}

PropertyInfo make_string_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype) {
	PropertyInfo prop(name, offset, flags);
	prop.custom_type_str = customtype;
	prop.type = core_type_id::StdString;
	return prop;
}

PropertyInfo make_list_property(const char* name, uint16_t offset, uint32_t flags, IListCallback* ptr,
								const char* customtype) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::List;
	prop.list_ptr = ptr;
	prop.custom_type_str = customtype;
	return prop;
}

PropertyInfo make_struct_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype,
								  const char* hint) {
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Struct;
	prop.custom_type_str = customtype;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_new_struct_type(const char* name, uint16_t offset, int flags, const char* tooltip,
								  StructTypeInfo* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::ActualStruct;
	p.struct_type = type;
	return p;
}
PropertyInfo make_stringname_property(const char* name, uint16_t offset, int flags, const char* tooltip) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::StringName;
	return p;
}
PropertyInfo make_new_array_type(const char* name, uint16_t offset, int flags, const char* tooltip,
								 IListCallback* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::List;
	p.list_ptr = type;
	return p;
}

PropertyInfo make_assetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip,
										const ClassTypeInfo* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::AssetPtr;
	p.class_type = type;
	return p;
}
PropertyInfo make_objhandleptr_property(const char* name, uint16_t offset, int flags, const char* tooltip,
										const ClassTypeInfo* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::ObjHandlePtr;
	p.class_type = type;
	return p;
}

PropertyInfo make_classtypeinfo_property(const char* name, uint16_t offset, int flags, const char* tooltip,
										 const ClassTypeInfo* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::ClassTypeInfo;
	p.class_type = type;
	return p;
}

IListCallback::IListCallback(PropertyInfo atom_prop) {
	StaticList.count = 1;
	StaticProp = atom_prop;
	StaticList.list = &StaticProp;
	this->props_in_list = &StaticList;
	is_new_list_type = true;
}

const PropertyInfo* IListCallback::get_property() const {
	if (!is_new_list_type)
		return nullptr;
	return &StaticProp;
}

bool IListCallback::get_is_new_list_type() const {
	return is_new_list_type;
}

float PropertyInfo::get_float(const void* ptr) const {
	ASSERT(type == core_type_id::Float);

	return *(float*)get_ptr(ptr);
}

void PropertyInfo::set_float(void* ptr, float f) const {
	ASSERT(type == core_type_id::Float);

	*(float*)get_ptr(ptr) = f;
}

uint64_t PropertyInfo::get_int(const void* ptr) const {
	ASSERT(is_integral_type());
	uint8_t* p = get_ptr(ptr);
	if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		return *(int8_t*)p;
	} else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		return *(uint16_t*)p;
	} else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		return *(uint32_t*)p;
	} else if (type == core_type_id::Int64) {
		return *(uint64_t*)p;
	} else {
		ASSERT(0);
		return 0;
	}
}

void PropertyInfo::set_int(void* ptr, uint64_t i) const {
	ASSERT(is_integral_type());
	uint8_t* p = get_ptr(ptr);
	if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		*(int8_t*)p = i;
	} else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		*(uint16_t*)p = i;
	} else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		*(uint32_t*)p = i;
	} else if (type == core_type_id::Int64) {
		*(uint64_t*)p = i; // ERROR NARROWING
	} else {
		ASSERT(0);
	}
}
