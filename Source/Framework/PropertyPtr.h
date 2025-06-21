#pragma once
#include "ReflectionProp.h"
#include "StructReflection.h"
#include <glm/gtc/quaternion.hpp>


// encapsulates a specific instance of a property
class PropertyPtr;
struct PropertyInfo;

class ArrayPropPtr
{
public:
	ArrayPropPtr(const PropertyInfo* info, void* ptr);
	PropertyPtr get_array_index(int index);
	int get_array_size();
	void resize_array(int newsize);
	// iterate over the array
	struct Iterator {
		Iterator(ArrayPropPtr&);
		bool operator!=(const Iterator& other);
		Iterator& operator++();
		PropertyPtr operator*();
		void advance();
		ArrayPropPtr& owner;
		int index = 0;
		int count = 0;
	};
	Iterator begin();
	Iterator end();
private:
	void* get_ptr() {
		return property->get_ptr(instance);
	}
	const PropertyInfo* get_array_template_property() {
		return property->list_ptr->get_property();
	}
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};
struct Serializer;
class StructPropPtr
{
public:
	StructPropPtr(const PropertyInfo* property, void* ptr);

	template<typename T>
	T* get_struct() const;
	int get_num_properties() const;
	void call_serialize(Serializer& s);

	// iterate over properties of struct
	struct Iterator {
		Iterator(StructPropPtr&);
		bool operator!=(const Iterator& other);
		Iterator& operator++();
		PropertyPtr operator*();
		StructPropPtr& self;
		int index = 0;
	};
	Iterator begin();
	Iterator end();
private:
	const PropertyInfoList& properties() {
		return *property->struct_type->properties;
	}
	void* get_ptr() const {
		return property->get_ptr(instance);
	}
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};
class ClassPropPtr
{
public:
	ClassPropPtr(ClassBase* base)
		: obj(base) {}
	ClassPropPtr(const ClassTypeInfo* ti)
		: obj(nullptr),ti(ti) {}

	struct Iterator {
		Iterator(ClassBase* obj);
		Iterator(const ClassTypeInfo* ti);
		Iterator();
		bool operator!=(const Iterator& other);
		Iterator& operator++();
		PropertyPtr operator*();
		void advance();

		const ClassTypeInfo* info = nullptr;
		int index = -1;
		ClassBase* obj = nullptr;
	};
	Iterator begin();
	Iterator end();
private:
	const ClassTypeInfo* ti = nullptr;
	ClassBase* obj = nullptr;
};
using std::vector;


class EnumPropPtr
{
public:
	int32_t get_as_integer();
	void set_from_integer(int32_t i);
	const char* get_as_string();
	void set_from_string(const char* str);
};


class PropertyPtr
{
public:
	PropertyPtr(const PropertyInfo* property, void* inst)
		: property(property), instance(inst) {}
	const char* get_name() const {
		return property->name;
	}
	bool is_an_array_property() const {
		return *property->name == 0;	// empty name, property belongs to an array
	}
	core_type_id get_type() const { return property->type; }

	bool is_array() const { return property->type == core_type_id::List;  }
	ArrayPropPtr as_array();

	bool is_struct() const { return property->type == core_type_id::ActualStruct; }
	StructPropPtr as_struct();

	bool is_string() const;
	std::string& as_string();

	bool is_float() const;
	float& as_float();

	bool is_numeric() const;
	int64_t get_integer_casted();
	void set_integer_casted(int64_t i);

	bool is_boolean() const;
	bool& as_boolean();

	bool is_vec3() const;
	glm::vec3& as_vec3();

	bool is_vec2() const;
	glm::vec2& as_vec2();

	bool is_quat() const;
	glm::quat& as_quat();

	bool is_enum() const;
	EnumPropPtr as_enum();
	//bool is_class() const;

	void* get_instance_ptr_unsafe() {
		return instance;
	}
	const PropertyInfo* get_property_info() {
		return property;
	}
private:
	void* get_ptr() const {
		return property->get_ptr(instance);
	}
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};

template<typename T>
inline T* StructPropPtr::get_struct() const {
	assert(property && instance);
	if (property->struct_type == &T::StructType)
		return (T*)get_ptr();
	return nullptr;
}
