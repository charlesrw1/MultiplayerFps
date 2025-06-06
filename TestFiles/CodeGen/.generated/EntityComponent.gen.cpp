// **** GENERATED SOURCE FILE version:1 ****
#include "./EntityComponent.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"
#include "Game/Entity.h"

PropertyInfoList Component::get_props()
{
	PropertyInfo* properties[] = {
		,
		,
		
	};
	return {properties, sizeof(properties)/sizeof(PropertyInfo)};
}

ClassTypeInfo Component::StaticType = ClassTypeInfo(
                     "Component",
                     &BaseUpdater::StaticType,
                     Component::get_props,
                     default_class_create<Component>(),
                     Component::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& Component::get_type() const{ return Component::StaticType;}
