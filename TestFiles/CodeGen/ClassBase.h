#pragma once
#include <cstdint>

class PropertyInfo;
struct PropertyInfoList
{
	PropertyInfo* p = nullptr;
	int count = 0;
};

struct InterfaceTypeInfo {
	int id = 0;
	const char* name = "";
};

class Serializer;
class ClassBase;
struct PropHashTable;
struct ClassTypeInfo
{
public:
	typedef ClassBase* (*CreateObjectFunc)();
	typedef PropertyInfoList (*GetPropsFunc)();

	ClassTypeInfo(const char* classname, 
		const ClassTypeInfo* super_typeinfo, 
		GetPropsFunc get_prop_func, 
		CreateObjectFunc alloc,
		bool create_default_obj
	);

	int id = 0;
	int last_child = 0;
	const char* classname = "";
	const char* superclassname = "";
	CreateObjectFunc allocate =nullptr;
	const ClassTypeInfo* super_typeinfo = nullptr;
	// store this, get_props might rely on other Class's typinfo being registered, so call these after init
	GetPropsFunc get_props_funct = nullptr;
	PropertyInfoList props;
	bool has_serialize = false;


	// an allocated object of the same type
	// use for default props etc.
	// not every class type will have this
	const ClassBase* default_class_object = nullptr;
	// opaque pointer to hash table for props
	const PropHashTable* prop_hash_table = nullptr;

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
};

typedef void(*serialize_struct_func)(void*, Serializer& s);
class StructTypeInfo
{
public:
	const char* structname = "";
	serialize_struct_func serialize_func = nullptr;
	PropertyInfoList properties;
};



#if 0
#define CLASS_IMPL(classname) \
ClassTypeInfo classname::StaticType = ClassTypeInfo( \
	#classname, \
	&classname::SuperClassType::StaticType, \
	get_get_props_if_it_exists<classname>(), \
	default_class_create<classname>(), \
	classname::CreateDefaultObject \
);

#define CLASS_H_EXPLICIT_SUPER(classname, cpp_supername, reflected_super) \
class classname : public cpp_supername { \
public: \
	using MyClassType = classname; \
	using SuperClassType = reflected_super; \
	static ClassTypeInfo StaticType; \
	const ClassTypeInfo& get_type() const override { return classname::StaticType; }

#define CLASS_H(classname, supername) \
	CLASS_H_EXPLICIT_SUPER(classname, supername, supername)
#endif

struct ClassTypeIterator
{
	ClassTypeIterator(ClassTypeInfo* ti);
	ClassTypeIterator& next();
	bool is_end() const;
	const ClassTypeInfo* get_type() const;
private:
	int index = 0;
	int end = 0;
};

class ClassBase
{
public:
	static ClassTypeInfo StaticType;
	const static bool CreateDefaultObject;	/* = false, default setting */

	virtual ~ClassBase() {
	}

	virtual const ClassTypeInfo& get_type() const;
	virtual void serialize(Serializer& s) {}	// custom serialization

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
	ClassBase* create_copy(ClassBase* userptr = nullptr);
public:
	// called by ClassTypeInfo only during static init
	static void register_class(ClassTypeInfo* cti);
	// called in main() after all classes have been reg'd
	static void init_class_reflection_system();
	// find a ClassTypeInfo by classname string
	static const ClassTypeInfo* find_class(const char* classname);
	// find a ClassTypeInfo by integer id
	static const ClassTypeInfo* find_class(int32_t id);

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
	static T* create_class(int16_t id) {
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

	static ClassTypeIterator get_subclasses(const ClassTypeInfo* typeinfo) {
		return ClassTypeIterator((ClassTypeInfo*)typeinfo/* remove const here, doesnt matter tho*/);
	}
};