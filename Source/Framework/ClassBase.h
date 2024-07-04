#pragma once
#include <cstdint>
#include "Framework/TypedVoidPtr.h"

struct PropertyInfoList;
class ClassBase;

struct ClassTypeInfo
{
public:
	typedef ClassBase* (*CreateObjectFunc)();

	ClassTypeInfo(const char* classname, const char* superclass, const PropertyInfoList* props, CreateObjectFunc alloc);

	uint16_t id = 0;
	uint16_t last_child = 0;
	const char* classname = "";
	const char* superclassname = "";
	ClassBase*(*allocate)()=nullptr;
	const PropertyInfoList* props = nullptr;
	const ClassTypeInfo* super_typeinfo = nullptr;

	template<typename T>
	static ClassBase* default_allocate() {
		return (ClassBase*)(new T);
	}

	bool is_a(const ClassTypeInfo& other) const {
		return id >= other.id && id <= other.last_child;
	}
	bool operator==(const ClassTypeInfo& other) const {
		return id == other.id;
	}

private:

};

#define CLASS_HEADER() \
static ClassTypeInfo StaticType; \
const ClassTypeInfo& get_type() const override;

#define CLASS_IMPL(classname, supername) \
ClassTypeInfo classname::StaticType = ClassTypeInfo(#classname, #supername, classname::get_props(),ClassTypeInfo::default_allocate<classname>); \
const ClassTypeInfo& classname::get_type() const  { return classname::StaticType; }
#define ABSTRACT_CLASS_IMPL(classname, supername) \
ClassTypeInfo classname::StaticType = ClassTypeInfo(#classname, #supername, classname::get_props(), nullptr); \
const ClassTypeInfo& classname::get_type() const  { return classname::StaticType; }

#define CLASS_IMPL_NO_PROPS(classname, supername) \
ClassTypeInfo classname::StaticType = ClassTypeInfo(#classname, #supername, nullptr,ClassTypeInfo::default_allocate<classname>); \
const ClassTypeInfo& classname::get_type() const  { return classname::StaticType; }

#define ABSTRACT_CLASS_IMPL_NO_PROPS(classname, supername) \
ClassTypeInfo classname::StaticType = ClassTypeInfo(#classname, #supername, nullptr, nullptr); \
const ClassTypeInfo& classname::get_type() const  { return classname::StaticType; }

struct ClassTypeIterator
{
	ClassTypeIterator(ClassTypeInfo* ti) {
		if (ti) {
			index = ti->id;
			end = ti->last_child + 1;
		}
	}
	ClassTypeIterator& next() { index++; return *this; }
	bool is_end() const { return index >= end; }
	const ClassTypeInfo* get_type() const;
private:
	uint16_t index = 0;
	uint16_t end = 0;
};

class ClassBase
{
public:
	static ClassTypeInfo StaticType;
	virtual const ClassTypeInfo& get_type() const;

	// cast this class to type T, returns nullptr if failed
	template<typename T>
	const T* cast_to() const {
		return (is_a<T>() ? static_cast<const T*>(this) : nullptr);
	}
	template<typename T>
	T* cast_to() {
		return (is_a<T>() ? static_cast<T*>(this) : nullptr );
	}

	// is this class a subclass or an instance of type T
	template<typename T>
	bool is_a() const {
		return get_type().is_a(T::StaticType);
	}

	// creates a copy of class and copies serializable fields
	ClassBase* create_copy(TypedVoidPtr userptr = TypedVoidPtr());
public:
	// called by ClassTypeInfo only during static init
	static void register_class(ClassTypeInfo* cti);

	// called in main() after all classes have been reg'd
	static void init();
	
	// find a ClassTypeInfo by classname string
	static const ClassTypeInfo* find_class(const char* classname);
	// find a ClassTypeInfo by integer id
	static const ClassTypeInfo* find_class(uint16_t id);

	static bool does_class_exist(const char* classname) {
		return find_class(classname) != nullptr;
	}

	// allocate a class by string
	template<typename T>
	static T* create_class(const char* classname) {
		auto classinfo = find_class(classname);
		if (!classinfo || !classinfo->allocate || !classinfo->is_a(T::StaticType))
			return nullptr;
		ClassBase* base = classinfo->allocate();
		return static_cast<T*>(base);
	}
	// allocate by id
	template<typename T>
	static T* create_class(uint16_t id) {
		auto classinfo = find_class(id);
		if (!classinfo || !classinfo->allocate || !classinfo->is_a(T::StaticType))
			return nullptr;
		ClassBase* base = classinfo->allocate();
		return static_cast<T*>(base);
	}

	// get all subclasses to a class excluding abstract ones
	template<typename T>
	static ClassTypeIterator get_subclasses() {
		return ClassTypeIterator(&T::StaticType);
	}
};