#pragma once
#include <cstdint>
#include "Framework/EnumDefReflection.h"
#include "Framework/StringUtil.h"
#include "Framework/Factory.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"

// If you modify this, change the autoenumdef!!
enum class core_type_id : uint8_t
{
	Bool,
	Int8,
	Int16,
	Int32,
	Int64,
	Enum8,
	Enum16,
	Enum32,
	Float,
	
	Vec2,
	Vec3,
	Quat,

	StdUniquePtr,	// unique_ptr<> to a property, type found in classname

	Struct,
	StdString,
	List,
	ActualStruct,	// not the fake struct...
	AssetPtr,
	SoftAssetPtr,
	ObjHandlePtr,
	ClassTypeInfo,
	StringName,	// hashed string

	//MulticastDelegate,
};


enum SerializedPropFlags
{
	// generic editable flag, if 0, then doesnt show up in property grid's
	PROP_EDITABLE = 1,
	// if 0, then not serialized
	PROP_SERIALIZE = 2,

	PROP_DEFAULT = PROP_EDITABLE | PROP_SERIALIZE,
	// Specific flags below:

	// if 1, then property is only editable/serialized with editor state
	PROP_EDITOR_ONLY = 4,
	// if 1, then instances of an entity in a level can edit it (if only editable, then only schemas can edit it)
	PROP_INSTANCE_EDITABLE = 8,

	// DONT USE
	PROP_IS_ENTITY_COMPONENT_TRANSFORM = 16,
	PROP_READ_ONLY_IF_NATIVE = 32,		// marks a prop as being read-only only if its from native component

};

struct ParsedHintStr
{
	bool has_default = false;
	bool has_min = false;
	bool has_max = false;
	float default_f = 0.0;
	int default_i = 0;
	float min_f = -9999.f;
	float max_f = 9999.f;
	int min_i = INT32_MIN;
	int max_i = INT32_MAX;
	float step_f = 0.1;
	int step_i = 1;
};

class DictWriter;
class DictParser;
struct PropertyInfo;

struct lua_State;

typedef std::string(*SerializePropFunc_t)(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user);
typedef void(*UnSerializePropFunc_t)(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user);

class StructTypeInfo;
struct multicast_funcs;
class IListCallback;
struct PropertyInfoList;
struct PropertyInfo {
	PropertyInfo() {}
	PropertyInfo(const char* name, uint16_t offset, uint32_t flags) 
		: name(name), offset(offset), flags(flags) {}

	const char* name = "";
	uint16_t offset = 0;
	core_type_id type = core_type_id::Int32;
	uint32_t flags = PROP_DEFAULT;
	const char* range_hint = "";
	IListCallback* list_ptr = nullptr;
	const char* custom_type_str = "";
	const char* tooltip = "";
	const EnumTypeInfo* enum_type = nullptr;
	const StructTypeInfo* struct_type = nullptr;
	const ClassTypeInfo* class_type = nullptr;	// used for objtype,assetptr,classtype

	//SerializePropFunc_t serialize_func = nullptr;
	//UnSerializePropFunc_t unserialize_func = nullptr;
	//int(*call_function)(lua_State* L) = nullptr;
	//const multicast_funcs* multicast = nullptr;

	uint8_t* get_ptr(const void* inst) const {
		return (uint8_t*)inst + offset;
	}
	
	float get_float(const void* ptr) const;
	void set_float(void* ptr, float f) const;

	uint64_t get_int(const void* ptr) const;
	void set_int(void* ptr, uint64_t i) const;

	bool can_edit() const {
		return flags & PROP_EDITABLE;
	}
	bool can_serialize() const {
		return flags & PROP_SERIALIZE;
	}
	bool is_integral_type() const {
		return type == core_type_id::Bool || type == core_type_id::Int8 
			|| type == core_type_id::Int16 || type == core_type_id::Int32 || type == core_type_id::Int64
			|| type == core_type_id::Enum8 || type == core_type_id::Enum16 || type == core_type_id::Enum32;
	}
};
struct FunctionInfo {
	const char* name = "";
	bool is_static = false;
	bool is_virtual = false;
	int(*lua_c_function)(lua_State* state) = nullptr;
};


ParsedHintStr parse_hint_str_for_property(PropertyInfo* prop);
PropertyInfo make_bool_property(const char* name, uint16_t offset, uint32_t flags, const char* hint = "");
PropertyInfo make_integer_property(const char* name, uint16_t offset, uint32_t flags, int bytes, const char* hint = "", const char* customtype = "");
PropertyInfo make_float_property(const char* name, uint16_t offset, uint32_t flags, const char* hint = "");
PropertyInfo make_enum_property(const char* name, uint16_t offset, uint32_t flags, int bytes, const EnumTypeInfo* t, const char* hint= "");
PropertyInfo make_string_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype = "");
PropertyInfo make_list_property(const char* name, uint16_t offset, uint32_t flags, IListCallback* ptr, const char* customtype = "");
PropertyInfo make_struct_property(const char* name, uint16_t offset, uint32_t flags, const char* customtype = "", const char* hint = "");
PropertyInfo make_vec3_property(const char* name, uint16_t offset, uint32_t flags, const char* hint = "");
PropertyInfo make_quat_property(const char* name, uint16_t offset, uint32_t flags, const char* hint = "");

PropertyInfo make_stringname_property(const char* name, uint16_t offset, int flags, const char* tooltip);
PropertyInfo make_new_array_type(const char* name, uint16_t offset, int flags, const char* tooltip, IListCallback* type);
PropertyInfo make_assetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type);
PropertyInfo make_objhandleptr_property(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type);
PropertyInfo make_classtypeinfo_property(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type);
PropertyInfo make_softassetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip, const ClassTypeInfo* type);

FunctionInfo make_lua_function_info(const char* name, bool is_static, bool is_virtual, int(*ptr)(lua_State*));

struct PropertyInfoList
{
	PropertyInfo* list = nullptr;
	int count = 0;
	const char* type_name = "Unknown";

	PropertyInfo* find(const char* name) const;
};


class DictWriter;
class DictParser;
struct PropertyListInstancePair
{
	const PropertyInfoList* list = nullptr;
	void* instance = nullptr;
};

struct Prop_Flag_Overrides;
class IAssetLoadingInterface;
class BinaryReader;
class FileWriter;



class IListCallback
{
public:
	IListCallback(const PropertyInfoList* struct_) 
		: props_in_list(struct_) {}
	IListCallback(PropertyInfo atom_prop);
	const PropertyInfoList* props_in_list = nullptr;
	const PropertyInfo* get_property() const;
	bool get_is_new_list_type() const;
	virtual uint8_t* get_index(void* inst, int index) = 0;
	virtual int get_size(void* inst) = 0;
	virtual void resize(void* inst, int new_size) = 0;
	virtual void swap_elements(void* inst, int item0, int item1) = 0;
private:
	bool is_new_list_type = false;
	// for atom types
	PropertyInfo StaticProp;
	PropertyInfoList StaticList;
};