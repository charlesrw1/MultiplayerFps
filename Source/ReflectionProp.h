#pragma once
#include <cstdint>
#include "EnumDefReflection.h"
#include "StringUtil.h"

// If you modify this, change the autoenumdef!!
extern AutoEnumDef core_type_id_def;
enum class core_type_id : uint8_t
{
	Bool,
	Int8,
	Int16,
	Int32,
	Enum8,
	Enum16,
	Enum32,
	Float,
	Struct,
	StdString,
	List,
};

enum SerializedPropFlags
{
	PROP_EDITABLE = 1,
	PROP_SERIALIZE = 2,

	PROP_DEFAULT = PROP_EDITABLE | PROP_SERIALIZE
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

class IListCallback;
struct PropertyInfoList;
struct PropertyInfo {
	PropertyInfo() {}
	PropertyInfo(const char* name, uint16_t offset, uint8_t flags) 
		: name(name), offset(offset), flags(flags) {}

	const char* name = "";
	uint16_t offset = 0;
	uint16_t enum_type_id = 0;
	core_type_id type = core_type_id::Int32;
	uint8_t flags = PROP_DEFAULT;
	const char* range_hint = "";
	IListCallback* list_ptr = nullptr;
	const char* custom_type_str = "";

	uint8_t* get_ptr(void* inst) {
		return (uint8_t*)inst + offset;
	}
	
	float get_float(void* ptr);
	void set_float(void* ptr, float f);

	int get_int(void* ptr);
	void set_int(void* ptr, int i);

	bool can_edit() const {
		return flags & PROP_EDITABLE;
	}
	bool can_serialize() const {
		return flags & PROP_SERIALIZE;
	}
	bool is_integral_type() const {
		return type == core_type_id::Bool || type == core_type_id::Int8 
			|| type == core_type_id::Int16 || type == core_type_id::Int32
			|| type == core_type_id::Enum8 || type == core_type_id::Enum16 || type == core_type_id::Enum32;
	}
};

ParsedHintStr parse_hint_str_for_property(PropertyInfo* prop);

PropertyInfo make_bool_property(const char* name, uint16_t offset, uint8_t flags, const char* hint = "");
PropertyInfo make_integer_property(const char* name, uint16_t offset, uint8_t flags, int bytes, const char* hint = "", const char* customtype = "");
PropertyInfo make_float_property(const char* name, uint16_t offset, uint8_t flags, const char* hint = "");
PropertyInfo make_enum_property(const char* name, uint16_t offset, uint8_t flags, int bytes, int enum_type_id, const char* hint= "");
PropertyInfo make_string_property(const char* name, uint16_t offset, uint8_t flags, const char* customtype = "");
PropertyInfo make_list_property(const char* name, uint16_t offset, uint8_t flags, IListCallback* ptr, const char* customtype = "");
PropertyInfo make_struct_property(const char* name, uint16_t offset, uint8_t flags, const char* customtype = "");


struct PropertyInfoList
{
	PropertyInfo* list = nullptr;
	int count = 0;
	const char* type_name = "Unknown";

	PropertyInfo* find(const char* name) const;
};


class DictWriter;
class DictParser;

struct Prop_Flag_Overrides;
void write_properties(PropertyInfoList& list, void* ptr, DictWriter& out, Prop_Flag_Overrides* overrides = nullptr);
bool read_properties(PropertyInfoList& list, void* ptr, DictParser& in, StringView first_token);

class IListCallback
{
public:
	IListCallback(PropertyInfoList* struct_) 
		: props_in_list(struct_) {}

	PropertyInfoList* props_in_list = nullptr;
	virtual uint8_t* get_index(void* inst, int index) = 0;
	virtual uint32_t get_size(void* inst) = 0;
	virtual void resize(void* inst, uint32_t new_size) = 0;
	virtual void swap_elements(void* inst, int item0, int item1) = 0;
};

struct PropertyListInstancePair
{
	PropertyInfoList* list = nullptr;
	void* instance = nullptr;
};