#pragma once
#include <cstdint>
#include "EnumDefReflection.h"

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
	float default_ = 0.0;
	float min_val = -9999.f;
	float max_val = 9999.f;
	float step = 1.0;
	bool less = false;
	bool greater = false;
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
	union {
		const char* range_hint = "";
		IListCallback* list_ptr;
	};
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

PropertyInfo make_bool_property(const char* name, uint16_t offset, uint8_t flags, const char* hint = "");
PropertyInfo make_integer_property(const char* name, uint16_t offset, uint8_t flags, int bytes, const char* hint = "");
PropertyInfo make_float_property(const char* name, uint16_t offset, uint8_t flags, const char* hint = "");
PropertyInfo make_enum_property(const char* name, uint16_t offset, uint8_t flags, int bytes, int enum_type_id);
PropertyInfo make_string_property(const char* name, uint16_t offset, uint8_t flags);
PropertyInfo make_list_property(const char* name, uint16_t offset, uint8_t flags, IListCallback* ptr);


struct PropertyInfoList
{
	PropertyInfo* list = nullptr;
	int count = 0;

	PropertyInfo* find(const char* name) const;
};


class DictWriter;
class DictParser;
void write_properties(PropertyInfoList& list, void* ptr, DictWriter& out);
bool read_properties(PropertyInfoList& list, void* ptr, DictParser& in);

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