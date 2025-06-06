// **** GENERATED SOURCE FILE version:1 ****
#include "./Entity.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"
#include "Game/EntityComponent.h"

PropertyInfoList Entity::get_props()
{
	PropertyInfo* properties[] = {
		,
		,
		,
		,
		,
		,
		,
		,
		,
		,
		,
		,
		,
		make_struct_actual_property("tag",offsetof(Entity, tag),"",PROP_DEFAULT,&StringName::StructType),
		make_vec3_property("position",offsetof(Entity, position),"",PROP_DEFAULT),
		make_quat_property("rotation",offsetof(Entity, rotation),"",PROP_DEFAULT),
		make_vec3_property("scale",offsetof(Entity, scale),"",PROP_DEFAULT),
		make_string_property("editor_name",offsetof(Entity, editor_name),"",PROP_DEFAULT),
		make_struct_actual_property("parent_bone",offsetof(Entity, parent_bone),"",PROP_DEFAULT,&StringName::StructType),
		make_bool_property("start_disabled",offsetof(Entity, start_disabled),"",PROP_DEFAULT),
		make_bool_property("prefab_editable",offsetof(Entity, prefab_editable),"",PROP_DEFAULT)
	};
	return {properties, sizeof(properties)/sizeof(PropertyInfo)};
}

ClassTypeInfo Entity::StaticType = ClassTypeInfo(
                     "Entity",
                     &BaseUpdater::StaticType,
                     Entity::get_props,
                     default_class_create<Entity>(),
                     Entity::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& Entity::get_type() const{ return Entity::StaticType;}
