#pragma once
#include "ReflectionProp.h"

// encapsulates a specific instance of a property
class PropertyPtr;
struct PropertyInfo;
class StructTypeInfo;
class ArrayPropPtr
{
public:
	ArrayPropPtr(const PropertyInfo* info, void* ptr);
	PropertyPtr get_array_index(int index);
	int get_array_size();
	void resize_array(int newsize);
	// iterate over the array
	struct Iterator {
		Iterator(const PropertyInfo* p, void* inst);
		Iterator();
		bool operator!=(const Iterator& other);
		Iterator& operator++();
		PropertyPtr operator*();
		void advance();
		int index = 0;
		const PropertyInfo* p;
		void* inst;
		int count = 0;
	};
	Iterator begin();
	Iterator end();
private:
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};
class StructPropPtr
{
public:
	StructPropPtr(const StructTypeInfo* info, void* ptr);

	template<typename T>
	T* get_struct() const;
	int get_num_properties() const;

	// iterate over properties of struct
	struct Iterator {
		Iterator(ClassBase* obj, bool only_seri);
		Iterator();
		bool operator!=(const Iterator& other);
		Iterator& operator++();
		PropertyPtr operator*();
		void advance();
	};
	Iterator begin();
	Iterator end();
private:
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};

class PropertyPtr
{
public:
	PropertyPtr(const PropertyInfo* property, void* inst)
		: property(property), instance(inst) {}
	bool is_array() const { return property->type == core_type_id::List;  }
	bool is_struct() const { return property->type == core_type_id::ActualStruct; }
	ArrayPropPtr as_array();
	StructPropPtr as_struct();
	//bool is_class() const;
	//bool is_numeric() const;
	//bool is_string() const;
private:
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};