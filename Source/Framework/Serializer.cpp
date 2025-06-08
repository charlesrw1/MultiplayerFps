#include "Serializer.h"

class SerializerRoot
{
public:
	std::unordered_map<std::string, ClassBase*> reference_table;


	// holds references etc
	// serialize object, gets added to table with its path
	// also can get the exter references when finished. references that arent in the set
};


void Serializer::serialize_property_ar(PropertyPtr ptr)
{
	assert(ptr.is_an_array_property());
	if (ptr.is_array()) {
		auto arrayprp = ptr.as_array();
		int sz = arrayprp.get_array_size();
		const bool has_array = serialize_array_ar(sz);
		if (has_array) {
			if (is_loading())
				arrayprp.resize_array(sz);
			for (auto p : arrayprp) {
				serialize_property_ar(p);
			}
			end_obj();
		}
	}
	else if (ptr.is_struct()) {
		const bool hasstruct = serialize_dict_ar();
		if (hasstruct) {
			auto asstruct = ptr.as_struct();
			asstruct.call_serialize(*this);
			for (auto p : asstruct) {
				serialize_property(p);
			}
			end_obj();
		}
	}
	else if (ptr.is_string()) {
		auto& ref = ptr.as_string();
		serialize_ar(ref);
	}
	else {
		switch (ptr.get_type()) {
		case core_type_id::Bool: {
			bool& b = ptr.as_boolean();
			serialize_ar(b);
		}break;
		case core_type_id::Int8:
		case core_type_id::Int16:
		case core_type_id::Int32:
		case core_type_id::Int64: {
			int64_t i = ptr.get_integer_casted();
			serialize_ar(i);
			if (is_loading())
				ptr.set_integer_casted(i);
		}break;
		case core_type_id::Float: {
			float& f = ptr.as_float();
			serialize_ar(f);
		}break;

		}
	}
}
void Serializer::serialize_property(PropertyPtr ptr)
{
	assert(!ptr.is_an_array_property());
	if (ptr.is_array()) {
		auto arrayprp = ptr.as_array();
		int sz = arrayprp.get_array_size();
		const bool has_array = serialize_array(ptr.get_name(), sz);
		if (has_array) {
			if (is_loading())
				arrayprp.resize_array(sz);
			for (auto p : arrayprp) {
				serialize_property_ar(p);
			}
			end_obj();
		}
	}
	else if (ptr.is_struct()) {
		const bool hasstruct = serialize_dict(ptr.get_name());
		if (hasstruct) {
			auto asstruct = ptr.as_struct();
			asstruct.call_serialize(*this);
			for (auto p : asstruct) {
				serialize_property(p);
			}
			end_obj();
		}
	}
	else if (ptr.is_string()) {
		auto& ref = ptr.as_string();
		serialize(ptr.get_name(), ref);
	}
	else if (ptr.is_enum()) {
		//int32_t i = ptr.as_enum().get_as_integer();
		//serialize(ptr.get_name(), i);
	}
	else {
		switch (ptr.get_type()) {
		case core_type_id::Bool: {
			bool& b = ptr.as_boolean();
			serialize(ptr.get_name(),b);
		}break;
		case core_type_id::Int8:
		case core_type_id::Int16:
		case core_type_id::Int32:
		case core_type_id::Int64: {
			int64_t i = ptr.get_integer_casted();
			bool found = serialize(ptr.get_name(),i);
			if (found && is_loading())
				ptr.set_integer_casted(i);
		}break;
		case core_type_id::Float: {
			float& f = ptr.as_float();
			serialize(ptr.get_name(), f);
		}break;

		}
	}
}
