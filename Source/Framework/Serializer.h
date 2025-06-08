#pragma once
#include "StructReflection.h"
#include <glm/glm.hpp>
#include "PropertyPtr.h"

struct Serializer
{
	STRUCT_BODY();

	Serializer() {}
	virtual bool serialize_dict(const char* tag)=0;
	virtual bool serialize_dict_ar() = 0;
	virtual bool serialize_array(const char* tag, int& size)=0;
	virtual bool serialize_array_ar(int& size) = 0;
	virtual void end_obj()=0;

	virtual bool serialize(const char* tag, bool& b)=0;
	virtual bool serialize(const char* tag, int8_t& i)=0;
	virtual bool serialize(const char* tag, int16_t& i)=0;
	virtual bool serialize(const char* tag, int32_t& i)=0;
	virtual bool serialize(const char* tag, int64_t& i)=0;
	virtual bool serialize(const char* tag, float& f)=0;
	virtual bool serialize(const char* tag, glm::vec3& f)=0;
	virtual bool serialize(const char* tag, glm::vec2& f)=0;
	virtual bool serialize(const char* tag, glm::quat& f)=0;
	virtual bool serialize(const char* tag, std::string& s)=0;

	virtual void serialize_ar(bool& b) = 0;
	virtual void serialize_ar(int8_t& i) = 0;
	virtual void serialize_ar(int16_t& i) = 0;
	virtual void serialize_ar(int32_t& i) = 0;
	virtual void serialize_ar(int64_t& i) = 0;
	virtual void serialize_ar(float& f) = 0;
	virtual void serialize_ar(glm::vec3& f) = 0;
	virtual void serialize_ar(glm::vec2& f) = 0;
	virtual void serialize_ar(glm::quat& f) = 0;
	virtual void serialize_ar(std::string& s) = 0;

	template<typename T>
	Serializer& serialize_struct(const char* tag, T& t);
	template<typename T>
	Serializer& serialize_class(const char* tag, T*& ptr);	// owning
	template<typename T>
	Serializer& serialize_class_reference(const char* tag, T*& ptr);	// non owning
	template<typename T>
	Serializer& serialize_struct_ar(T& t);
	template<typename T>
	Serializer& serialize_class_ar(T*& ptr);	// owning
	template<typename T>
	Serializer& serialize_class_reference_ar(T*& ptr);	// non owning
	
	// Handles both loading and saving
	void serialize_property(PropertyPtr ptr);
	void serialize_property_ar(PropertyPtr ptr);

	virtual void serialize_class(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) {}
	virtual void serialize_class_ar(const ClassTypeInfo& info, ClassBase*& ptr) {}
	virtual void serialize_class_reference(const char* tag, const ClassTypeInfo& info, ClassBase*& ptr) {}
	virtual void serialize_class_reference_ar(const ClassTypeInfo& info, ClassBase*& ptr) {}
	virtual void serialize_enum(const char* tag, const EnumTypeInfo* info, int& i) {}
	virtual void serialize_enum_ar(const EnumTypeInfo* info, int& i) {}
	virtual void serialize_object_handle() {}

	virtual bool is_loading() = 0;
	bool is_saving() { return !is_loading(); }
};


class BaseUpdater;

template<typename T>
inline Serializer& Serializer::serialize_struct(const char* tag, T& t)
{
	serialize_struct(tag, T::StructType, &t);
	return *this;
}

template<typename T>
inline Serializer& Serializer::serialize_class(const char* tag, T*& ptr)
{
	ClassBase* p = ptr;
	serialize_class(tag, T::StaticType, p);
	ptr = (T*)p;
	return *this;
}

template<typename T>
inline Serializer& Serializer::serialize_class_reference(const char* tag, T*& ptr)
{
	serialize_class_reference(tag, T::StaticType, ptr);
	return *this;
}
template<typename T>
inline Serializer& Serializer::serialize_struct_ar(T& t)
{
	serialize_struct(T::StructType, &t);
	return *this;
}

template<typename T>
inline Serializer& Serializer::serialize_class_ar(T*& ptr)
{
	serialize_class(T::StaticType, ptr);
	return *this;
}

template<typename T>
inline Serializer& Serializer::serialize_class_reference_ar(T*& ptr)
{
	serialize_class_reference(T::StaticType, ptr);
	return *this;
}
