#include "ReflectionProp.h"
#include "DictParser.h"
#include "DictWriter.h"
#include "Util.h"
#include "GlobalEnumMgr.h"
#include <cassert>

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

PropertyInfo make_integer_property(const char* name, uint16_t offset, uint8_t flags, int bytes, const char* hint)
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

PropertyInfo make_list_property(const char* name, uint16_t offset, uint8_t flags, IListCallback* ptr)
{
	PropertyInfo prop(name,offset,flags);
	prop.type = core_type_id::List;
	prop.list_ptr = ptr;
	return prop;
}



void write_properties(PropertyInfoList& list, void* ptr, DictWriter& out)
{
	out.write_key_list_start("props");
	for (int i = 0; i < list.count; i++)
	{
		out.write_item_start();
		auto& prop = list.list[i];

		const char* value_str = nullptr;

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


		default:
			ASSERT(0);
			break;
		}

		if (value_str) {
			out.write_key_value("name", prop.name);
			out.write_key_value("type", GlobalEnumDefMgr::get().get_enum_name(core_type_id_def.id, (int)prop.type));
			out.write_key_value("value", value_str);
		}

		out.write_item_end();
	}
	out.write_list_end();
}



bool read_properties(PropertyInfoList& list, void* ptr, DictParser& in)
{
	StringView tok;

	in.expect_list_start();

	in.read_string(tok);
	while (!in.is_eof() && !in.check_list_end(tok))
	{
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
			while (!in.is_eof() && !in.check_item_end(tok)) {
				in.read_string(tok);
			}
			in.read_string(tok);
			continue;
		}
		
		in.read_string(tok);
		if (!tok.cmp("value"))
			return false;

		switch (prop->type)
		{

		case core_type_id::Bool:
		case core_type_id::Int8:
		case core_type_id::Int16: 
		case core_type_id::Int32: {
			int i = 0;
			in.read_int(i);
			prop->set_int(ptr, i);
		}break;

		case core_type_id::Float: {
			float f = 0.f;
			in.read_float(f);
			prop->set_float(ptr, f);
		}break;

		case core_type_id::StdString: {
			std::string* i = (std::string*)((char*)ptr + prop->offset);

			in.read_string(tok);
			*i = tok.to_stack_string().c_str();
		}break;

		case core_type_id::Enum8:
		case core_type_id::Enum16:
		case core_type_id::Enum32: {
			in.read_string(tok);
			Stack_String<256> str = tok.to_stack_string();
			auto enum_idx = GlobalEnumDefMgr::get().get_for_name(str.c_str());

			if (enum_idx.enum_idx < 0) {
				printf("\n\n!!! CANT FIND ENUM %s !!!\n\n", str.c_str());
				prop->set_int(ptr, 0);
			}
			else
				prop->set_int(ptr, enum_idx.val_idx);

		}break;

		default:
			ASSERT(0);
			break;
		}

		in.read_string(tok);
		if (!in.check_item_end(tok))
			return false;

		in.read_string(tok);
	}

	return true;
}

PropertyInfo* PropertyInfoList::find(const char* name) const
{
	for (int i = 0; i < count; i++) {
		if (strcmp(list[i].name, name) == 0)
			return list + i;
	}
	return nullptr;
}

