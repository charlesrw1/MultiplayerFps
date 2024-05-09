#include "ReflectionProp.h"
#include "DictParser.h"
#include "DictWriter.h"
#include "Util.h"
#include <cassert>
#include "StdVectorReflection.h"

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

static void parse_str(StringView str, float& f, int& i, core_type_id type, bool is_min_max_or_inc) {
	if (str.cmp("true") || str.cmp("True")) {
		ASSERT(type == core_type_id::Bool);
		ASSERT(!is_min_max_or_inc);
		i = 1;
	}
	else if (str.cmp("false") || str.cmp("False")) {
		i = 0;
		ASSERT(!is_min_max_or_inc);
		ASSERT(type == core_type_id::Bool);
	}
	else {
		if (type == core_type_id::Enum8 || type == core_type_id::Enum16 || type == core_type_id::Enum32) {
			auto idx = Enum::find_for_full_name(str.to_stack_string().c_str());
			ASSERT(idx.enum_idx != -1 && idx.val_idx != -1);
			i = idx.val_idx;
			ASSERT(!is_min_max_or_inc);
		}
		else if (type == core_type_id::Int8 || type == core_type_id::Int16 || type == core_type_id::Int32 || type == core_type_id::Bool) {
			i = atoi(str.to_stack_string().c_str());
		}
		else if (type == core_type_id::Float) {
			f = atof(str.to_stack_string().c_str());
		}
		else
			ASSERT(!"no parse hint str allowed for non-numbers/enums");	// no hint strs
	}
}

ParsedHintStr parse_hint_str_for_property(PropertyInfo* prop)
{
	ParsedHintStr parsed;
	const char* start = prop->range_hint;
	StringView sv = delimit(start);
	if (sv.str_len > 0) {
		parse_str(sv, parsed.default_f, parsed.default_i, prop->type, false);
		parsed.has_default = true;
	}
	const char* next = sv.str_start + sv.str_len;
	if (*next == 0)
		return parsed;
	ASSERT(*next == ',');
	next++;
	sv = delimit(next);
	if (sv.str_len > 0) {
		parse_str(sv, parsed.min_f, parsed.min_i, prop->type, true);
		parsed.has_min = true;

	}

	next = sv.str_start + sv.str_len;
	if (*next == 0)
		return parsed;
	ASSERT(*next == ',');
	next++;
	sv = delimit(next);
	if (sv.str_len > 0) {
		parse_str(sv, parsed.max_f, parsed.max_i, prop->type, true);
		parsed.has_max = true;

	}

	next = sv.str_start + sv.str_len;
	if (*next == 0)
		return parsed;
	ASSERT(*next == ',');
	next++;
	sv = delimit(next);
	if (sv.str_len > 0) {
		parse_str(sv, parsed.step_f, parsed.step_i, prop->type, true);
	}

	return parsed;
}

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

PropertyInfo make_enum_property(const char* name, uint16_t offset, uint8_t flags, int bytes, int enum_type_id, const char* hint)
{
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

void write_list(PropertyInfo* prop, void* ptr, DictWriter& out, TypedVoidPtr userptr);
std::string write_field_type(core_type_id type, void* ptr, PropertyInfo& prop, DictWriter& out, TypedVoidPtr userptr) {
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
		const char* type_name = Enum::get_type_name(prop.enum_type_id);
		const char* enum_str = Enum::get_enum_name(prop.enum_type_id, prop.get_int(ptr));
		value_str = string_format("%s::%s", type_name, enum_str);
	}break;

	case core_type_id::StdString: {
		std::string* i = (std::string*)((char*)ptr + prop.offset);
		value_str = i->c_str();
	}break;

	case core_type_id::List: {

		write_list(&prop, prop.get_ptr(ptr), out, userptr);

	}break;

	case core_type_id::Struct: {

		auto& fac = get_property_serializer_factory();
		auto serializer = fac.createObject(prop.custom_type_str);
		if (serializer) {
			auto str = serializer->serialize(out, prop, ptr, userptr);
			delete serializer;	// fixme: inplace new/free instead?
			return str;
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop.custom_type_str);
			return "";
		}

	}break;
	}
	return value_str;
}

void write_list(PropertyInfo* listprop, void* ptr, DictWriter& out, TypedVoidPtr userptr)
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

			buf += write_field_type(prop.type, member_dat, prop, out, userptr);
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

			out.write_item_start();
			write_properties(*list_ptr->props_in_list, list_ptr->get_index(ptr, i), out, userptr);
			out.write_item_end();
		}
		out.write_list_end();
	}
}


void write_properties(PropertyInfoList& list, void* ptr, DictWriter& out, TypedVoidPtr userptr)
{
	for (int i = 0; i < list.count; i++)
	{
		auto& prop = list.list[i];

		uint32_t flags = prop.flags;
		

		if (!(flags & PROP_SERIALIZE))
			continue;
		
		out.write_value(string_format("%s ", prop.name));
		std::string value_str = write_field_type(prop.type, ptr, prop, out, userptr);
		if (!value_str.empty())
			out.write_value(value_str.c_str());
	}
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok, TypedVoidPtr userptr);
bool read_list_field(PropertyInfo* prop, void* listptr, DictParser& in, StringView tok, TypedVoidPtr userptr)
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

			if (!read_propety_field(prop, listcallback->get_index(listptr, count), in, tok, userptr))
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

			in.read_string(tok);


			if (!read_properties(*listcallback->props_in_list, listcallback->get_index(listptr, count), in, tok, userptr).second)
				return false;

			count++;

			in.read_string(tok);
		}
	}
	return true;
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok, TypedVoidPtr userptr)
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
		auto enum_idx = Enum::find_for_full_name(str.c_str());

		if (enum_idx.enum_idx < 0) {
			printf("\n\n!!! CANT FIND ENUM %s !!!\n\n", str.c_str());
			prop->set_int(ptr, 0);
		}
		else
			prop->set_int(ptr, enum_idx.val_idx);

	}break;

	case core_type_id::List:
		return read_list_field(prop, prop->get_ptr(ptr), in, tok, userptr);

	case core_type_id::Struct: {

		auto& fac = get_property_serializer_factory();
		auto serializer = fac.createObject(prop->custom_type_str);
		if (serializer) {
			serializer->unserialize(in, *prop, ptr, tok, userptr);
			delete serializer;	// fixme: inplace new/free instead?
			return true;
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop->custom_type_str);
			return "";
		}

	}break;

	default:
		ASSERT(0);
		break;
	}

	return true;
}


struct FindInst
{
	PropertyInfo* prop = nullptr;
	void* instptr = nullptr;
};

FindInst find_in_proplists(const char* name, std::vector<PropertyListInstancePair>& proplists)
{
	for (auto& prop : proplists) {
		auto val = prop.list->find(name);
		if (val)
			return { val,prop.instance };
	}
	return { nullptr,nullptr };
}

std::pair<StringView, bool> read_multi_properties(std::vector<PropertyListInstancePair>& proplists, DictParser& in, StringView tok, TypedVoidPtr userptr)
{
	// expect { (start field list)
	if (!in.check_item_start(tok))
		return { tok, false };

	in.read_string(tok);
	while (!in.is_eof() && !in.check_item_end(tok))	// exit out if } (end field list)
	{
		// expect { (start property field)
		if (!in.check_item_start(tok))
			return { tok, false };

		in.read_string(tok);
		if (!tok.cmp("name"))
			return { tok, false };
		in.read_string(tok);
		auto name = tok.to_stack_string();
		auto find = find_in_proplists(name.c_str(), proplists);

		if (!find.prop) {
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
			return { tok, false };

		in.read_string(tok);
		if (!read_propety_field(find.prop, find.instptr, in, tok, userptr))
			return { tok, false };


		in.read_string(tok);
		if (!in.check_item_end(tok))
			return { tok, false };

		in.read_string(tok);
	}

	return { tok, true };
}

std::pair<StringView, bool> read_properties(PropertyInfoList& list, void* ptr, DictParser& in, StringView tok, TypedVoidPtr userptr)
{
	std::vector<PropertyListInstancePair> props(1);
	props[0] = { &list,ptr };
	return read_multi_properties(props, in, tok, userptr);
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

Factory<std::string, IPropertySerializer>& get_property_serializer_factory()
{
	static Factory<std::string, IPropertySerializer> inst;
	return inst;
}