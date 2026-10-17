#pragma once
#include "ReflectionProp.h"

// A property of one object.
class PropertyPtr
{
public:
	PropertyPtr(const PropertyInfo* property, void* inst) : property(property), instance(inst) {}
	const char* get_name() const { return property->name; }
	const PropertyInfo& info() const { return *property; }
	void* get_instance() const { return instance; }

private:
	const PropertyInfo* property = nullptr;
	void* instance = nullptr;
};

// Iterates the properties of a class and its base classes.
class ClassPropPtr
{
public:
	ClassPropPtr(ClassBase* base) : obj(base) {}
	ClassPropPtr(const ClassTypeInfo* ti) : obj(nullptr), ti(ti) {}

	struct Iterator
	{
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
