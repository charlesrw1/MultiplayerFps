#include "SerializerJson.h"
#include "Assets/IAsset.h"
#include "Framework/MapUtil.h"
#include "Log.h"

const char* const CLASSNAME = "classname";

//extern string serialize_build_relative_path(const char* from, const char* to);
//extern string unserialize_relative_to_absolute(const char* relative, const char* root);

using std::vector;

vector<string> split_path(const string& path, char delimiter='/') {
	vector<string> result;
	size_t start = 0;
	size_t end = 0;
	while ((end = path.find(delimiter, start)) !=  string::npos) {
		if (end != start) // skip empty segments
			result.emplace_back(path.substr(start, end - start));
		start = end + 1;
	}
	// Add last segment if not empty
	if (start < path.length()) {
		result.emplace_back(path.substr(start));
	}
	return result;
}

vector<string> make_relative_path(const vector<string>& from, const vector<string>& to)
{
	size_t common = 0;
	while (common < from.size() && common < to.size() && from[common] == to[common]) {
		++common;
	}
	vector<string> result;
	// Go up from "from" to the common base
	for (size_t i = common; i < from.size(); ++i) {
		result.push_back("..");
	}
	// Go down into "to"
	for (size_t i = common; i < to.size(); ++i) {
		result.push_back(to[i]);
	}

	return result;
}


vector<string> resolve_relative_path(const vector<string>& base, const vector<string>& relative)
{
	vector<string> result = base;

	for (const auto& part : relative) {
		if (part == "..") {
			if (!result.empty()) result.pop_back(); // go up one level
		}
		else if (part != ".") {
			result.push_back(part); // descend into directory or file
		}
	}

	return result;
}

string combine_path(const vector<string>& paths) {
	string out;
	for (auto& p : paths)
		out += p + "/";
	if (!out.empty()) out.pop_back();
	return out;
}


void WriteSerializerBackendJson::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference)
{
	const bool is_array = !tag.has_value();

	if (!ptr) {
		string path = "";
		if (is_array)
			serialize_ar(path);
		else
			serialize(tag.value(), path);
		return;
	}

	//string path = pathmaker.make_path(ptr);
	auto [path, is_subobject] = pathmaker.make_path(ptr);
	vector<string> path_s = split_path(path);
	assert(!path_s.empty());

	string pathtowrite = "";
	if (currently_writing_class) {
		vector<string> parent_s = split_path(pathmaker.make_path(currently_writing_class).path);
		assert(!parent_s.empty());
		if (parent_s.at(0) == path_s.at(0)) {
			auto relative = make_relative_path(parent_s, path_s);
			pathtowrite = combine_path(relative);
		}
	}
	if (pathtowrite.empty())
		pathtowrite = path;


	if (is_array)
		serialize_ar(pathtowrite);
	else
		serialize(tag.value(), pathtowrite);

	if(!is_only_reference)
		write_queue.push_back(ptr);

	if (!MapUtil::contains(paths_to_objects, path)) {
		paths_to_objects[path].o = ptr;
		paths_to_objects[path].is_from_sub_object = is_subobject;
	}
}

void WriteSerializerBackendJson::write_actual_class(ClassBase* ptr, const string& path)
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

WriteSerializerBackendJson::WriteSerializerBackendJson(const char* debug_tag, IMakePathForObject& pathmaker, ClassBase& obj_to_serialize, bool serialize_single_object) 
	: pathmaker(pathmaker), debug_tag(debug_tag),serialize_single_object(serialize_single_object)
{
	if (!serialize_single_object) {
		stack.push_back(&obj);
		{
			ClassBase* ptr = &obj_to_serialize;
			serialize_class("root", ClassBase::StaticType, ptr);
		}
		serialize_dict("objs");
		while (!write_queue.empty()) {
			ClassBase* obj = write_queue.back();
			write_queue.pop_back();
			const string path = pathmaker.make_path(obj).path;
			assert(MapUtil::contains(paths_to_objects, path));
			if (!paths_to_objects.find(path)->second.has_been_written)
				write_actual_class(obj, path);
			paths_to_objects[path].has_been_written = true;
		}
		end_obj();

		int dummysz{};
		serialize_array("paths", dummysz);
		for (auto& o : paths_to_objects) {
			const bool is_valid = o.second.has_been_written && !o.second.is_from_sub_object;
			if (is_valid) {
				serialize_array_ar(dummysz);
				string path = o.first;
				serialize_ar(path);
				auto type = pathmaker.make_type_name(o.second.o);
				serialize_ar(type);
				end_obj();
			}
		}
		end_obj();
	}
	else {
		stack.push_back({ &obj });
		ClassBase* ptr = &obj_to_serialize;
		assert(!currently_writing_class);
		currently_writing_class = ptr;
		string className = ptr->get_type().classname;
		serialize("__classname", className);
		ptr->serialize(*this);
		for (auto p : ClassPropPtr(ptr)) {
			serialize_property(p);
		}
		currently_writing_class = nullptr;
		// now diff it
		auto diff = pathmaker.find_diff_for_obj(ptr);
		if (diff) {
			(*stack.back().ptr) = JsonSerializerUtil::diff_json(*diff, (*stack.back().ptr));
		}
		stack.pop_back();
	}
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
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize_ar(name);
}

bool WriteSerializerBackendJson::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
{
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize(tag, name);
	return true;
}
ReadSerializerBackendJson::ReadSerializerBackendJson(const char* debug_tag, nlohmann::json& json_obj, IMakeObjectFromPath& objmaker, IAssetLoadingInterface& loader)
	:objmaker(objmaker), loader(loader), debug_tag(debug_tag)
{
	this->obj = &json_obj;
	load_shared();
	this->obj = nullptr;
	stack.clear();
}

void ReadSerializerBackendJson::load_shared()
{
	if (obj->contains("__classname")) {
		stack.push_back(obj);
		// load single
		string classname;
		serialize("__classname", classname);
		ClassBase* obj = objmaker.create_from_name(*this, classname, "");
		if (!obj) {
			throw std::runtime_error("ReadSerializerBackendJson::load_shared: no object of name: " + classname);
		}
		this->rootobj = obj;
		assert(!current_root_path);
		string rootPath = "";
		current_root_path = &rootPath;
		obj->serialize(*this);
		for (PropertyPtr property : ClassPropPtr(obj)) {
			try {
				serialize_property(property);
			}
			catch (...) {
				sys_print(Error, "ReadSerializerBackendJson(%s): error serializing property %s for class %s\n", get_debug_tag(), property.get_name(),obj->get_type().classname);
			}
		}
		current_root_path = nullptr;
		stack.pop_back();
		assert(stack.empty());
	}
	else {

		stack.push_back(obj);
		int sz = 0;
		if (serialize_array("paths", sz)) {
			for (int i = 0; i < sz; i++) {
				int count = 0;
				serialize_array_ar(count);
				if (count != 2) {
					LOG_WARN("expected 2-tuple in paths");
					continue;
				}
				string path;
				string type;
				serialize_ar(path);
				serialize_ar(type);
				ClassBase* obj = objmaker.create_from_name(*this, type, path);

				if (!obj) {
					LOG_WARN("null obj from creation");
					// fallthrough
				}

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
			if (!p.second) {
				sys_print(Warning, "ReadSerializerBackendJson(%s): null object (path=%s)\n", get_debug_tag(), p.first.c_str());
				continue;
			}

			if (serialize_dict(p.first.c_str())) {
				assert(!current_root_path);
				current_root_path = &p.first;
				p.second->serialize(*this);
				for (PropertyPtr property : ClassPropPtr(p.second)) {
					try {
						serialize_property(property);
					}
					catch (...) {
						sys_print(Error, "ReadSerializerBackendJson(%s): error serializing property %s for class %s\n", get_debug_tag(), property.get_name(), p.second->get_type().classname);
					}
				}
				current_root_path = nullptr;
				end_obj();
			}
		}
		end_obj();
	}
}

ReadSerializerBackendJson::ReadSerializerBackendJson(const char* debug_tag, const string& text, IMakeObjectFromPath& objmaker, IAssetLoadingInterface& loader)
	:objmaker(objmaker), loader(loader), debug_tag(debug_tag)
{
	auto objStack = nlohmann::json::parse(text);
	this->obj = &objStack;
	load_shared();
	this->obj = nullptr;
	stack.clear();
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
	string path = "";
	serialize_ar(path);
	if (path.empty())
		ptr = nullptr;
	else
		ptr = loader.load_asset(&info, path);
}
bool ReadSerializerBackendJson::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
{
	string path = "";
	bool found = serialize(tag, path);
	if (!found) 
		return false;
	if (path.empty())
		ptr = nullptr;
	else
		ptr = loader.load_asset(&info, path);
	return true;
}
#include <variant>
bool ReadSerializerBackendJson::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference)
{
	const bool is_array = !tag.has_value();

	ptr = nullptr;
	// find in dict
	string relativepath;
	bool found = true;
	if (is_array)
		serialize_ar(relativepath);
	else
		found = serialize(tag.value(), relativepath);
	if (!found)
		return false;	// not found, false
	if (relativepath.empty())
		return true;	// found so true, but a nullptr

	string path;
	if (current_root_path) {
		vector<string> rel_s = split_path(relativepath);
		vector<string> cur_s = split_path(*current_root_path);
		assert(!rel_s.empty());
		if (rel_s.at(0) == "..") {
			path = combine_path(resolve_relative_path(cur_s, rel_s));
		}

		//path = unserialize_relative_to_absolute(relativepath.c_str(), current_root_path->c_str());
	}
	if (path.empty()) {
		path = relativepath;
	}
	//else
	//	path = relativepath;

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
			const string& key = it.key();
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
#include "SerializedForDiffing.h"
nlohmann::json* MakePathForGenericObj::find_diff_for_obj(ClassBase* obj) {
	if (!diff_available) 
		return nullptr;
	if (obj->get_type().diff_data)
		return &obj->get_type().diff_data->jsonObj;
	else
		return nullptr;
}
void ReadSerializerBackendJson::insert_nested_object(string path, ClassBase* obj)
{
	//LOG_MSG("inserted nested obj");
	MapUtil::insert_test_exists(path_to_objs, path, obj);
	MapUtil::insert_test_exists(obj_to_path, obj, path);
}