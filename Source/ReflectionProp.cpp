#include "ReflectionProp.h"
#include "DictParser.h"
#include "DictWriter.h"
#include "Util.h"
#include "GlobalEnumMgr.h"
#include <cassert>


inline std::string string_view_to_std_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}


static const char* core_type_id_strs[] = {
	"Bool",
	"Int8",
	"Int16",
	"Int32",
	"Enum8",
	"Enum16",
	"Enum32",
	"Float",
	"Struct",
	"StdString",
	"List"
};

static_assert((sizeof(core_type_id_strs) / sizeof(char*)) == ((int)core_type_id::List + 1), "out of sync");
AutoEnumDef core_type_id_def = AutoEnumDef("", sizeof(core_type_id_strs) / sizeof(char*), core_type_id_strs);



PropertyInfo make_bool_property(const char* name, uint16_t offset, uint8_t flags, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Bool;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_integer_property(const char* name, uint16_t offset, uint8_t flags, int bytes, const char* hint, const char* customtype)
{
	PropertyInfo prop(name, offset, flags);

	if (bytes == 1)
		prop.type = core_type_id::Int8;
	else if (bytes == 2)
		prop.type = core_type_id::Int16;
	else if (bytes == 4)
		prop.type = core_type_id::Int32;
	else
		assert(0);

	prop.range_hint = hint;
	prop.custom_type_str = customtype;
	return prop;
}

PropertyInfo make_float_property(const char* name, uint16_t offset, uint8_t flags, const char* hint)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Float;
	prop.range_hint = hint;
	return prop;
}

PropertyInfo make_enum_property(const char* name, uint16_t offset, uint8_t flags, int bytes, int enum_type_id)
{
	PropertyInfo prop(name, offset, flags);
	if (bytes == 1)
		prop.type = core_type_id::Enum8;
	else if (bytes == 2)
		prop.type = core_type_id::Enum16;
	else if (bytes == 4)
		prop.type = core_type_id::Enum32;
	else
		assert(0);
	prop.enum_type_id = enum_type_id;
	return prop;
}

PropertyInfo make_string_property(const char* name, uint16_t offset, uint8_t flags, const char* customtype)
{
	PropertyInfo prop(name, offset, flags);
	prop.custom_type_str = customtype;
	prop.type = core_type_id::StdString;
	return prop;
}

PropertyInfo make_list_property(const char* name, uint16_t offset, uint8_t flags, IListCallback* ptr, const char* customtype)
{
	PropertyInfo prop(name,offset,flags);
	prop.type = core_type_id::List;
	prop.list_ptr = ptr;
	prop.custom_type_str = customtype;
	return prop;
}

PropertyInfo make_struct_property(const char* name, uint16_t offset, uint8_t flags, const char* customtype)
{
	PropertyInfo prop(name, offset, flags);
	prop.type = core_type_id::Struct;
	prop.custom_type_str = customtype;
	return prop;
}

void write_list(PropertyInfo* prop, void* ptr, DictWriter& out);
std::string write_field_type(core_type_id type, void* ptr, PropertyInfo& prop, DictWriter& out) {
	std::string value_str;

	switch (prop.type)
	{
	case core_type_id::Bool:
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
		value_str = string_format("%d", prop.get_int(ptr));
		break;

	case core_type_id::Float:
		value_str = string_format("%f", prop.get_float(ptr));
		break;

	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32: {
		const char* type_name = GlobalEnumDefMgr::get().get_enum_type_name(prop.enum_type_id);
		const char* enum_str = GlobalEnumDefMgr::get().get_enum_name(prop.enum_type_id, prop.get_int(ptr));
		value_str = string_format("%s::%s", type_name, enum_str);
	}break;

	case core_type_id::StdString: {
		std::string* i = (std::string*)((char*)ptr + prop.offset);
		value_str = i->c_str();
	}break;

	case core_type_id::List: {

		write_list(&prop, prop.get_ptr(ptr), out);

	}break;
	}
	return value_str;
}

void write_list(PropertyInfo* listprop, void* ptr, DictWriter& out)
{
	out.write_list_start();

	// special case for atom types
	auto list_ptr = listprop->list_ptr;
	ASSERT(list_ptr);
	bool is_atom_type =
		(list_ptr->props_in_list->count == 1
			&& strcmp(list_ptr->props_in_list->list[0].name, "_value") == 0);
	if (is_atom_type) {

		 auto& prop = list_ptr->props_in_list->list[0];
		std::string buf;
		int count = list_ptr->get_size(ptr);
		for (int i = 0; i < count; i++) {

			uint8_t* member_dat = list_ptr->get_index(ptr, i);

			buf += write_field_type(prop.type, member_dat, prop, out);
			buf += ' ';
			out.write_value(buf.c_str());
			buf.clear();
		}
		out.write_list_end();
	}
	else {

		int count = list_ptr->get_size(ptr);
		for (int i = 0; i < count; i++) {

			uint8_t* member_dat = list_ptr->get_index(ptr, i);

			write_properties(*list_ptr->props_in_list, list_ptr->get_index(ptr, i), out);
		}
		out.write_list_end();
	}
}


void write_properties(PropertyInfoList& list, void* ptr, DictWriter& out)
{
	out.write_item_start();
	for (int i = 0; i < list.count; i++)
	{
		out.write_item_start();
		auto& prop = list.list[i];

		std::string value_str = write_field_type(prop.type, ptr, prop, out);

		if (!value_str.empty()) {
			out.write_key_value("name", prop.name);
			out.write_key_value("type", GlobalEnumDefMgr::get().get_enum_name(core_type_id_def.id, (int)prop.type));
			out.write_key_value("value", value_str.c_str());
		}

		out.write_item_end();
	}
	out.write_item_end();
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok);
bool read_list_field(PropertyInfo* prop, void* listptr, DictParser& in, StringView tok)
{
	if (!in.check_list_start(tok))
		return false;

	auto listcallback = prop->list_ptr;
	bool is_atom_type =
		(listcallback->props_in_list->count == 1
			&& strcmp(listcallback->props_in_list->list[0].name, "_value") == 0);
	if (is_atom_type) {
		in.read_string(tok);
		int count = 0;
		while (!in.is_eof() && !in.check_list_end(tok)) {

			listcallback->resize(listptr, count + 1);

			if (!read_propety_field(prop, listcallback->get_index(listptr, count), in, tok))
				return false;

			count++;

			in.read_string(tok);
		}
	}
	else {
		in.read_string(tok);
		int count = 0;
		while (!in.is_eof() && !in.check_list_end(tok)) {

			listcallback->resize(listptr, count + 1);

			if (!read_properties(*listcallback->props_in_list, listcallback->get_index(listptr, count), in, tok))
				return false;

			count++;

			in.read_string(tok);
		}
	}
	return true;
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok)
{

	switch (prop->type)
	{

	case core_type_id::Bool:
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32: {
		int i = atoi(tok.to_stack_string().c_str());
		prop->set_int(ptr, i);
	}break;

	case core_type_id::Float: {
		float f = atof(tok.to_stack_string().c_str());
		prop->set_float(ptr, f);
	}break;

	case core_type_id::StdString: {
		std::string* i = (std::string*)((char*)ptr + prop->offset);

		*i = string_view_to_std_string(tok);
	}break;

	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32: {
		Stack_String<256> str = tok.to_stack_string();
		auto enum_idx = GlobalEnumDefMgr::get().get_for_name(str.c_str());

		if (enum_idx.enum_idx < 0) {
			printf("\n\n!!! CANT FIND ENUM %s !!!\n\n", str.c_str());
			prop->set_int(ptr, 0);
		}
		else
			prop->set_int(ptr, enum_idx.val_idx);

	}break;

	case core_type_id::List:
		return read_list_field(prop, prop->get_ptr(ptr), in, tok);

	default:
		ASSERT(0);
		break;
	}

	return true;
}


bool read_properties(PropertyInfoList& list, void* ptr, DictParser& in, StringView tok)
{

	// expect { (start field list)
	if (!in.check_item_start(tok))
		return false;

	in.read_string(tok);
	while (!in.is_eof() && !in.check_item_end(tok))	// exit out if } (end field list)
	{
		// expect { (start property field)
		if (!in.check_item_start(tok))
			return false;

		in.read_string(tok);
		if (!tok.cmp("name"))
			return false;
		in.read_string(tok);
		auto name = tok.to_stack_string();
		auto prop = list.find(name.c_str());

		if (!prop) {
			printf("\n\n!!! COULDN'T FIND PARAM %s !!!\n\n", name.c_str());

			// skip property field
			while (!in.is_eof() && !in.check_item_end(tok)) {
				in.read_string(tok);
			}
			in.read_string(tok);
			continue;
		}
		
		in.read_string(tok);
		if (!tok.cmp("value"))
			return false;

		in.read_string(tok);
		if (!read_propety_field(prop, ptr, in, tok))
			return false;


		in.read_string(tok);
		if (!in.check_item_end(tok))
			return false;

		in.read_string(tok);
	}

	return true;
}

PropertyInfo* PropertyInfoList::find(const char* name) const
{
	// currently a linear serach, so n^2 for reads, this could become a hash table if nessecary
	for (int i = 0; i < count; i++) {
		if (strcmp(list[i].name, name) == 0)
			return list + i;
	}
	return nullptr;
}

#include "StdVectorReflection.h"

template<>
PropertyInfoList* get_list_value<uint32_t>() {
	static PropertyInfo info[] = {
		make_integer_property("_value",0/* 0 offset */,PROP_DEFAULT,sizeof(uint32_t))
	};
	static PropertyInfoList list = { info,1,"_uint32" };
	return &list;

}
template<>
PropertyInfoList* get_list_value<uint16_t>() {
	static PropertyInfo info[] = {
		make_integer_property("_value",0/* 0 offset */,PROP_DEFAULT,sizeof(uint16_t))
	};
	static PropertyInfoList list = { info,1,"_uint16" };
	return &list;

}
template<>
PropertyInfoList* get_list_value<int32_t>() {
	static PropertyInfo info[] = {
		make_integer_property("_value",0/* 0 offset */,PROP_DEFAULT,sizeof(int32_t))
	};
	static PropertyInfoList list = { info,1,"_int32" };
	return &list;

}
template<>
PropertyInfoList* get_list_value<float>() {
	static PropertyInfo info[] = {
		make_float_property("_value",0/* 0 offset */,PROP_DEFAULT)
	};
	static PropertyInfoList list = { info,1,"_float" };
	return &list;
}