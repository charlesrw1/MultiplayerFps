#pragma once
#include "Serializer.h"
#include <json.hpp>
#include <optional>	// just to try it out :)
#include "glm/gtc/quaternion.hpp"
class ReadSerializerBackendJson;
using std::unordered_map;
using std::string;
template<typename T>
using opt = std::optional<T>;

struct JsonStack {
	JsonStack(nlohmann::json* j) : ptr(j) {}
	nlohmann::json* ptr = nullptr;
	int arr_idx = 0;
};

// The writer version
class IMakePathForObject {
public:

	virtual std::string make_path(const ClassBase* toobj)=0;	// unique file id along with prefab nesting
	virtual std::string make_type_name(ClassBase* obj) = 0;	// returns if prefab owner or not
	virtual nlohmann::json* find_diff_for_obj(ClassBase* obj) = 0;
};
// The reader version
class IMakeObjectFromPath {
public:
	virtual ClassBase* create_from_name(ReadSerializerBackendJson& s, const std::string& str, const string& parent_path) = 0;
};

class MakePathForGenericObj : public IMakePathForObject {
public:
	MakePathForGenericObj(bool diffable=true) 
		: diff_available(diffable) {}
	std::string make_path(const ClassBase* toobj) final {
		return "x"+std::to_string(uintptr_t(toobj));
	}
	std::string make_type_name(ClassBase* obj) final {
		return obj->get_type().classname;
	}
	nlohmann::json* find_diff_for_obj(ClassBase* obj) final;
private:
	bool diff_available = false;
};
class MakeObjectFromPathGeneric : public IMakeObjectFromPath {
public:
	ClassBase* create_from_name(ReadSerializerBackendJson& s, const std::string& str, const string& parent_path) final {
		return ClassBase::create_class<ClassBase>(str.c_str());
	}
};

class WriteSerializerBackendJson : public Serializer
{
public:
	WriteSerializerBackendJson(IMakePathForObject& pathmaker, ClassBase& obj_to_serialize);

	nlohmann::json* get_root_object() {
		nlohmann::json* objs = get_objects_object();
		if (!objs) return nullptr;
		if (!obj.contains("root")) return nullptr;
		std::string rootpath = obj["root"];
		if (!objs->contains(rootpath)) return nullptr;
		return &(*objs)[rootpath];
	}
	nlohmann::json* get_objects_object() {
		if (!obj.contains("objs")) return nullptr;
		return &obj["objs"];
	}
	nlohmann::json& get_output() {
		return obj;
	}
	void serialize_ar(bool& b) final {
		write_to_array(b);
	}
	void serialize_ar(int8_t& i) final {
		write_to_array(i);
	}
	void serialize_ar(int16_t& i) final {
		write_to_array(i);
	}
	void serialize_ar(int32_t& i) final {
		write_to_array(i);
	}
	void serialize_ar(int64_t& i) final {
		write_to_array(i);
	}
	void serialize_ar(float& f) final {
		write_to_array(f);
	}
	bool serialize(const char* tag, glm::vec3& v) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			vec[0] = v.x;
			vec[1] = v.y;
			vec[2] = v.z;
		}
		return true;
	}
	bool serialize(const char* tag, glm::vec2& v) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			vec[0] = v.x;
			vec[1] = v.y;
		}
		return true;
	}
	bool serialize(const char* tag, glm::quat& q) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			vec[0] = q.w;
			vec[1] = q.x;
			vec[2] = q.y;
			vec[3] = q.z;
		}
		return true;
	}
	void serialize_ar(glm::vec3& v) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			vec[0]=v.x;
			vec[1]=v.y;
			vec[2]=v.z;
		}
	}
	void serialize_ar(glm::vec2& v) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			vec[0]=v.x;
			vec[1]=v.y;
		}
	}
	void serialize_ar(glm::quat& q) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			vec[0]=q.w;
			vec[1]=q.x;
			vec[2]=q.y;
			vec[3]=q.z;
		}
	}

	void serialize_ar(std::string& s) final {
		write_to_array(s);
	}
	bool serialize(const char* tag, bool& b) final {
		return write_to_dict(tag, b);
	}
	bool serialize(const char* tag, float& f) final {
		return write_to_dict(tag, f);
	}
	bool serialize(const char* tag, int& i) final {
		return write_to_dict(tag, i);
	}
	bool serialize(const char* tag, int8_t& i) final {
		return write_to_dict(tag, i);
	}
	bool serialize(const char* tag, int16_t& i) final {
		return write_to_dict(tag, i);
	}
	bool serialize(const char* tag, int64_t& i) final {
		return write_to_dict(tag, i);
	}

	bool serialize(const char* tag, std::string& str) final {
		return write_to_dict(tag, str);
	}
	bool serialize_dict(const char* tag) final {
		(*get_back().ptr)[tag] = nlohmann::json::object();
		auto newdict = &(*get_back().ptr)[tag];
		stack.push_back(newdict);
		return true;
	}
	bool serialize_array(const char* tag, int& sz) final {
		(*get_back().ptr)[tag] = nlohmann::json::array();
		auto newarray =  &(*get_back().ptr)[tag];
		stack.push_back(newarray);
		return true;
	}
	bool serialize_dict_ar() final {
		get_back().ptr->push_back(nlohmann::json::object());
		auto newdict = &get_back().ptr->back();
		stack.push_back(newdict);
		return true;
	}
	bool serialize_array_ar(int& sz) final {
		(get_back()).ptr->push_back(nlohmann::json::array());
		auto newarray =  &get_back().ptr->back();
		stack.push_back(newarray);
		return true;
	}

	void end_obj() final {
		stack.pop_back();
	}

	bool is_loading() final {
		return false;
	}

	bool serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) final;
	bool serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) final;
	bool serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) final;
	void serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) final;
	void serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) final;
	void serialize_enum_ar(const EnumTypeInfo* info, int& i) final;
	void serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr) final;
	bool serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr) final;

private:
	template<typename T>
	void write_to_array(T& t) {
		JsonStack& s = get_back();
		(*s.ptr)[s.arr_idx++] = t;
	}
	template<typename T>
	bool write_to_dict(const char* tag, T& t) {
		JsonStack& s = get_back();
		(*s.ptr)[tag] = t;
		return true;
	}
	JsonStack& get_back() {
		assert(!stack.empty());
		return stack.back();
	}
	nlohmann::json& get_json(const char* tag) {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		backptr[tag] = nlohmann::json::array();
		return backptr[tag];
	}
	nlohmann::json& get_json_ar() {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		backptr[back.arr_idx] = nlohmann::json::array();
		return backptr[back.arr_idx++];
	}

	void serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference);
	void write_actual_class(ClassBase* o, const std::string& path);

	struct Object {
		ClassBase* o = nullptr;
		bool has_been_written = false;
		bool is_from_sub_object = false;	// dont write out a new statement
	};
	std::unordered_map<std::string, Object> paths_to_objects;
	std::vector<ClassBase*> write_queue;
	ClassBase* currently_writing_class = nullptr;

	IMakePathForObject& pathmaker;
	std::vector<JsonStack> stack;
	nlohmann::json obj;
};

class IAssetLoadingInterface;
class ReadSerializerBackendJson : public Serializer
{
public:
	ReadSerializerBackendJson(const std::string& text, IMakeObjectFromPath& objmaker, IAssetLoadingInterface& loader);
	
	// expects a second file
	void insert_nested_object(string path, ClassBase* obj);

	unordered_map<string, ClassBase*> path_to_objs;
	unordered_map<ClassBase*, string> obj_to_path;

	ClassBase* get_root_obj() {
		return rootobj;
	}

	ClassBase* find_object_for_path(const string& path) {
		auto find = path_to_objs.find(path);
		if (find == path_to_objs.end())
			return nullptr;
		return find->second;
	}
	const string* get_path_for_object(ClassBase& object) {
		auto find = obj_to_path.find(&object);
		if (find == obj_to_path.end())
			return nullptr;
		return &find->second;
	}

	bool serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) final;
	bool serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) final;
	bool serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) final;
	void serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) final;
	void serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) final;
	void serialize_enum_ar(const EnumTypeInfo* info, int& i) final;
	void serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr) final;
	bool serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr) final;

	void serialize_ar(bool& b) final {
		read_from_array(b);
	}
	void serialize_ar(int8_t& i) final {
		read_from_array(i);
	}
	void serialize_ar(int16_t& i) final {
		read_from_array(i);
	}
	void serialize_ar(int32_t& i) final {
		read_from_array(i);
	}
	void serialize_ar(int64_t& i) final {
		read_from_array(i);
	}
	void serialize_ar(float& f) final {
		read_from_array(f);
	}
	void serialize_ar(std::string& s) final {
		read_from_array(s);
	}
	bool serialize(const char* tag, bool& b) final {
		return read_from_dict(tag, b);
	}
	bool serialize(const char* tag, float& f) final {
		return read_from_dict(tag, f);
	}
	bool serialize(const char* tag, int& i) final {
		return read_from_dict(tag, i);
	}
	bool serialize(const char* tag, int8_t& i) final {
		return read_from_dict(tag, i);
	}
	bool serialize(const char* tag, int16_t& i) final {
		return read_from_dict(tag, i);
	}
	bool serialize(const char* tag, int64_t& i) final {
		return read_from_dict(tag, i);
	}

	bool serialize(const char* tag, glm::vec3& v) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			v.x = vec[0];
			v.y = vec[1];
			v.z = vec[2];
			return true;
		}
		return false;
	}
	bool serialize(const char* tag, glm::vec2& v) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			v.x = vec[0];
			v.y = vec[1];
			return true;
		}
		return false;
	}
	bool serialize(const char* tag, glm::quat& q) final {
		auto& vec = get_json(tag);
		if (vec.is_array()) {
			q.w = vec[0];
			q.x = vec[1];
			q.y = vec[2];
			q.z = vec[3];
			return true;
		}
		return false;
	}
	void serialize_ar(glm::vec3& v) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			v.x = vec[0];
			v.y = vec[1];
			v.z = vec[2];
		}
	}
	void serialize_ar(glm::vec2& v) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			v.x = vec[0];
			v.y = vec[1];
		}
	}
	void serialize_ar(glm::quat& q) final {
		auto& vec = get_json_ar();
		if (vec.is_array()) {
			q.w = vec[0];
			q.x = vec[1];
			q.y = vec[2];
			q.z = vec[3];
		}
	}

	bool serialize(const char* tag, std::string& str) final {
		return read_from_dict(tag, str);
	}
	bool serialize_dict(const char* tag) final {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		if (!backptr.contains(tag))
			return false;
		stack.push_back(&backptr[tag]);
		return true;
	}
	bool serialize_array(const char* tag, int& sz) final {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		if (!backptr.contains(tag))
			return false;
		sz = backptr[tag].size();
		stack.push_back(&backptr[tag]);
		return true;
	}
	bool serialize_dict_ar() final {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		if (backptr.size()<= back.arr_idx)
			return false;
		stack.push_back(&backptr[back.arr_idx++]);
		return true;
	}
	bool serialize_array_ar(int& sz) final {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		if (backptr.size() <= back.arr_idx)
			return false;
		sz =backptr[back.arr_idx].size();
		stack.push_back(&backptr[back.arr_idx++]);
		return true;
	}
	void end_obj() final {
		stack.pop_back();
	}

	bool is_loading() final {
		return true;
	}
private:
	nlohmann::json& get_json(const char* tag) {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		return backptr[tag];
	}
	nlohmann::json& get_json_ar() {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		return backptr[back.arr_idx++];
	}
	JsonStack& get_back() {
		return stack.back();
	}
	template<typename T>
	void read_from_array(T& t) {
		auto& back = get_back();
		auto& backptr = *back.ptr;

		t = backptr[back.arr_idx++];
	}
	template<typename T>
	bool read_from_dict(const char* tag, T& t) {
		auto& back = get_back();
		auto& backptr = *back.ptr;
		if (backptr.contains(tag)) {
			t = backptr[tag];
			return true;
		}
		return false;
	}

	bool serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference);

	const std::string* current_root_path = nullptr;
	IAssetLoadingInterface& loader;
	ClassBase* rootobj = nullptr;
	IMakeObjectFromPath& objmaker;
	std::vector<JsonStack> stack;
	nlohmann::json obj;
};

class JsonSerializerUtil
{
public:
	// takes in 2 objects A,B, and returns the diff such that B = A + diff
	static nlohmann::json diff_json(const nlohmann::json& a, const nlohmann::json& b);
};