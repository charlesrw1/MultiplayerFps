#pragma once
#include "ClassBase.h"
#include "Reflection2.h"

struct SerializedForDiffing;
struct PropHashTable;
struct FunctionInfo;
class ClassTypeInfo : public ClassBase {
public:
	CLASS_BODY(ClassTypeInfo);

	typedef ClassBase* (*CreateObjectFunc)();
	typedef const PropertyInfoList* (*GetPropsFunc_t)();

	ClassTypeInfo(const char* classname,
		const ClassTypeInfo* super_typeinfo,
		GetPropsFunc_t get_prop_func,
		CreateObjectFunc alloc,
		bool create_default_obj,
		const FunctionInfo* func_infos,
		int func_info_count
	);
	~ClassTypeInfo();

	int32_t id = 0;
	int32_t last_child = 0;
	const char* classname = "";
	const char* superclassname = "";
	ClassBase* (*allocate)() = nullptr;
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

private:

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
