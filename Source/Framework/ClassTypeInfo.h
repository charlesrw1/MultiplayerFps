#pragma once
#include "ClassBase.h"
#include "Reflection2.h"
#include <string>

struct SerializedForDiffing;
struct PropHashTable;
class ScriptManager;
struct FunctionInfo;
class ClassTypeInfo : public ClassBase {
public:
	CLASS_BODY(ClassTypeInfo);

	typedef ClassBase* (*CreateObjectFunc)(const ClassTypeInfo*);
	typedef const PropertyInfoList* (*GetPropsFunc_t)();

	ClassTypeInfo(const char* classname,
		const ClassTypeInfo* super_typeinfo,
		GetPropsFunc_t get_prop_func,
		CreateObjectFunc alloc,
		bool create_default_obj,
		const FunctionInfo* func_infos,
		int func_info_count,
		CreateObjectFunc scriptable_alloc=nullptr,
		bool is_lua_obj=false
	);
	~ClassTypeInfo();

	int32_t id = 0;
	int32_t last_child = 0;
	const char* classname = "";
	const char* superclassname = "";
	const PropertyInfoList* props = nullptr;
	const ClassTypeInfo* super_typeinfo = nullptr;

	// store this, get_props might rely on other Class's typinfo being registered, so call these after init
	GetPropsFunc_t get_props_function = nullptr;

	// an allocated object of the same type
	// use for default props etc.
	// not every class type will have this
	const ClassBase* default_class_object = nullptr;
	std::unique_ptr<SerializedForDiffing> diff_data;
	const FunctionInfo* lua_functions = nullptr;
	int lua_function_count = 0;
	// allocates a scriptable version of this object
	// it overrides virtal functions to call into lua
	ClassBase* (*scriptable_allocate)(const ClassTypeInfo*) = nullptr;
	// opaque pointer to hash table for props
	const PropHashTable* prop_hash_table = nullptr;

	template<typename T>
	static ClassBase* default_allocate(const ClassTypeInfo*) {
		return (ClassBase*)(new T);
	}
	bool is_this_scriptable() const { return scriptable_allocate; }

	bool is_a(const ClassTypeInfo& other) const {
		return id >= other.id && id <= other.last_child;
	}
	bool operator==(const ClassTypeInfo& other) const {
		return id == other.id;
	}
	ClassBase* allocate_this_type() const {
		return allocate ? allocate(this) : nullptr;
	}
	bool has_allocate_func() const {
		return allocate != nullptr;
	}

	REFLECT();
	bool is_subclass_of(const ClassTypeInfo* info) const;
	REFLECT();
	std::string get_classname() const;
	REFLECT();
	const ClassTypeInfo* get_super_type() const;

	int get_prototype_index_table() const;
protected:
	friend class ScriptManager;

	CreateObjectFunc allocate = nullptr;
	// table that provides functions of this and inherited types
	// used with lua tables
	int lua_prototype_index_table = 0;
};


// Function template to create an instance of the class if it is not abstract
template<typename T>
typename std::enable_if<!is_abstract<T>::value, ClassTypeInfo::CreateObjectFunc>::type default_class_create() {
	return ClassTypeInfo::default_allocate<T>;
}

// Overload for the case when the class is abstract
template<typename T>
typename std::enable_if<is_abstract<T>::value, ClassTypeInfo::CreateObjectFunc>::type default_class_create() {
	return nullptr;
}


template<typename T>
inline T* class_cast(ClassBase* in) {
	return in ? in->cast_to<T>() : nullptr;
}
