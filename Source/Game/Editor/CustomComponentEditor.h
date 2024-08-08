#pragma once
#include "Framework/AddClassToFactory.h"
#include "Framework/ClassBase.h"

// you can register component types in this factory and they will be instantiated in the level editor
// use for debug line rendering or custom ui when a component is selected etc.

class EntityComponent;
class ICustomComponentEditor
{
public:
	// uses raii for lifetime
	virtual ~ICustomComponentEditor() {}
	virtual bool init(EntityComponent* ec) = 0;	// return false to destroy this ICustomComponentEditor
	virtual bool tick() = 0;	// return false to destroy this ICustomComponentEditor

	static Factory<const ClassTypeInfo*, ICustomComponentEditor>& get_factory() {
		static Factory<const ClassTypeInfo*, ICustomComponentEditor> inst;
		return inst;
	}

};

template<typename T,typename K>
struct RegisterComponentEditor
{
	RegisterComponentEditor() {
		ICustomComponentEditor::get_factory().registerClass<K>(&T::StaticType);
	}
};
#define REGISTER_COMPONENTEDITOR_MACRO(EDITOR_TYPE, ENTITY_COMPONENT_TYPE) \
static auto componet_ed_type##ENTITY_COMPONENT_TYPE = RegisterComponentEditor<ENTITY_COMPONENT_TYPE,EDITOR_TYPE>();