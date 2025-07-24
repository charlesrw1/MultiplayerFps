#include "Framework/ReflectionProp.h"
#include "Framework/DictParser.h"
#include "Framework/DictWriter.h"
#include "Framework/Util.h"
#include <cassert>
#include "Framework/ArrayReflection.h"
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>


inline std::string string_view_to_std_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}

static StringView delimit(const char* start, const char character = ',')
{
	StringView s;
	s.str_start = start;
	s.str_len = 0;
	while (*start != 0 && *start != character) {
		s.str_len++;
		start++;
	}
	return s;
}

PropertyInfo make_vec3_property(const char* name, uint16_t offset, uint32_t flags, const char* hint )
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Vec3;
	prop.range_hint = hint;
	return prop;
}
PropertyInfo make_quat_property(const char* name, uint16_t offset, uint32_t flags, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Quat;
	prop.range_hint = hint;
	return prop;
}
PropertyInfo make_bool_property(const char* name, uint16_t offset, uint32_t flags, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Bool;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_integer_property(const char* name, uint16_t offset, uint32_t flags, int bytes, const char* hint, const char* customtype)
{
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

PropertyInfo make_float_property(const char* name, uint16_t offset, uint32_t flags, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Float;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_enum_property(const char* name, uint16_t offset, uint32_t flags, int bytes, const EnumTypeInfo* enumtype, const char* hint)
{
	//ASSERT(enumtype->name);
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

PropertyInfo make_string_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype)
{
	PropertyInfo prop(name, offset, flags);
	prop.custom_type_str = customtype;
	prop.type = core_type_id::StdString;
	return prop;
}

PropertyInfo make_list_property(const char* name, uint16_t offset, uint32_t flags, IListCallback* ptr, const char* customtype)
{
	PropertyInfo prop(name,offset,flags);
	prop.type = core_type_id::List;
	prop.list_ptr = ptr;
	prop.custom_type_str = customtype;
	return prop;
}

PropertyInfo make_struct_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Struct;
	prop.custom_type_str = customtype;
	prop.range_hint = hint;
	return prop;
}



Factory<std::string, IPropertySerializer>& IPropertySerializer::get_factory()
{
	static Factory<std::string, IPropertySerializer> inst;
	return inst;
}
PropertyInfo make_new_struct_type(const char* name, uint16_t offset, int flags, const char* tooltip, StructTypeInfo* type)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::ActualStruct;
	p.struct_type = type;
	return p;
}
PropertyInfo make_stringname_property(const char* name, uint16_t offset, int flags, const char* tooltip)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::StringName;
	return p;
}
PropertyInfo make_new_array_type(const char* name, uint16_t offset, int flags, const char* tooltip, IListCallback* type)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::List;
	p.list_ptr = type;
	return p;
}

PropertyInfo make_assetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::AssetPtr;
	p.class_type = type;
	return p;
}
PropertyInfo make_softassetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::SoftAssetPtr;
	p.class_type = type;
	return p;
}
PropertyInfo make_objhandleptr_property(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type)
{
	PropertyInfo p(name, offset, flags);
	p.tooltip = tooltip;
	p.type = core_type_id::ObjHandlePtr;
	p.class_type = type;
	return p;
}

PropertyInfo make_classtypeinfo_property(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type)
{
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

bool IListCallback::get_is_new_list_type() const
{
	return is_new_list_type;
}



float PropertyInfo::get_float(const void* ptr) const
{
	ASSERT(type == core_type_id::Float);

	return *(float*)((char*)ptr + offset);
}

void PropertyInfo::set_float(void* ptr, float f) const
{
	ASSERT(type == core_type_id::Float);

	*(float*)((char*)ptr + offset) = f;
}

uint64_t PropertyInfo::get_int(const void* ptr) const
{
	ASSERT(is_integral_type());
	if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		return *(int8_t*)((char*)ptr + offset);
	}
	else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		return *(uint16_t*)((char*)ptr + offset);
	}
	else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		return *(uint32_t*)((char*)ptr + offset);
	}
	else if (type == core_type_id::Int64) {
		return *(uint64_t*)((char*)ptr + offset);
	}
	else {
		ASSERT(0);
		return 0;
	}
}

void PropertyInfo::set_int(void* ptr, uint64_t i) const
{
	ASSERT(is_integral_type());
	if (type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Enum8) {
		*(int8_t*)((char*)ptr + offset) = i;
	}
	else if (type == core_type_id::Int16 || type == core_type_id::Enum16) {
		*(uint16_t*)((char*)ptr + offset) = i;
	}
	else if (type == core_type_id::Int32 || type == core_type_id::Enum32) {
		*(uint32_t*)((char*)ptr + offset) = i;
	}
	else if (type == core_type_id::Int64) {
		*(uint64_t*)((char*)ptr + offset) = i;	// ERROR NARROWING
	}
	else {
		ASSERT(0);
	}
}

