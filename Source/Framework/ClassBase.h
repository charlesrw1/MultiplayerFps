#pragma once
#include <cstdint>
#include <type_traits>
#include <memory>
struct PropertyInfoList;
class ClassBase;
class ClassTypeInfo;


struct ClassTypeIterator
{
	ClassTypeIterator(ClassTypeInfo* ti);
	ClassTypeIterator& next() { index++; return *this; }
	bool is_end() const { return index >= end; }
	const ClassTypeInfo* get_type() const;
private:
	int32_t index = 0;
	int32_t end = 0;
};

// arguments are provided as comma seperated list, dont include outer quotes
// options:
//		Visibility: (default is visible and read+write)
//		- 'script_hide' : dont let script read or write
//		- 'script_read' : script can only read
//		- 'script_write' : script_can only write
//		- 'edit_hide'	: dont show in editor inspector
//		- 'hide' : (edit_hide AND script_hide)
// 
//		- 'lua_generic' : will treat arg as a generic parameter and return that when making the lua stub file (doesnt affect runtime)
//		- 'transient' : dont serialize this property
//		- 'type="my_custom_type"' : tags this for use with custom serializer/editor
//		- 'name="some name"' : provides a name override
//		- 'hint="hint override"' : provides a hint value
//		- 'getter' : only for functions, marks it as a getter (can be called in script like a variable access)
//		- 'tooltip' : give a tooltip for property
// supported types:
//		- int, bool, float, uint32_t, int32_t, uint16_t, int16_t, int64_t, uint8_t, int8_t
//		- glm::vec3
//		- glm::quat
//		- std::vector<>
//		- std::string
//		- MulticastDelegate<>
//		- class functions (only if argument types are supported)
//		- EntityPtr
//		- AssetPtr<>
//		- enums (reflected with NEWENUM())
//		- Color32
#define REFLECT(...)

// additionally, you can use this as a shorthand on the same line, but you dont get any arguments
// like REF int myvariable = 0;
#define REF

// like reflect, this also supports arguments (in ...)
// options:
//		- 'scriptable' : lets this class be implemented in lua
#define CLASS_BODY(classname, ...) \
	using MyClassType = classname; \
	static ClassTypeInfo StaticType; \
	virtual const ClassTypeInfo& get_type() const { return classname::StaticType; } \
	static const PropertyInfoList* get_props();

struct Serializer;
class ClassBase
{
public:
	CLASS_BODY(ClassBase);

	const static bool CreateDefaultObject;	/* = false, default setting */
	ClassBase() = default;
	virtual ~ClassBase();
	ClassBase& operator=(const ClassBase& other);
	ClassBase(const ClassBase& other);
	ClassBase(ClassBase&& other);



	virtual void serialize(Serializer& s) {}	// override to add custom serialization functionality
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
	bool is_a() const;
	// creates a copy of class and copies serializable fields
	ClassBase* create_copy(ClassBase* userptr = nullptr);

	REFLECT();
	const ClassTypeInfo* my_type() const;
	REFLECT();
	bool is_subclass_of(const ClassTypeInfo* type) const;
public:
	// called by ClassTypeInfo only during static init
	static void register_class(ClassTypeInfo* cti);
	// called in main() after all classes have been reg'd
	static void init_class_reflection_system();
	static const ClassTypeInfo* find_class(const char* classname);
	static const ClassTypeInfo* find_class(int32_t id);

	static bool does_class_exist(const char* classname) {
		return find_class(classname) != nullptr;
	}

	// allocate a class by string
	template<typename T>
	static T* create_class(const char* classname);
	// allocate by id
	template<typename T>
	static T* create_class(int16_t id);
	// get all subclasses to a class excluding abstract ones
	template<typename T>
	static ClassTypeIterator get_subclasses() {
		return ClassTypeIterator(&T::StaticType);
	}
	static ClassTypeIterator get_subclasses(const ClassTypeInfo* typeinfo) {
		return ClassTypeIterator((ClassTypeInfo*)typeinfo/* remove const here, doesnt matter tho*/);
	}
	static void init_class_info_for_script();

	// non-const, lazy evalutation
	int get_table_registry_id();
	bool is_class_referenced_from_lua() const;
	void free_table_registry_id();
private:
	// this is used for interop with lua
	// this is the table id returned by luaL_ref in the registry
	// the lua table stores a ptr to this object. when the object is deleted in c++, the table's ptr is nulled
	// for non-lua objects, this table is lazily initialized
	// when an object first wants a reference in lua, it creates a table and sets this
	// lua derived objects will set this on construction
	int lua_table_id = 0;
};

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

// see Scripts/codegen.py for the code gen tool

// creates new class that will be picked by codegen tool
// DONT make get_props() or CLASS_IMPL(x), the tool does that for you
// use REFLECT() macros instead


// sometimes you want forward declared class types in the header, but they need to be definied when registering 
// them in the generated file like AssetPtr<>'s
// use GENERATED_CLASS_INCLUDE(file) to include a file in the generated source, but not in the header
#define GENERATED_CLASS_INCLUDE(x)

#include "ClassTypeInfo.h"

// is this class a subclass or an instance of type T

template<typename T>
inline bool ClassBase::is_a() const {
	return get_type().is_a(T::StaticType);
}

// allocate a class by string

template<typename T>
inline T* ClassBase::create_class(const char* classname) {
	auto classinfo = find_class(classname);
	if (!classinfo || !classinfo->is_a(T::StaticType))
		return nullptr;
	ClassBase* base = classinfo->allocate_this_type();
	return static_cast<T*>(base);
}

// allocate by id

template<typename T>
inline T* ClassBase::create_class(int16_t id) {
	auto classinfo = find_class(id);
	if (!classinfo  || !classinfo->is_a(T::StaticType))
		return nullptr;
	ClassBase* base = classinfo->allocate_this_type();
	return static_cast<T*>(base);
}
