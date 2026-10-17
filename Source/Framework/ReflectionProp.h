#pragma once
#include <cstdint>
#include <cfloat>
#include <string>
#include <string_view>
#include "Framework/EnumDefReflection.h"
#include "Framework/StringUtil.h"
#include "Framework/Factory.h"
#include "Framework/DictParser.h"
#include "Framework/ClassBase.h"
#include "Framework/PropertyValue.h"

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

	StdUniquePtr, // unique_ptr<> to a property, type found in classname

	Struct,
	StdString,
	List,
	ActualStruct, // not the fake struct...
	AssetPtr,
	ObjHandlePtr,
	ClassTypeInfo,
	StringName, // hashed string

	// LuaSharp script fields (PROP_SCRIPT_FIELD)
	Double,
	ScriptString, // a GC string; only through get_value/set_value

	// MulticastDelegate,
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
	PROP_READ_ONLY_IF_NATIVE = 32, // marks a prop as being read-only only if its from native component

	// Storage is in the owning ClassBase's lua_field_shadow buffer (offset is from its start). Only the accessor
	// (LuaClassTypeInfo) reaches it; get_ptr asserts. Set on the fields of Lua-defined Component subclasses.
	PROP_LUA_BACKED = 64,

	// Storage is in the LuaSharp script object of the owning ClassBase (offset is from its start). Only the
	// accessor (LuaSharpEngine) reaches it; get_ptr asserts. Set on the [Editable] fields of LuaSharp classes.
	PROP_SCRIPT_FIELD = 128,
};


// Structured per-property attributes populated by codegen from REFLECT(...) kwargs.
// Unset means "no constraint": min/max use sentinel +/-FLT_MAX, step==0 disables stepping.
// Strings are owned by static codegen output, never heap-allocated.
// @docs [[reflection_macros]]
struct PropertyAttributes
{
	float min  = -FLT_MAX;
	float max  =  FLT_MAX;
	float step = 0.f;
	const char* category = nullptr;
	bool hidden   = false;
	bool readonly = false;

	bool has_min()  const { return min  != -FLT_MAX; }
	bool has_max()  const { return max  !=  FLT_MAX; }
	bool has_step() const { return step != 0.f; }
};

class DictWriter;
class DictParser;
struct PropertyInfo;

struct lua_State;

// PropertyInfo::offset is 16 bits. Generated code computes offsets through this, so a field past 64 KB is a compile error.
template <size_t Offset> constexpr uint16_t checked_offset() {
	static_assert(Offset <= UINT16_MAX, "reflected field offset does not fit in PropertyInfo::offset");
	return static_cast<uint16_t>(Offset);
}

class StructTypeInfo;
struct multicast_funcs;
struct PropertyInfoList;
struct PropertyInfo
{
	PropertyInfo() {}
	PropertyInfo(const char* name, uint16_t offset, uint32_t flags) : name(name), offset(offset), flags(flags) {}

	const char* name = "";
	uint16_t offset = 0;
	core_type_id type = core_type_id::Int32;
	uint32_t flags = PROP_DEFAULT;
	const char* range_hint = "";
	const ListAccess* list_access = nullptr; // lists only
	const char* custom_type_str = "";
	const char* tooltip = "";
	const EnumTypeInfo* enum_type = nullptr;
	const StructTypeInfo* struct_type = nullptr;
	const ClassTypeInfo* class_type = nullptr; // used for objtype,assetptr,classtype
	PropertyAttributes attrs;
	// Reads and writes the value. Null: the storage at get_ptr(), by type.
	const PropertyAccess* access = nullptr;
	// The class whose objects hold this property; set by the class type. Null for struct members and list elements.
	const ClassTypeInfo* owner = nullptr;
	// The previous name (REFLECT(former=...), LuaSharp [Former("...")]): readers use it when name is missing.
	const char* former_name = nullptr;

	// The storage of a C++ field, for code that has not moved to values. Asserts for script fields.
	uint8_t* get_ptr(const void* inst) const;

	// False for lists and structs: they have no single value.
	bool has_value() const;
	void get_value(const void* inst, PropValue& out) const;
	PropValue get_value(const void* inst) const;
	// Clamps numbers to attrs.min/max.
	void set_value(void* inst, const PropValue& in, SetMode mode = SetMode::Edit) const;
	// T is a domain type or a C++ type that converts to one (see prop_value_as).
	template <typename T> T get(const void* inst) const { return prop_value_as<T>(get_value(inst)); }
	template <typename T> void set(void* inst, const T& value, SetMode mode = SetMode::Edit) const {
		if constexpr (std::is_same_v<T, IAsset*>)
			set_value(inst, AssetRef{value, class_type}, mode);
		else
			set_value(inst, to_prop_value(value), mode);
	}
	// Struct properties: calls fn with the struct, which is valid only during the call.
	void with_struct(void* inst, StructFn fn) const;
	// List properties.
	const ListAccess& list() const {
		ASSERT(type == core_type_id::List && list_access);
		return *list_access;
	}

	bool can_edit() const { return flags & PROP_EDITABLE; }
	bool can_serialize() const { return flags & PROP_SERIALIZE; }
	bool is_integral_type() const {
		return type == core_type_id::Bool || type == core_type_id::Int8 || type == core_type_id::Int16 ||
			   type == core_type_id::Int32 || type == core_type_id::Int64 || type == core_type_id::Enum8 ||
			   type == core_type_id::Enum16 || type == core_type_id::Enum32;
	}
};
struct FunctionInfo
{
	const char* name = "";
	bool is_static = false;
	bool is_virtual = false;
	int (*lua_c_function)(lua_State* state) = nullptr;
};

// The value at ptr, by p.type: the default accessor, and accessors over raw buffers (the Lua shadow buffer).
void read_storage(const PropertyInfo& p, const uint8_t* ptr, PropValue& out);
void write_storage(const PropertyInfo& p, uint8_t* ptr, const PropValue& in);

// A property with no storage of its own, for editors that keep the value elsewhere (add_iproped_manual).
PropertyInfo make_assetptr_property_new(const char* name, uint16_t offset, int flags, const char* tooltip,
										const ClassTypeInfo* type);

struct PropertyInfoList
{
	PropertyInfo* list = nullptr;
	int count = 0;
	const char* type_name = "Unknown";

	PropertyInfo* find(const char* name) const;
};
