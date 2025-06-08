#include "SerializerJson.h"

const char* const CLASSNAME = "classname";

void WriteSerializerBackendJson::write_actual_class(ClassBase* ptr, const std::string& path)
{
	assert(ptr);
	serialize_dict(path.c_str());
	ptr->serialize(*this);
	for (auto p : ClassPropPtr(ptr)) {
		serialize_property(p);
	}
	end_obj();
}

WriteSerializerBackendJson::WriteSerializerBackendJson(IMakePathForObject* pathmaker, ClassBase* obj_to_serialize) {
	stack.push_back(&obj);
	this->pathmaker = pathmaker;

	serialize_class("root", ClassBase::StaticType, obj_to_serialize);
	serialize_dict("objs");
	while (!write_queue.empty()) {
		ClassBase* obj = write_queue.back();
		write_queue.pop_back();
		auto path = pathmaker->make_path(obj);
		write_actual_class(obj,path);
		assert(paths_to_objects.find(path) != paths_to_objects.end());
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
			auto type = pathmaker->make_type_name(o.second.o);
			serialize_ar(type);
			end_obj();
		}
	}
	end_obj();
}

void WriteSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	if (!ptr) {
		std::string path = "";
		serialize(tag, path);
		return;
	}

	auto path = pathmaker->make_path(ptr);
	write_queue.push_back(ptr);
	serialize(tag, path);
	paths_to_objects[path] = { ptr };

	//assert(ptr);
	//assert(ptr->get_type().is_a(info));
	//serialize_dict(tag);
	//std::string n = info.classname;
	//serialize(CLASSNAME,n);
	//for (auto p : ClassPropPtr(ptr)) {
	//	serialize_property(p);
	//}
	//end_obj();
}

void WriteSerializerBackendJson::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{

}

void WriteSerializerBackendJson::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
}

void WriteSerializerBackendJson::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
}

void WriteSerializerBackendJson::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
}

void WriteSerializerBackendJson::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
}

ReadSerializerBackendJson::ReadSerializerBackendJson(const std::string& text, IMakePathForObject* pathmaker) {
	this->pathmaker = pathmaker;
	this->obj = nlohmann::json::parse(text);
	stack.push_back(&obj);

	int sz = 0;
	if (serialize_array("paths",sz)) {
		for (int i = 0; i < sz; i++) {
			int count = 0;
			serialize_array_ar(count);
			assert(count == 2);//fixme
			std::string path;
			std::string type;
			serialize_ar(path);
			serialize_ar(type);
			auto obj = pathmaker->create_from_name(*this, type);
			path_to_objs.insert({ path,obj });
			end_obj();
		}
		end_obj();
	}
	
	serialize_class("root", ClassBase::StaticType, rootobj);

	serialize_dict("objs");
	for (auto& p : path_to_objs) {
		if (p.second && serialize_dict(p.first.c_str())) {
			p.second->serialize(*this);
			for (auto p : ClassPropPtr(p.second)) {
				serialize_property(p);
			}
			end_obj();
		}
	}
	end_obj();

}

void ReadSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	ptr = nullptr;
	// find in dict
	std::string path;
	if (serialize(tag, path) && !path.empty()) {	// empty path is null
		auto find = path_to_objs.find(path);
		if (find != path_to_objs.end())
			ptr = find->second;
	}


	//bool b = serialize_dict(tag);
	//if (b) {
	//	std::string type;
	//	serialize(CLASSNAME, type);
	//	auto created_class = ClassBase::create_class<ClassBase>(type.c_str());
	//	if (created_class) {
	//		for (auto p : ClassPropPtr(created_class)) {
	//			serialize_property(p);
	//		}
	//	}
	//	ptr = created_class;
	//	end_obj();
	//}
}

