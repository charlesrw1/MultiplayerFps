#include "SerializerJson2.h"
#include "Assets/IAsset.h"
#include "Framework/MapUtil.h"
#include "Log.h"
#include "SerializedForDiffing.h"
const char* const CLASSNAME = "classname";

//extern string serialize_build_relative_path(const char* from, const char* to);
//extern string unserialize_relative_to_absolute(const char* relative, const char* root);

using std::vector;

void WriteSerializerBackendJson2::serialize_class_shared(opt<const char*> tag, const ClassTypeInfo& info, ClassBase*& ptr, bool is_only_reference)
{
	return;
	ASSERT(0);
}

void WriteSerializerBackendJson2::write_actual_class(ClassBase* ptr, const string& path)
{
	return;
	ASSERT(0);
}

WriteSerializerBackendJson2::WriteSerializerBackendJson2(const char* debug_tag,  ClassBase& obj_to_serialize)
	: debug_tag(debug_tag)
{
	stack.push_back({ &obj });
	ClassBase* ptr = &obj_to_serialize;
	assert(!currently_writing_class);
	currently_writing_class = ptr;
	string className = ptr->get_type().classname;
	//serialize("__classname", className);
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

bool WriteSerializerBackendJson2::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(tag, info, ptr, false);
	return true;
}

bool WriteSerializerBackendJson2::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(tag, info, ptr, true);
	return true;
}

bool WriteSerializerBackendJson2::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
	return false;
}

void WriteSerializerBackendJson2::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, false);
}

void WriteSerializerBackendJson2::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	serialize_class_shared(std::nullopt, info, ptr, true);
}

void WriteSerializerBackendJson2::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
}

void WriteSerializerBackendJson2::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr)
{
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize_ar(name);
}

bool WriteSerializerBackendJson2::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
{
	string name = "";
	if (ptr)
		name = ptr->get_name();
	serialize(tag, name);
	return true;
}
ReadSerializerBackendJson2::ReadSerializerBackendJson2(const char* debug_tag, nlohmann::json& json_obj, IAssetLoadingInterface& loader, ClassBase& obj)
	: loader(loader), debug_tag(debug_tag), rootobj(obj)
{
	this->obj = &json_obj;
	load_shared();
	this->obj = nullptr;
	stack.clear();
}

void ReadSerializerBackendJson2::load_shared()
{
	stack.push_back(obj);

	rootobj.serialize(*this);
	for (PropertyPtr property : ClassPropPtr(&rootobj)) {
		try {
			serialize_property(property);
		}
		catch (...) {
			sys_print(Error, "ReadSerializerBackendJson2(%s): error serializing property %s for class %s\n", get_debug_tag(), property.get_name(), rootobj.get_type().classname);
		}
	}
	stack.pop_back();
	assert(stack.empty());
}


bool ReadSerializerBackendJson2::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	return false;
}

bool ReadSerializerBackendJson2::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	return false;
}

bool ReadSerializerBackendJson2::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
	return false;
}

void ReadSerializerBackendJson2::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	return;
}

void ReadSerializerBackendJson2::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	return;
}

void ReadSerializerBackendJson2::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
}

#include "Assets/AssetDatabase.h"
void ReadSerializerBackendJson2::serialize_asset_ar(const ClassTypeInfo& info, IAsset*& ptr)
{
	string path = "";
	serialize_ar(path);
	if (path.empty())
		ptr = nullptr;
	else
		ptr = loader.load_asset(&info, path);
}
bool ReadSerializerBackendJson2::serialize_asset(const char* tag, const ClassTypeInfo& info, IAsset*& ptr)
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


