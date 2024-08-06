#pragma once
#include <cstdint>
#include <type_traits>

struct PropertyInfoList;
class ClassBase;

struct PropHashTable;
struct ClassTypeInfo
{
public:
	typedef ClassBase* (*CreateObjectFunc)();
	typedef const PropertyInfoList* (*GetPropsFunc_t)();

	ClassTypeInfo(const char* classname, 
		const ClassTypeInfo* super_typeinfo, 
		GetPropsFunc_t get_prop_func, 
		CreateObjectFunc alloc,
		bool create_default_obj
	);

	uint16_t id = 0;
	uint16_t last_child = 0;
	const char* classname = "";
	const char* superclassname = "";
	ClassBase*(*allocate)()=nullptr;
	const PropertyInfoList* props = nullptr;
	const ClassTypeInfo* super_typeinfo = nullptr;

	// store this, get_props might rely on other Class's typinfo being registered, so call these after init
	GetPropsFunc_t get_props_function = nullptr;

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

private:

};

template<typename, typename T>
struct has_get_props {
	static_assert(
		std::integral_constant<T, false>::value,
		"Second template parameter needs to be of function type.");
};

// Specialization that does the checking
template<typename C, typename Ret, typename... Args>
struct has_get_props<C, Ret(Args...)> {
private:
	template<typename T>
	static constexpr auto check(T*) -> typename
		std::is_same<
		decltype(std::declval<T>().get_props(std::declval<Args>()...)),
		Ret    // Ensure the return type matches
		>::type;

	template<typename>
	static constexpr std::false_type check(...);

	typedef decltype(check<C>(0)) type;

public:
	static constexpr bool value = type::value;
};



template<typename T>
ClassTypeInfo::GetPropsFunc_t get_get_props_if_it_exists() {
	if constexpr (has_get_props<T, const PropertyInfoList*()>::value) {
		return T::get_props;
	}
	else {
		return nullptr;
	}
}

template<typename T>
struct is_abstract {
private:
	template<typename U>
	static constexpr auto test(U*) -> decltype(U(), std::false_type());

	template<typename>
	static constexpr std::true_type test(...);

public:
	static constexpr bool value = decltype(test<T>(nullptr))::value;
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
	const static bool CreateDefaultObject;	/* = false, default setting */

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
	ClassBase* create_copy(ClassBase* userptr = nullptr);
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

	static ClassTypeIterator get_subclasses(const ClassTypeInfo* typeinfo) {
		return ClassTypeIterator((ClassTypeInfo*)typeinfo/* remove const here, doesnt matter tho*/);
	}
};