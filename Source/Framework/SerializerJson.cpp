#include "SerializerJson.h"

void WriteSerializerBackendJson::serialize_struct(const char* tag, const StructTypeInfo& info, void* ptr)
{
	// create dict with tag
	// for each proeprty, serialize property
	// end dict
}
const char* const CLASSNAME = "classname";

void WriteSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	assert(ptr);
	assert(ptr->get_type().is_a(info));
	serialize_dict(tag);
	std::string n = info.classname;
	serialize(CLASSNAME,n);
	for (auto p : ClassPropPtr(ptr)) {
		serialize_property(p);
	}
	end_obj();
}

void WriteSerializerBackendJson::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{

}

void WriteSerializerBackendJson::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
}

void WriteSerializerBackendJson::serialize_struct_ar(const StructTypeInfo& info, void* ptr)
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

void ReadSerializerBackendJson::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	bool b = serialize_dict(tag);
	if (b) {
		std::string type;
		serialize(CLASSNAME, type);
		auto created_class = ClassBase::create_class<ClassBase>(type.c_str());
		if (created_class) {
			for (auto p : ClassPropPtr(created_class)) {
				serialize_property(p);
			}
		}
		ptr = created_class;
		end_obj();
	}
}
