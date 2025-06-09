#include "SerializerJson.h"
#include "Assets/IAsset.h"
#include "Framework/MapUtil.h"

const char* const CLASSNAME = "classname";

extern std::string serialize_build_relative_path(const char* from, const char* to);
extern std::string unserialize_relative_to_absolute(const char* relative, const char* root);
static bool is_path_a_subpath(const std::string& p)
{
	auto find = p.find('/');
	return find != std::string::npos;
}

void WriteSerializerBackendJson::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference)
{
	const bool is_array = !tag.has_value();

	if (!ptr) {
		std::string path = "";
		if (is_array)
			serialize_ar(path);
		else
			serialize(tag.value(), path);
		return;
	}

	auto path = pathmaker.make_path(ptr);
	std::string pathtowrite = "";
	if (currently_writing_class) {
		auto parentpath = pathmaker.make_path(currently_writing_class);
		auto relative = serialize_build_relative_path(parentpath.c_str(), path.c_str());
		pathtowrite = relative;
	}
	else
		pathtowrite = path;

	if (is_array)
		serialize_ar(pathtowrite);
	else
		serialize(tag.value(), pathtowrite);

	if(!is_only_reference)
		write_queue.push_back(ptr);

	if (!MapUtil::contains(paths_to_objects, path)) {
		paths_to_objects[path].o = ptr;
		paths_to_objects[path].is_from_sub_object = is_path_a_subpath(path);
	}
}

void WriteSerializerBackendJson::write_actual_class(ClassBase* ptr, const std::string& path)
{
	assert(ptr);
	assert(!currently_writing_class);

	currently_writing_class = ptr;
	serialize_dict(path.c_str());
	ptr->serialize(*this);
	for (auto p : ClassPropPtr(ptr)) {
		serialize_property(p);
	}
	end_obj();
	currently_writing_class = nullptr;

	// now diff it
	auto diff = pathmaker.find_diff_for_obj(ptr);
	if (diff) {
		(*stack.back().ptr)[path] = JsonSerializerUtil::diff_json(*diff, (*stack.back().ptr)[path]);
	}
}

WriteSerializerBackendJson::WriteSerializerBackendJson(IMakePathForObject& pathmaker, ClassBase& obj_to_serialize) 
	:pathmaker(pathmaker)
{
	stack.push_back(&obj);
	{
		ClassBase* ptr = &obj_to_serialize;
		serialize_class("root", ClassBase::StaticType, ptr);
	}
	serialize_dict("objs");
	while (!write_queue.empty()) {
		ClassBase* obj = write_queue.back();
		write_queue.pop_back();
		const string path = pathmaker.make_path(obj);
		assert(MapUtil::contains(paths_to_objects, path));

		if(!paths_to_objects.find(path)->second.has_been_written)
			write_actual_class(obj,path);

		paths_to_objects[path].has_been_written = true;
	}
	end_obj();

	int dummysz{};
	serialize_array("paths", dummysz);
	for (auto& o : paths_to_objects) {
		const bool is_valid = o.second.has_been_written && !o.second.is_from_sub_object;
		if (is_valid) {
			serialize_array_ar(dummysz);
			std::string path = o.first;
			serialize_ar(path);
			auto type = pathmaker.make_type_name(o.second.o);
			serialize_ar(type);
			end_obj();
		}
	}
	end_obj();
}

bool WriteSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(tag, info, ptr, false);
	return true;
}

bool WriteSerializerBackendJson::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(tag, info, ptr, true);
	return true;
}

bool WriteSerializerBackendJson::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
	return false;
}

void WriteSerializerBackendJson::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, false);
}

void WriteSerializerBackendJson::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, true);
}

void WriteSerializerBackendJson::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
}

void WriteSerializerBackendJson::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr)
{
	std::string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize_ar(name);
}

bool WriteSerializerBackendJson::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
{
	std::string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize(tag, name);
	return true;
}

ReadSerializerBackendJson::ReadSerializerBackendJson(const std::string& text, IMakeObjectFromPath& objmaker, IAssetLoadingInterface& loader)
	:objmaker(objmaker), loader(loader)
{
	this->obj = nlohmann::json::parse(text);
	stack.push_back(&obj);

	int sz = 0;
	if (serialize_array("paths",sz)) {
		for (int i = 0; i < sz; i++) {
			int count = 0;
			serialize_array_ar(count);
			if (count != 2) {
				printf("count != 2 in paths json serialize\n");
				continue;
			}
			string path;
			string type;
			serialize_ar(path);
			serialize_ar(type);
			ClassBase* obj = objmaker.create_from_name(*this, type);
			
			MapUtil::insert_test_exists(path_to_objs, path, obj);
			if (obj)
				MapUtil::insert_test_exists(obj_to_path, obj, path);

			end_obj();
		}
		end_obj();
	}
	
	serialize_class("root", ClassBase::StaticType, rootobj);

	serialize_dict("objs");
	for (auto& p : path_to_objs) {
		if (p.second && serialize_dict(p.first.c_str())) {
			assert(!current_root_path);
			current_root_path = &p.first;
			p.second->serialize(*this);
			for (auto p : ClassPropPtr(p.second)) {
				serialize_property(p);
			}
			current_root_path = nullptr;
			end_obj();
		}
	}
	end_obj();
}

bool ReadSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	return serialize_class_shared(tag, info, ptr, false);
}

bool ReadSerializerBackendJson::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	return serialize_class_shared(tag, info, ptr, true);
}

bool ReadSerializerBackendJson::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
	return false;
}

void ReadSerializerBackendJson::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, false);
}

void ReadSerializerBackendJson::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, true);
}

void ReadSerializerBackendJson::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
}

#include "Assets/AssetDatabase.h"
void ReadSerializerBackendJson::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr)
{
	std::string path = "";
	serialize_ar(path);
	if (path.empty())
		ptr = nullptr;
	else
		ptr = loader.load_asset(&info, path);
}
bool ReadSerializerBackendJson::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
{
	std::string path = "";
	bool found = serialize(tag, path);
	if (!found) 
		return false;
	if (path.empty())
		ptr = nullptr;
	else
		ptr = loader.load_asset(&info, path);
	return true;
}

bool ReadSerializerBackendJson::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference)
{
	const bool is_array = !tag.has_value();

	ptr = nullptr;
	// find in dict
	std::string relativepath;
	bool found = true;
	if (is_array)
		serialize_ar(relativepath);
	else
		found = serialize(tag.value(), relativepath);
	if (!found)
		return false;	// not found, false
	if (relativepath.empty())
		return true;	// found so true, but a nullptr

	std::string path;
	if (current_root_path)
		path = unserialize_relative_to_absolute(relativepath.c_str(), current_root_path->c_str());
	else
		path = relativepath;

	//auto find = path_to_objs.find(path);
	//if (find != path_to_objs.end())
	//	ptr = find->second;

	ptr = MapUtil::get_or_null(path_to_objs, path);

	return true;

}

nlohmann::json JsonSerializerUtil::diff_json(const nlohmann::json& a, const nlohmann::json& b) {
	using namespace nlohmann;
	if (a == b) {
		return json();
	}
	// if types differ, or non-object/array, just return b as replacement
	if (a.type() != b.type() || !a.is_structured()) {
		return b;
	}
	json result;
	if (a.is_object()) {
		for (auto it = b.begin(); it != b.end(); ++it) {
			const std::string& key = it.key();
			if (!a.contains(key)) {
				result[key] = it.value();  // new key
			}
			else {
				json subdiff = diff_json(a.at(key), it.value());
				if (!subdiff.is_null() && !(subdiff.is_object() && subdiff.empty())) {
					result[key] = subdiff;
				}
			}
		}
	}
	else if (a.is_array()) {
		// if arrays are different, return full array from b
		if (a != b) {
			result = b;
		}
	}

	return result;
}

nlohmann::json* MakePathForGenericObj::find_diff_for_obj(ClassBase* obj) {
	if (!diff_available) return nullptr;
	MakePathForGenericObj pathmaker(false);
	const ClassBase* diffobj = obj->get_type().default_class_object;
	WriteSerializerBackendJson writer(pathmaker,*const_cast<ClassBase*>(diffobj));
	return new nlohmann::json(*writer.get_root_object());	// FIXME, test
}
