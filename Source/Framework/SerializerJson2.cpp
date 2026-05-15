#include "SerializerJson2.h"
#include "Assets/IAsset.h"
#include "Framework/MapUtil.h"
#include "Log.h"
#include "SerializedForDiffing.h"
#include "LevelSerialization/SerializeNew.h"

// extern string serialize_build_relative_path(const char* from, const char* to);
// extern string unserialize_relative_to_absolute(const char* relative, const char* root);

using std::vector;

void WriteSerializerBackendJson2::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info,
														 ClassBase*& ptr, bool is_only_reference) {
	return;
	ASSERT(0);
}

void WriteSerializerBackendJson2::write_actual_class(ClassBase* ptr, const string& path) {
	return;
	ASSERT(0);
}

WriteSerializerBackendJson2::WriteSerializerBackendJson2(const char* debug_tag, ClassBase& obj_to_serialize)
	: debug_tag(debug_tag) {
	stack.push_back({&obj});
	ClassBase* ptr = &obj_to_serialize;
	assert(!currently_writing_class);
	currently_writing_class = ptr;
	string className = ptr->get_type().classname;
	// serialize("__classname", className);
	ptr->serialize(*this);
	for (auto p : ClassPropPtr(ptr)) {
		serialize_property(p);
	}
	currently_writing_class = nullptr;
	// now diff it

	auto diff = ptr->get_type().diff_data.get();
	if (diff) {
		(*stack.back().ptr) = JsonSerializerUtil::diff_json(diff->jsonObj, (*stack.back().ptr));
	}
	stack.pop_back();
}

bool WriteSerializerBackendJson2::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) {
	serialize_class_shared(tag, info, ptr, false);
	return true;
}

bool WriteSerializerBackendJson2::serialize_class_reference(const char* tag, const ClassTypeInfo& info,
															ClassBase*& ptr) {
	serialize_class_shared(tag, info, ptr, true);
	return true;
}

// --- Enum serialization ---
// Wire format: emit the unqualified value name as a string (e.g. "Realtime") so the file
// is robust to enum value reordering across versions. If the runtime value has no matching
// name (corrupt / out-of-range), fall back to the integer so we never silently lose data.
bool WriteSerializerBackendJson2::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) {
	if (!info)
		return false;
	auto& back = get_back();
	if (auto* pair = info->find_for_value(i))
		(*back.ptr)[tag] = pair->name;
	else
		(*back.ptr)[tag] = i;
	return true;
}

void WriteSerializerBackendJson2::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) {
	serialize_class_shared(std::nullopt, info, ptr, false);
}

void WriteSerializerBackendJson2::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) {
	serialize_class_shared(std::nullopt, info, ptr, true);
}

void WriteSerializerBackendJson2::serialize_enum_ar(const EnumTypeInfo* info, int& i) {
	auto& back = get_back();
	if (info) {
		if (auto* pair = info->find_for_value(i)) {
			(*back.ptr)[back.arr_idx++] = pair->name;
			return;
		}
	}
	(*back.ptr)[back.arr_idx++] = i;
}

void WriteSerializerBackendJson2::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr) {
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize_ar(name);
}

bool WriteSerializerBackendJson2::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr) {
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize(tag, name);
	return true;
}
ReadSerializerBackendJson2::ReadSerializerBackendJson2(const char* debug_tag, nlohmann::json& json_obj,
													   ClassBase& obj)
	: debug_tag(debug_tag), rootobj(obj) {
	this->obj = &json_obj;
	load_shared();
	this->obj = nullptr;
	stack.clear();
}

void ReadSerializerBackendJson2::load_shared() {
	stack.push_back(obj);

	rootobj.serialize(*this);
	for (PropertyPtr property : ClassPropPtr(&rootobj)) {
		try {
			serialize_property(property);
		}
		// SerializeInputError is the user-facing "bad data" signal; enrich with the property
		// path so the message points at the exact field, then re-throw so the load aborts
		// rather than silently producing a half-loaded object.
		catch (const SerializeInputError& e) {
			throw SerializeInputError(std::string(rootobj.get_type().classname) + "." +
									  property.get_name() + ": " + e.what());
		}
		// Unexpected std::exception from inner code (e.g. nlohmann::json type mismatch when
		// we forgot to check is_array): log with full context and continue, matching prior
		// best-effort behaviour. These are bugs in our serializer rather than bad input.
		catch (const std::exception& e) {
			sys_print(Error, "ReadSerializerBackendJson2(%s): error serializing %s.%s: %s\n",
					  get_debug_tag(), rootobj.get_type().classname, property.get_name(), e.what());
		}
		catch (...) {
			sys_print(Error, "ReadSerializerBackendJson2(%s): unknown error serializing %s.%s\n",
					  get_debug_tag(), rootobj.get_type().classname, property.get_name());
		}
	}
	stack.pop_back();
	assert(stack.empty());
}

bool ReadSerializerBackendJson2::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) {
	return false;
}

bool ReadSerializerBackendJson2::serialize_class_reference(const char* tag, const ClassTypeInfo& info,
														   ClassBase*& ptr) {
	return false;
}

bool ReadSerializerBackendJson2::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) {
	if (!info)
		return false;
	auto& back = get_back();
	auto& backptr = *back.ptr;
	mark_consumed(tag);
	if (!backptr.is_object() || !backptr.contains(tag))
		return false;
	const auto& v = backptr[tag];
	if (v.is_string()) {
		auto name = v.get<std::string>();
		if (auto* pair = info->find_for_name(name.c_str())) {
			i = (int)pair->value;
			return true;
		}
		// Unknown name: keep prior value, log so it doesn't fail silently.
		sys_print(Warning, "ReadSerializerBackendJson2: enum '%s' has unknown value '%s'\n",
				  info->name ? info->name : "?", name.c_str());
		return false;
	}
	if (v.is_number_integer()) {
		i = v.get<int>();
		return true;
	}
	return false;
}

void ReadSerializerBackendJson2::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) {
	return;
}

void ReadSerializerBackendJson2::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) {
	return;
}

void ReadSerializerBackendJson2::serialize_enum_ar(const EnumTypeInfo* info, int& i) {
	auto& back = get_back();
	auto& backptr = *back.ptr;
	if (!backptr.is_array() || (size_t)back.arr_idx >= backptr.size())
		return;
	const auto& v = backptr[back.arr_idx++];
	if (v.is_string() && info) {
		auto name = v.get<std::string>();
		if (auto* pair = info->find_for_name(name.c_str())) {
			i = (int)pair->value;
			return;
		}
		sys_print(Warning, "ReadSerializerBackendJson2: enum '%s' has unknown value '%s'\n",
				  info->name ? info->name : "?", name.c_str());
		return;
	}
	if (v.is_number_integer())
		i = v.get<int>();
}

#include "Assets/AssetDatabase.h"
void ReadSerializerBackendJson2::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr) {
	string path = "";
	serialize_ar(path);
	if (path.empty())
		ptr = nullptr;
	else
		ptr = g_assets.generic_find(path, &info).get_unsafe();
}
bool ReadSerializerBackendJson2::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr) {
	string path = "";
	bool found = serialize(tag, path);
	if (!found)
		return false;
	if (path.empty())
		ptr = nullptr;
	else
		ptr = g_assets.generic_find(path, &info).get_unsafe();
	return true;
}
