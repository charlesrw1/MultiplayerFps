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
			auto idx = EnumRegistry::find_enum_by_name(str.to_stack_string().c_str()).enum_idx;
			ASSERT(idx != -1);
			i = idx;
			ASSERT(!is_min_max_or_inc);
		}
		else if (type == core_type_id::Int8 || type == core_type_id::Int16 || type == core_type_id::Int32 || type == core_type_id::Int64|| type == core_type_id::Bool) {
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
	ASSERT(enumtype->name);
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
#include "ObjectSerialization.h"
void write_unique_ptr(PropertyInfo* listprop, void* ptr, DictWriter& out, ClassBase* userptr)
{
	write_object_properties(*(ClassBase**)ptr, userptr, out);
}

void write_list(PropertyInfo* prop, void* ptr, const void* diff_ptr, DictWriter& out, ClassBase* userptr);
std::pair<std::string,bool> write_field_type(bool write_name, core_type_id type, void* ptr, const void* diff_ptr, PropertyInfo& prop, DictWriter& out, ClassBase* userptr) {
	std::string value_str;

	switch (prop.type)
	{
	case core_type_id::Bool:
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:

		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return { {}, false };
		}
		value_str = string_format("%lld", prop.get_int(ptr));
		break;

	case core_type_id::Float:
		if (diff_ptr) {
			float mine = prop.get_float(ptr);
			float other = prop.get_float(diff_ptr);
			if (std::abs(mine-other)<0.00001)	// skip
				return { {}, false };
		}

		value_str = string_format("%f", prop.get_float(ptr));
		break;

	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32: {
		ASSERT(prop.enum_type);
		auto v = prop.enum_type->find_for_value(prop.get_int(ptr));
		if(v)
			value_str = string_format("%s::%s",prop.enum_type->name, v->name);
	}break;

	case core_type_id::StdString: {
		if (diff_ptr) {
			std::string* mine = (std::string*)prop.get_ptr(ptr);
			std::string* other = (std::string*)prop.get_ptr(diff_ptr);
			if (*mine==*other)	// skip
				return { {}, false };
		}

		std::string* i = (std::string*)((char*)ptr + prop.offset);
		value_str = i->c_str();

		// validate:
		for (auto& c : value_str) {
			if (c == '"') {
				printf("!!! dictparser attempted write '\"'\n");
				c = ' ';
			}
		}
	}break;

	case core_type_id::List: {

		const void* diff = (diff_ptr) ? prop.get_ptr(diff_ptr) : nullptr;

		if (diff) {
			int diffsize = prop.list_ptr->get_size((void*)diff);
			int mysize = prop.list_ptr->get_size(prop.get_ptr(ptr));
			if (diffsize == 0 && mysize == 0)
				return { {},false };	// skip
		}

		if(write_name)
			out.write_key(prop.name);
		write_list(&prop, prop.get_ptr(ptr), diff, out, userptr);

		return std::pair<std::string,bool>{ {},false };
	}break;
	case core_type_id::StdUniquePtr: {

		if (write_name)
			out.write_key(prop.name);
		write_unique_ptr(&prop, prop.get_ptr(ptr), out, userptr);

		return std::pair<std::string, bool>{ {}, false };
	}

	case core_type_id::Struct: {

		auto& fac = IPropertySerializer::get_factory();
		auto serializer = fac.createObject(prop.custom_type_str);
		if (serializer) {
			// FIXME?? this has lots of issues tbh

			auto str = serializer->serialize(out, prop, ptr, userptr);
			if (diff_ptr) {
				auto str_other = serializer->serialize(out, prop, diff_ptr, userptr);
				if (str == str_other) {
					delete serializer;// skip
					return { {}, false };
				}
			}
			delete serializer;	// fixme: inplace new/free instead?


			// validate:
			for (auto& c : str) {
				if (c == '"') {
					printf("!! dictparser: struct attempted to write '\"' character, removing\n");
					c = ' ';
				}
			}
			value_str = std::move(str);
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop.custom_type_str);
			//ASSERT(0);
		}

	}break;

	case core_type_id::Vec3:
	{
		glm::vec3* v = (glm::vec3*)prop.get_ptr(ptr);

		if (diff_ptr) {
			glm::vec3* other = (glm::vec3*)prop.get_ptr(diff_ptr);
			if (glm::dot(*other-*v, *other-*v) < 0.0005) {
				return { {}, false };
			}
		}

		value_str = string_format("%f %f %f", v->x, v->y, v->z);
	}break;
	case core_type_id::Quat:
	{
		glm::quat* v = (glm::quat*)prop.get_ptr(ptr);

		if (diff_ptr) {
			glm::quat* other = (glm::quat*)prop.get_ptr(diff_ptr);
			if (glm::dot(*other, *v) >= 0.9999){ // equal quaternions
				return { {}, false };
			}
		}

		glm::vec3 eulerA=glm::eulerAngles(*v);

		value_str = string_format("%f %f %f", eulerA.x, eulerA.y, eulerA.z);
	}break;

	}

	if (write_name)
		out.write_key(prop.name);

	return { value_str,true };
}


void write_list(PropertyInfo* listprop, void* ptr, const void* diff_ptr, DictWriter& out, ClassBase* userptr)
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

			auto str = write_field_type(false/* dont write name */, prop.type, member_dat, nullptr, prop, out, userptr);
			//ASSERT(str.second); // FIXME???
			if(str.second)
				buf += str.first;
			buf += ' ';
			out.write_value_no_ln(buf.c_str());
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


void write_properties_with_diff(const PropertyInfoList& list, void* ptr, const void* diff_class, DictWriter& out, ClassBase* userptr)
{
	for (int i = 0; i < list.count; i++)
	{
		auto& prop = list.list[i];

		uint32_t flags = prop.flags;


		if (!(flags & PROP_SERIALIZE))
			continue;

		auto write_val = write_field_type(true, prop.type, ptr, diff_class, prop, out, userptr);
		if (write_val.second)
			out.write_value_quoted(write_val.first.c_str());
	}
}
void write_properties(const PropertyInfoList& list, void* ptr, DictWriter& out, ClassBase* userptr)
{
	for (int i = 0; i < list.count; i++)
	{
		auto& prop = list.list[i];

		uint32_t flags = prop.flags;
		

		if (!(flags & PROP_SERIALIZE))
			continue;
		
		auto write_val = write_field_type(true, prop.type, ptr, nullptr, prop, out, userptr);
		if (write_val.second)
			out.write_value_quoted(write_val.first.c_str());
	}
}

void copy_properties(std::vector<const PropertyInfoList*> lists, void* from, void* to, ClassBase* userptr, IAssetLoadingInterface* load)
{
	DictWriter out;
	for(auto l : lists)
		write_properties(*l, from, out, userptr);
	DictParser parser;
	auto str = std::move(out.get_output());
	parser.load_from_memory((char*)str.c_str(), str.size(), "...");
	std::vector<PropertyListInstancePair> pairs;
	for (auto l : lists)
		pairs.push_back({ l,to });
	read_multi_properties(pairs, parser, {}, userptr,load);
}

// skips to the next '}'
void parse_skip_object(DictParser& in,StringView tok) {
	int nestings = 1;
	for (;;) {
		if (tok.cmp("{"))
			nestings++;
		else if (tok.cmp("}"))
			nestings--;
		if (nestings == 0)
			return;
		if (in.is_eof() || in.has_read_error())
			return;
		in.read_string(tok);
	}
}
void parse_skip_object(DictParser& in) {
	StringView tok;
	in.read_string(tok);
	parse_skip_object(in, tok);
}

void parse_skip_list(DictParser& in) {
	int nestings = 1;
	StringView tok;
	for (;;) {
		in.read_string(tok);
		if (tok.cmp("["))
			nestings++;
		else if (tok.cmp("]"))
			nestings--;
		if (nestings == 0)
			return;
		if (in.is_eof() || in.has_read_error())
			return;
	}
}

// after reading the name of a field, skips the value part (string value, list value, or another object value)
// returns false on unexpected input (like an early '}')
void parse_skip_field(DictParser& in, StringView tok)
{
	if (tok.cmp("[")) {
		parse_skip_list(in);
	}
	else if (tok.cmp("{")) {
		parse_skip_object(in);
	}

	in.read_string(tok);
}



bool read_unique_ptr(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load)
{
	ClassBase* p = read_object_properties<ClassBase>(userptr, in, tok,load);
	*(ClassBase**)ptr = p;
	return true;
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load);
bool read_list_field(PropertyInfo* prop, void* listptr, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load)
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

			if (!read_propety_field(&listcallback->props_in_list->list[0], listcallback->get_index(listptr, count), in, tok, userptr, load))
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


			if (!read_properties(*listcallback->props_in_list, listcallback->get_index(listptr, count), in, tok, userptr,load).second)
				return false;

			count++;

			in.read_string(tok);
		}
	}
	return true;
}
bool read_propety_field(PropertyInfo* prop, void* ptr, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load)
{

	switch (prop->type)
	{

	case core_type_id::Bool:
	case core_type_id::Int8:
	case core_type_id::Int16:
	case core_type_id::Int32:
	case core_type_id::Int64:{
		uint64_t i = _atoi64(tok.to_stack_string().c_str());
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
		auto enum_idx = EnumRegistry::find_enum_by_name(str.c_str());
		

		if (enum_idx.enum_idx < 0) {
			printf("\n\n!!! CANT FIND ENUM %s !!!\n\n", str.c_str());
			prop->set_int(ptr, 0);
		}
		else
			prop->set_int(ptr, enum_idx.enum_idx);

	}break;

	case core_type_id::List: {
		bool res = read_list_field(prop, prop->get_ptr(ptr), in, tok, userptr, load);
		if (!res)
			parse_skip_list(in);
	}break;
	case core_type_id::StdUniquePtr: {
		bool res = read_unique_ptr(prop, prop->get_ptr(ptr), in, tok, userptr,load);
		if (!res)
			parse_skip_object(in);
	}break;
	case core_type_id::Struct: {

		auto& fac = IPropertySerializer::get_factory();
		auto serializer = fac.createObject(prop->custom_type_str);
		if (serializer) {
			serializer->unserialize(in, *prop, ptr, tok, userptr,load);
			delete serializer;	// fixme: inplace new/free instead?
			return true;
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop->custom_type_str);
			return "";
		}

	}break;

	case core_type_id::Vec3:
	{
		glm::vec3* v = (glm::vec3*)prop->get_ptr(ptr);

		int field = sscanf(tok.to_stack_string().c_str(), "%f %f %f", &v->x, &v->y, &v->z);
		return true;
	}
	case core_type_id::Quat:
	{
		glm::quat* v = (glm::quat*)prop->get_ptr(ptr);
		glm::vec3 eulerA{};
		int field = sscanf(tok.to_stack_string().c_str(), "%f %f %f", &eulerA.x, &eulerA.y, &eulerA.z);
		*v = (glm::quat(eulerA));
		return true;
	}

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

static FindInst find_in_proplists(const char* name, std::vector<PropertyListInstancePair>& proplists)
{
	for (auto& prop : proplists) {
		if (!prop.list)
			continue;

		auto val = prop.list->find(name);
		if (val)
			return { val,prop.instance };
	}
	return { nullptr,nullptr };
}
#include "PropHashTable.h"



std::pair<StringView, bool> read_props_to_object(ClassBase* dest_obj,const ClassTypeInfo* typeinfo, DictParser& in, StringView tok, IAssetLoadingInterface* load,ClassBase* userptr)
{
	// expect { (start field list) if not a null token
	if (tok.str_len > 0 && !in.check_item_start(tok))
		return { tok, false };

	in.read_string(tok);
	while (!in.is_eof() && !in.check_item_end(tok))	// exit out if } (end field list)
	{
		if (!typeinfo||!dest_obj) {
			auto name = tok.to_stack_string();
			in.read_string(tok);	// ERROR
			parse_skip_field(in,tok);
			in.read_string(tok);
			continue;
		}

		auto find = typeinfo->prop_hash_table->prop_table.find(tok);// dest_obj->find_in_proplists(name.c_str(), proplists);

		if (find == typeinfo->prop_hash_table->prop_table.end()) {
			auto name = tok.to_stack_string();
			printf("\n\n!!! COULDN'T FIND PARAM %s !!!\n\n", name.c_str());
			in.read_string(tok);	// ERROR
			parse_skip_field(in, tok);
			in.read_string(tok);
			continue;
		}

		in.read_string(tok);
		if (!read_propety_field(find->second, dest_obj, in, tok, userptr,load))
			return { tok, false };


		in.read_string(tok);
	}

	return { tok, true };
}



std::pair<StringView, bool> read_multi_properties(std::vector<PropertyListInstancePair>& proplists, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load)
{
	// expect { (start field list) if not a null token
	if (tok.str_len > 0 && !in.check_item_start(tok))
		return { tok, false };

	in.read_string(tok);
	while (!in.is_eof() && !in.check_item_end(tok))	// exit out if } (end field list)
	{
		auto name = tok.to_stack_string();
		auto find = find_in_proplists(name.c_str(), proplists);

		if (!find.prop) {
			printf("\n\n!!! COULDN'T FIND PARAM %s !!!\n\n", name.c_str());
			
			in.read_string(tok);	// ERROR
			if (tok.cmp("[")) {
				while (in.read_string(tok) && !tok.cmp("]")) {}
			}


			in.read_string(tok);
			//ASSERT(0);
			continue;
		}

		in.read_string(tok);
		if (!read_propety_field(find.prop, find.instptr, in, tok, userptr, load))
			return { tok, false };


		in.read_string(tok);
	}

	return { tok, true };
}

std::pair<StringView, bool> read_properties(const PropertyInfoList& list, void* ptr, DictParser& in, StringView tok, ClassBase* userptr, IAssetLoadingInterface* load)
{
	std::vector<PropertyListInstancePair> props(1);
	props[0] = { &list,ptr };
	return read_multi_properties(props, in, tok, userptr,load);
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

PropertyInfo GetAtomValueWrapper<std::string>::get() {
	return make_string_property("", 0/* 0 offset */, PROP_DEFAULT);
}
PropertyInfo GetAtomValueWrapper<bool>::get() {
	return make_bool_property("", 0/* 0 offset */, PROP_DEFAULT);
}
PropertyInfo GetAtomValueWrapper<uint16_t>::get() {
	return make_integer_property("", 0/* 0 offset */, PROP_DEFAULT, sizeof(uint16_t));
}
PropertyInfo GetAtomValueWrapper<int32_t>::get() {
	return make_integer_property("", 0/* 0 offset */, PROP_DEFAULT, sizeof(int32_t));
}
PropertyInfo GetAtomValueWrapper<uint32_t>::get() {
	return make_integer_property("", 0/* 0 offset */, PROP_DEFAULT, sizeof(uint32_t));
}
PropertyInfo GetAtomValueWrapper<float>::get() {
	return make_float_property("", 0/* 0 offset */, PROP_DEFAULT);
}


Factory<std::string, IPropertySerializer>& IPropertySerializer::get_factory()
{
	static Factory<std::string, IPropertySerializer> inst;
	return inst;
}