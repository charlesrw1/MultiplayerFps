#include "SerializerFunctions.h"
#include "PropHashTable.h"

#include "SerializerFunctions.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <stdexcept>

#include "DictParser.h"
#include "DictWriter.h"


void write_properties_binary(const PropertyInfoList& list, void* ptr, FileWriter& out, ClassBase* userptr);
void read_properties_binary(const PropertyInfoList& list, void* ptr, BinaryReader& in, ClassBase* userptr);

struct FindInst2
{
	PropertyInfo* prop = nullptr;
	void* instptr = nullptr;
};

static FindInst2 find_in_proplists(const char* name, std::vector<PropertyListInstancePair>& proplists)
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

void write_field_name_and_type(PropertyInfo* prop, FileWriter* out)
{
	out->write_string(prop->name);
	out->write_byte((int)prop->type);
}

void write_list_binary(PropertyInfo* prop, void* ptr, FileWriter& out, ClassBase* userptr);
void write_field_type_binary(bool write_name, core_type_id type, void* ptr, const void* diff_ptr, PropertyInfo& prop, FileWriter& out, ClassBase* userptr) {


	switch (prop.type)
	{
	case core_type_id::Bool:
	case core_type_id::Int8:
		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_byte(prop.get_int(ptr));

		break;
	case core_type_id::Int16:
		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_int16(prop.get_int(ptr));

		break;
	case core_type_id::Int32:
		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_int32(prop.get_int(ptr));

		break;
	case core_type_id::Int64:
		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_int64(prop.get_int(ptr));

		break;
	case core_type_id::Float:
		if (diff_ptr) {
			float mine = prop.get_float(ptr);
			float other = prop.get_float(diff_ptr);
			if (std::abs(mine - other) < 0.00001)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_float(prop.get_float(ptr));
		
		break;
	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32: {
		if (diff_ptr) {
			uint64_t mine = prop.get_int(ptr);
			uint64_t other = prop.get_int(diff_ptr);
			if (mine == other)	// skip
				return;
		}
		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_int32(prop.get_int(ptr));

	}break;

	case core_type_id::StdString: {
		if (diff_ptr) {
			std::string* mine = (std::string*)prop.get_ptr(ptr);
			std::string* other = (std::string*)prop.get_ptr(diff_ptr);
			if (*mine == *other)	// skip
				return;
		}

		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_string(*(std::string*)prop.get_ptr(ptr));

	}break;

	case core_type_id::List: {

		if (diff_ptr) {
			size_t count_me = prop.list_ptr->get_size(prop.get_ptr(ptr));
			size_t count_diff = prop.list_ptr->get_size(prop.get_ptr(diff_ptr));
			if (count_diff == 0 && count_me == 0)
				return;

		}

		if (write_name)
			write_field_name_and_type(&prop, &out);

		write_list_binary(&prop, prop.get_ptr(ptr), out, userptr);

		
	}break;

	case core_type_id::Struct: {

		auto& fac = IPropertySerializer::get_factory();
		auto serializer = fac.createObject(prop.custom_type_str);
		if (serializer) {
			// FIXME?? this has lots of issues tbh

			DictWriter DUMMY;
			auto str = serializer->serialize(DUMMY, prop, ptr, userptr);
			if (diff_ptr) {
				auto str_other = serializer->serialize(DUMMY, prop, diff_ptr, userptr);
				if (str == str_other) {
					delete serializer;// skip
					return;
				}
			}
			delete serializer;	// fixme: inplace new/free instead?
			
			if (write_name)
				write_field_name_and_type(&prop, &out);

			out.write_string(str);
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop.custom_type_str);
			ASSERT(0);
			throw std::runtime_error("no serializer struct");
		}

	}break;

	case core_type_id::Vec3:
	{
		glm::vec3* v = (glm::vec3*)prop.get_ptr(ptr);

		if (diff_ptr) {
			glm::vec3* other = (glm::vec3*)prop.get_ptr(diff_ptr);
			if (glm::dot(*other - *v, *other - *v) < 0.001) {
				return;
			}
		}

		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_float(v->x);
		out.write_float(v->y);
		out.write_float(v->z);

	}break;
	case core_type_id::Quat:
	{
		glm::quat* v = (glm::quat*)prop.get_ptr(ptr);

		if (write_name)
			write_field_name_and_type(&prop, &out);

		out.write_float(v->x);
		out.write_float(v->y);
		out.write_float(v->z);
		out.write_float(v->w);
	}break;

	}

	out.write_int32(0xDEADBEEF);
}

void write_list_binary(PropertyInfo* listprop, void* ptr, FileWriter& out, ClassBase* userptr)
{
	// special case for atom types
	auto list_ptr = listprop->list_ptr;
	ASSERT(list_ptr);
	bool is_atom_type =
		(list_ptr->props_in_list->count == 1);
	if (is_atom_type) {

		auto& prop = list_ptr->props_in_list->list[0];
		std::string buf;
		int count = list_ptr->get_size(ptr);

		out.write_int32(count);

		for (int i = 0; i < count; i++) {

			uint8_t* member_dat = list_ptr->get_index(ptr, i);

			write_field_type_binary(false/* dont write name */, prop.type, member_dat, nullptr, prop, out, userptr);
		}
	}
	else {

		int count = list_ptr->get_size(ptr);
		out.write_int32(count);
		for (int i = 0; i < count; i++) {

			uint8_t* member_dat = list_ptr->get_index(ptr, i);

			write_properties_binary(*list_ptr->props_in_list, list_ptr->get_index(ptr, i), out, userptr);

			out.write_string("");
		}
	}
}


void write_properties_with_diff_binary(const PropertyInfoList& list, void* ptr, const void* diff_class, FileWriter& out, ClassBase* userptr)
{
	for (int i = 0; i < list.count; i++)
	{
		auto& prop = list.list[i];

		uint32_t flags = prop.flags;


		if (!(flags & PROP_SERIALIZE))
			continue;

		write_field_type_binary(true, prop.type, ptr, diff_class, prop, out, userptr);
	}
}
void write_properties_binary(const PropertyInfoList& list, void* ptr, FileWriter& out, ClassBase* userptr)
{
	for (int i = 0; i < list.count; i++)
	{
		auto& prop = list.list[i];

		uint32_t flags = prop.flags;


		if (!(flags & PROP_SERIALIZE))
			continue;

		write_field_type_binary(true, prop.type, ptr, nullptr, prop, out, userptr);
	}
}

void read_propety_field_binary(PropertyInfo* prop, void* ptr, BinaryReader& in, ClassBase* userptr);
bool read_list_field_binary(PropertyInfo* prop, void* listptr, BinaryReader& in, ClassBase* userptr)
{
	auto listcallback = prop->list_ptr;
	bool is_atom_type =
		(listcallback->props_in_list->count == 1);

	const uint32_t list_size = in.read_int32();
	listcallback->resize(listptr, list_size);

	if (is_atom_type) {
		uint32_t count = 0;
		for(;count < list_size;count++) {

			read_propety_field_binary(
				&listcallback->props_in_list->list[0],
				listcallback->get_index(listptr, count),
				in, userptr);
		}
	}
	else {
		uint32_t count = 0;
		for(;count<list_size;count++) {

			read_properties_binary(
				*listcallback->props_in_list,
				listcallback->get_index(listptr, count), in, userptr);

		}
	}
	return true;
}
void read_propety_field_binary(PropertyInfo* prop, void* ptr, BinaryReader& in, ClassBase* userptr)
{
	switch (prop->type)
	{

	case core_type_id::Bool:
	case core_type_id::Int8:
		prop->set_int(ptr, in.read_byte());
		break;
	case core_type_id::Int16:
		prop->set_int(ptr, in.read_int16());
		break;
	case core_type_id::Int32:
		prop->set_int(ptr, in.read_int32());
		break;
	case core_type_id::Int64:
		prop->set_int(ptr, in.read_int64());
		break;

	case core_type_id::Float: {
		prop->set_float(ptr, in.read_float());
		break;
	}break;

	case core_type_id::StdString: {
		std::string* i = (std::string*)((char*)ptr + prop->offset);
		StringView sv = in.read_string_view();
		*i = std::string(sv.str_start, sv.str_len);
	}break;

	case core_type_id::Enum8:
	case core_type_id::Enum16:
	case core_type_id::Enum32: {
		prop->set_int(ptr, in.read_int32());
		break;
	}break;

	case core_type_id::List:
		read_list_field_binary(prop, prop->get_ptr(ptr), in, userptr);
		break;
	case core_type_id::Struct: {

		auto& fac = IPropertySerializer::get_factory();
		auto serializer = fac.createObject(prop->custom_type_str);
		StringView sv = in.read_string_view();
		if (serializer) {
			DictParser DUMMY;
			serializer->unserialize(DUMMY, *prop, ptr, sv, userptr);
			delete serializer;	// fixme: inplace new/free instead?
		}
		else {
			printf("!!!!!! NO SERIALIZER FOR STRUCT %s !!!!!!!!", prop->custom_type_str);
			return;
		}

	}break;

	case core_type_id::Vec3:
	{
		glm::vec3* v = (glm::vec3*)prop->get_ptr(ptr);
		v->x = in.read_float();
		v->y = in.read_float();
		v->z = in.read_float();

	}break;
	case core_type_id::Quat:
	{
		glm::quat* v = (glm::quat*)prop->get_ptr(ptr);
		v->x = in.read_float();
		v->y = in.read_float();
		v->z = in.read_float();
		v->w = in.read_float();
	}break;

	default:
		Fatalf("bad core type id\n");
		ASSERT(0);
		break;
	}

	auto val = in.read_int32();
	ASSERT(val == 0xDEADBEEF);

	return;
}


void read_multi_properties_binary(std::vector<PropertyListInstancePair>& proplists, BinaryReader& in,  ClassBase* userptr)
{
	while (!in.has_failed())
	{
		StringView property_name = in.read_string_view();

		if (property_name.str_len == 0)	// null terminator
			return;

		core_type_id type = core_type_id(in.read_byte());

		std::string name(property_name.str_start, property_name.str_len);
		auto find = find_in_proplists(name.c_str(), proplists);

		if (!find.prop) {
			printf("\n\n!!! COULDN'T FIND PARAM %s !!!\n\n", name.c_str());
			throw std::runtime_error("bad parse\n");
			continue;
		}
		if (find.prop->type != type)
			throw std::runtime_error("type mismatches with existing");

		read_propety_field_binary(find.prop, find.instptr, in, userptr);
	}
}

void read_properties_binary(const PropertyInfoList& list, void* ptr, BinaryReader& in, ClassBase* userptr)
{
	std::vector<PropertyListInstancePair> props(1);
	props[0] = { &list,ptr };
	read_multi_properties_binary(props, in, userptr);
}



void read_props_to_object_binary(ClassBase* dest_obj, const ClassTypeInfo* typeinfo, BinaryReader& in, ClassBase* userptr)
{

	while (!in.has_failed())
	{
		StringView prop_name = in.read_string_view();
		if (prop_name.str_len == 0)	// null terminator
			return;

		auto find = typeinfo->prop_hash_table->prop_table.find(prop_name);// dest_obj->find_in_proplists(name.c_str(), proplists);

		if (find == typeinfo->prop_hash_table->prop_table.end()) {
			std::string prop_name_as_std(prop_name.str_start, prop_name.str_len);
			printf("\n\n!!! COULDN'T FIND PARAM %s !!!\n\n", prop_name_as_std.c_str());
			throw std::runtime_error("couldnt find param error: " + prop_name_as_std);
			continue;
		}

		core_type_id type = (core_type_id)in.read_byte();

		read_propety_field_binary(find->second, dest_obj, in, userptr);
	}
}
