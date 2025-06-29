#include "Serializer.h"
#include "Game/BaseUpdater.h"
#include "GameEnginePublic.h"
class IAsset;

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
		case core_type_id::Enum8:
		case core_type_id::Enum16:
		case core_type_id::Enum32:
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
		case core_type_id::Quat: {
			serialize_ar(ptr.as_quat());
		}break;
		case core_type_id::Vec2: {
			serialize_ar(ptr.as_vec2());
		}break;
		case core_type_id::Vec3: {
			serialize_ar(ptr.as_vec3());
		}break;
		case core_type_id::StringName: {
			if (is_loading()) {
				string s;
				serialize_ar(s);
				ptr.as_string_name() = StringName(s.c_str());
			}
			else {
				string s = ptr.as_string_name().get_c_str();
				serialize_ar(s);
			}
		}break;
		case core_type_id::AssetPtr: {
			auto pi = ptr.get_property_info();
			IAsset** assetptr = (IAsset**)pi->get_ptr(ptr.get_instance_ptr_unsafe());
			serialize_asset_ar(*pi->class_type, *assetptr);
		}break;
		case core_type_id::ObjHandlePtr: {
			auto pi = ptr.get_property_info();
			int64_t* handle = (int64_t*)pi->get_ptr(ptr.get_instance_ptr_unsafe());
			if (is_loading()) {
				BaseUpdater* b = nullptr;
				serialize_class_reference_ar(b);
				if (b) {
					*((BaseUpdater**)handle) = b;	// gets fixed up later
				}
			}
			else {
				BaseUpdater* b = nullptr;
				if (*handle != 0)
					b = eng->get_object(*handle);
				serialize_class_reference_ar(b);
			}
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
	else {
		switch (ptr.get_type()) {
		case core_type_id::Bool: {
			bool& b = ptr.as_boolean();
			serialize(ptr.get_name(),b);
		}break;
		case core_type_id::Enum8:
		case core_type_id::Enum16:
		case core_type_id::Enum32:
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
		case core_type_id::Quat: {
			serialize(ptr.get_name(),ptr.as_quat());
		}break;
		case core_type_id::Vec2: {
			serialize(ptr.get_name(),ptr.as_vec2());
		}break;
		case core_type_id::Vec3: {
			serialize(ptr.get_name(),ptr.as_vec3());
		}break;
		case core_type_id::StringName: {
			if (is_loading()) {
				string s;
				serialize(ptr.get_name(), s);
				ptr.as_string_name() = StringName(s.c_str());
			}
			else {
				string s = ptr.as_string_name().get_c_str();
				serialize(ptr.get_name(), s);
			}
		}break;
		case core_type_id::AssetPtr: {
			auto pi = ptr.get_property_info();
			IAsset** assetptr = (IAsset**)pi->get_ptr(ptr.get_instance_ptr_unsafe());
			serialize_asset(ptr.get_name(), *pi->class_type, *assetptr);
		}break;
		case core_type_id::ObjHandlePtr: {
			auto pi = ptr.get_property_info();
			int64_t* handle = (int64_t*)pi->get_ptr(ptr.get_instance_ptr_unsafe());
			if (is_loading()) {
				BaseUpdater* b = nullptr;
				serialize_class_reference(ptr.get_name(), b);
				if (b) {
					*((BaseUpdater**)handle) = b;	// gets fixed up later
				}
			}
			else {
				BaseUpdater* b = nullptr;
				if(*handle!=0)
					b = eng->get_object(*handle);
				serialize_class_reference(ptr.get_name(), b);
			}
		}break;
		}
	}
}
