#include "Framework/ReflectionProp.h"
#include "Framework/ClassBase.h"
#include "Framework/DictParser.h"
#include "Framework/DictWriter.h"
#include "Framework/Util.h"
#include <cassert>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

uint8_t* PropertyInfo::get_ptr(const void* inst) const {
	ASSERT(!(flags & (PROP_SCRIPT_FIELD | PROP_LUA_BACKED)) && "the field is not in the object; use get_value/set_value");
	return (uint8_t*)inst + offset;
}

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

template <typename T> void store(uint8_t* ptr, T v) {
	*reinterpret_cast<T*>(ptr) = v;
}

void storage_get(const PropertyInfo& p, const void* inst, PropValue& out) {
	read_storage(p, p.get_ptr(inst), out);
}
void storage_set(const PropertyInfo& p, void* inst, const PropValue& in, SetMode) {
	write_storage(p, p.get_ptr(inst), in);
}
const PropertyAccess storage_access{storage_get, storage_set};
} // namespace

void read_storage(const PropertyInfo& p, const uint8_t* ptr, PropValue& out) {
	ASSERT(ptr);
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

void write_storage(const PropertyInfo& p, uint8_t* ptr, const PropValue& in) {
	ASSERT(ptr);
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
	case core_type_id::StringName:
		*reinterpret_cast<StringName*>(ptr) = StringName(prop_value_get<std::string>(in).c_str());
		break;
	case core_type_id::AssetPtr: store(ptr, prop_value_get<AssetRef>(in).asset); break;
	case core_type_id::ObjHandlePtr: store(ptr, prop_value_get<EntityRef>(in).handle); break;
	case core_type_id::ClassTypeInfo: store(ptr, prop_value_get<const ClassTypeInfo*>(in)); break;
	default: ASSERT(!"the property has no value");
	}
}

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


PropertyInfo make_assetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip,
										const ClassTypeInfo* type) {
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::AssetPtr;
	p.class_type = type;
	return p;
}

void PropertyInfo::with_struct(void* inst, StructFn fn) const {
	ASSERT(inst && type == core_type_id::ActualStruct && struct_type);
	ASSERT(owns(*this, inst) && "the object is not of the property's class");
	if (access) {
		ASSERT(access->with_struct);
		access->with_struct(*this, inst, fn);
	} else {
		fn(StructRef{struct_type, get_ptr(inst)});
	}
}

PropertyInfo* PropertyInfoList::find(const char* name) const {
	ASSERT(name);
	for (int i = 0; i < count; i++)
		if (std::strcmp(list[i].name, name) == 0)
			return &list[i];
	return nullptr;
}
