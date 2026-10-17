#include "Serializer.h"
#include "Game/BaseUpdater.h"
#include "GameEnginePublic.h"
class IAsset;

void Serializer::serialize_property(PropertyPtr ptr) {
	assert(!ptr.is_an_array_property());
	serialize_property_shared(ptr.get_name(), ptr);
}

void Serializer::serialize_property_ar(PropertyPtr ptr) {
	assert(ptr.is_an_array_property());
	serialize_property_shared(nullptr, ptr);
}

void Serializer::serialize_property_shared(const char* tag, PropertyPtr ptr) {
	const PropertyInfo& prop = *ptr.get_property_info();
	// A renamed property reads its data under the former name when the current name is missing.
	const char* former = tag && is_loading() ? prop.former_name : nullptr;
	if (ptr.is_array()) {
		auto arrayprp = ptr.as_array();
		int sz = arrayprp.get_array_size();
		const bool has_array =
			tag ? serialize_array(tag, sz) || (former && serialize_array(former, sz)) : serialize_array_ar(sz);
		if (has_array) {
			if (is_loading())
				arrayprp.resize_array(sz);
			for (auto p : arrayprp)
				serialize_property_ar(p);
			end_obj();
		}
	} else if (ptr.is_struct()) {
		const bool hasstruct = tag ? serialize_dict(tag) || (former && serialize_dict(former)) : serialize_dict_ar();
		if (hasstruct) {
			auto asstruct = ptr.as_struct();
			asstruct.call_serialize(*this);
			for (auto p : asstruct)
				serialize_property(p);
			end_obj();
		}
	} else if (prop.has_value()) {
		void* inst = ptr.get_instance_ptr_unsafe();
		if (!serialize_value(tag, prop, inst) && former)
			serialize_value(former, prop, inst);
	}
}

template <typename T> bool Serializer::serialize_tagged(const char* tag, T& value) {
	if (tag)
		return serialize(tag, value);
	serialize_ar(value);
	return true;
}

// The wire format of each domain value. Float fields stay floats, colors are their uint value.
bool Serializer::serialize_value(const char* tag, const PropertyInfo& prop, void* inst) {
	PropValue value = prop.get_value(inst);
	bool found = false;
	if (auto* b = std::get_if<bool>(&value)) {
		found = serialize_tagged(tag, *b);
	} else if (auto* i = std::get_if<int64_t>(&value)) {
		found = serialize_tagged(tag, *i);
	} else if (auto* d = std::get_if<double>(&value)) {
		if (prop.type == core_type_id::Float) {
			float f = static_cast<float>(*d);
			found = serialize_tagged(tag, f);
			*d = f;
		} else {
			found = serialize_tagged(tag, *d);
		}
	} else if (auto* s = std::get_if<std::string>(&value)) {
		found = serialize_tagged(tag, *s);
	} else if (auto* v2 = std::get_if<glm::vec2>(&value)) {
		found = serialize_tagged(tag, *v2);
	} else if (auto* v3 = std::get_if<glm::vec3>(&value)) {
		found = serialize_tagged(tag, *v3);
	} else if (auto* q = std::get_if<glm::quat>(&value)) {
		found = serialize_tagged(tag, *q);
	} else if (auto* c = std::get_if<Color32>(&value)) {
		int64_t bits = c->to_uint();
		found = serialize_tagged(tag, bits);
		*c = Color32(static_cast<unsigned int>(bits));
	} else if (auto* asset = std::get_if<AssetRef>(&value)) {
		ASSERT(asset->type);
		if (tag)
			found = serialize_asset(tag, *asset->type, asset->asset);
		else {
			serialize_asset_ar(*asset->type, asset->asset);
			found = true;
		}
	} else if (auto* ref = std::get_if<EntityRef>(&value)) {
		BaseUpdater* object = ref->handle && is_saving() ? eng->get_object(ref->handle) : nullptr;
		if (tag)
			found = serialize_class_reference(tag, object);
		else {
			serialize_class_reference_ar(object);
			found = true;
		}
		found = found && object;
		if (object)
			ref->handle = object->get_instance_id();
	} else {
		return false; // ClassTypeInfo properties are not serialized
	}
	if (found && is_loading())
		prop.set_value(inst, value, SetMode::Load);
	return found;
}
