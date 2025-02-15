#pragma once
#include "ClassBase.h"
#include "ReflectionProp.h"

// see Scripts/codegen.py for the code gen tool

#define EXPLICIT_NEWCLASS(classname, cpp_supername, reflected_super) \
class classname : public cpp_supername { \
public: \
	using MyClassType = classname; \
	using SuperClassType = reflected_super; \
	static ClassTypeInfo StaticType; \
	const ClassTypeInfo& get_type() const override { return classname::StaticType; } \
	static const PropertyInfoList* get_props();

// creates new class that will be picked by codegen tool
// DONT make get_props() or CLASS_IMPL(x), the tool does that for you
// use REFLECT() macros instead
#define NEWCLASS(classname, supername) \
	EXPLICIT_NEWCLASS(classname, supername, supername)

// arguments are provided as comma seperated list, dont include outer quotes
// options:
//		- 'hide' : dont show in editor properties
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

// sometimes you want forward declared class types in the header, but they need to be definied when registering 
// them in the generated file like AssetPtr<>'s
// use GENERATED_CLASS_INCLUDE(file) to include a file in the generated source, but not in the header
#define GENERATED_CLASS_INCLUDE(x)

// todo
#define NEWSTRUCT(classname) \
struct classname {	\
	using MyClassType = classname; \
	static const PropertyInfoList* get_props();