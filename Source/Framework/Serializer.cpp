#include "Serializer.h"
#include "Game/BaseUpdater.h"
#include "GameEnginePublic.h"
class IAsset;

void Serializer::serialize_property(PropertyPtr ptr) {
	serialize_field(ptr.get_name(), ptr.info(), ptr.get_instance());
}

void Serializer::serialize_field(const char* tag, const PropertyInfo& prop, void* inst) {
	ASSERT(tag && inst);
	const char* former = is_loading() ? prop.former_name : nullptr;
	if (prop.type == core_type_id::List) {
		const ListAccess& list = prop.list();
		int sz = list.size(prop, inst);
		if (serialize_array(tag, sz) || (former && serialize_array(former, sz))) {
			if (is_loading())
				list.resize(prop, inst, sz);
			for (int i = 0; i < sz; i++)
				serialize_element(prop, inst, i);
			end_obj();
		}
	} else if (prop.type == core_type_id::ActualStruct) {
		if (serialize_dict(tag) || (former && serialize_dict(former))) {
			prop.with_struct(inst, [&](StructRef s) { serialize_struct_members(s); });
			end_obj();
		}
	} else if (prop.has_value()) {
		PropValue value = prop.get_value(inst);
		const bool found = serialize_domain(tag, prop, value) || (former && serialize_domain(former, prop, value));
		if (found && is_loading())
			prop.set_value(inst, value, SetMode::Load);
	}
}

void Serializer::serialize_element(const PropertyInfo& list_prop, void* inst, int index) {
	const ListAccess& list = list_prop.list();
	const PropertyInfo& element = *list.element;
	if (element.type == core_type_id::ActualStruct) {
		if (serialize_dict_ar()) {
			list.with_element(list_prop, inst, index, [&](StructRef s) { serialize_struct_members(s); });
			end_obj();
		}
	} else {
		PropValue value;
		list.get(list_prop, inst, index, value);
		if (serialize_domain(nullptr, element, value) && is_loading())
			list.set(list_prop, inst, index, value, SetMode::Load);
	}
}

void Serializer::serialize_struct_members(StructRef s) {
	ASSERT(s.type && s.data);
	if (s.type->custom_serialize)
		s.type->custom_serialize(s.data, *this);
	if (const PropertyInfoList* props = s.type->properties)
		for (int i = 0; i < props->count; i++)
			serialize_field(props->list[i].name, props->list[i], s.data);
}

template <typename T> bool Serializer::serialize_tagged(const char* tag, T& value) {
	if (tag)
		return serialize(tag, value);
	serialize_ar(value);
	return true;
}

// Float fields stay floats, colors are their uint value.
bool Serializer::serialize_domain(const char* tag, const PropertyInfo& prop, PropValue& value) {
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
	return found;
}
