#include "SerializerBinary.h"

BinaryWriterBackend::BinaryWriterBackend(ClassBase& rootObj)
{
	write_class(rootObj);
	for (auto&[obj,index] : classToIndex) {
		if(obj)
			write_class(*obj);
	}

}
void BinaryWriterBackend::write_class(ClassBase& b)
{
	b.serialize(*this);
	for (auto p : ClassPropPtr(&b)) {
		serialize_property(p);
	}
}

bool BinaryWriterBackend::serialize_dict(const char* tag)
{
	// nop
	return true;
}

bool BinaryWriterBackend::serialize_dict_ar()
{
	// nop
	return true;
}

bool BinaryWriterBackend::serialize_array(const char* tag, int& size)
{
	writer.write_int32(size);
	return true;
}

bool BinaryWriterBackend::serialize_array_ar(int& size)
{
	writer.write_int32(size);
	return true;
}

void BinaryWriterBackend::end_obj()
{
	// nop
}

bool BinaryWriterBackend::serialize(const char* tag, bool& b)
{
	writer.write_byte(b);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, int8_t& i)
{
	writer.write_byte(i);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, int16_t& i)
{
	writer.write_int16(i);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, int32_t& i)
{
	writer.write_int32(i);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, int64_t& i)
{
	writer.write_int64(i);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, float& f)
{
	writer.write_float(f);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, glm::vec3& f)
{
	writer.write_float(f.x);
	writer.write_float(f.y);
	writer.write_float(f.z);
	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, glm::vec2& f)
{
	writer.write_float(f.x);
	writer.write_float(f.y);

	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, glm::quat& f)
{
	writer.write_float(f.x);
	writer.write_float(f.y);
	writer.write_float(f.z);
	writer.write_float(f.w);

	return true;
}

bool BinaryWriterBackend::serialize(const char* tag, std::string& s)
{
	writer.write_string(s);
	return true;
}

void BinaryWriterBackend::serialize_ar(bool& b)
{
	BinaryWriterBackend::serialize("", b);
}

void BinaryWriterBackend::serialize_ar(int8_t& i)
{
	BinaryWriterBackend::serialize("", i);
}

void BinaryWriterBackend::serialize_ar(int16_t& i)
{
	BinaryWriterBackend::serialize("", i);
}

void BinaryWriterBackend::serialize_ar(int32_t& i)
{
	BinaryWriterBackend::serialize("", i);
}

void BinaryWriterBackend::serialize_ar(int64_t& i)
{
	BinaryWriterBackend::serialize("", i);
}

void BinaryWriterBackend::serialize_ar(float& f)
{
	BinaryWriterBackend::serialize("", f);
}

void BinaryWriterBackend::serialize_ar(glm::vec3& f)
{
	BinaryWriterBackend::serialize("", f);
}

void BinaryWriterBackend::serialize_ar(glm::vec2& f)
{
	BinaryWriterBackend::serialize("", f);
}

void BinaryWriterBackend::serialize_ar(glm::quat& f)
{
	BinaryWriterBackend::serialize("", f);
}

void BinaryWriterBackend::serialize_ar(std::string& s)
{
	BinaryWriterBackend::serialize("", s);
}
#include "Framework/MapUtil.h"
bool BinaryWriterBackend::serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	int index = 0;
	if (MapUtil::contains(classToIndex, ptr)) {
		index= MapUtil::get_or(classToIndex, ptr, 0);
	}
	else {
		index = curIndex++;
		classToIndex.insert({ ptr,(index-1) });
	}
	writer.write_int32(index);

	return true;
}

void BinaryWriterBackend::serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	BinaryWriterBackend::serialize_class("", info, ptr);
}

bool BinaryWriterBackend::serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr)
{
	BinaryWriterBackend::serialize_class("", info, ptr);
	return true;
}

void BinaryWriterBackend::serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr)
{
	BinaryWriterBackend::serialize_class("", info, ptr);
}

bool BinaryWriterBackend::serialize_enum(const char* tag, const EnumTypeInfo* info, int& i)
{
	BinaryWriterBackend::serialize("",i);
	return true;
}

void BinaryWriterBackend::serialize_enum_ar(const EnumTypeInfo* info, int& i)
{
	BinaryWriterBackend::serialize("", i);

}
#include "Assets/IAsset.h"
bool BinaryWriterBackend::serialize_asset(const char* tag, const ClassTypeInfo& type, IAsset*& ptr)
{
	if (ptr) {
		writer.write_string(ptr->get_name());
	}
	else {
		writer.write_string("");
	}
	return true;
}

void BinaryWriterBackend::serialize_asset_ar(const ClassTypeInfo& type, IAsset*& ptr)
{
	BinaryWriterBackend::serialize_asset("", type, ptr);
}

bool BinaryWriterBackend::is_loading()
{
	return false;
}

const char* BinaryWriterBackend::get_debug_tag()
{
	return "writer";
}
