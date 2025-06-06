// **** GENERATED SOURCE FILE version:1 ****
#include "./MaterialPublic.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"

PropertyInfoList MaterialInstance::get_props()
{
	return {nullptr, 0};
}

ClassTypeInfo MaterialInstance::StaticType = ClassTypeInfo(
                     "MaterialInstance",
                     &IAsset::StaticType,
                     MaterialInstance::get_props,
                     default_class_create<MaterialInstance>(),
                     MaterialInstance::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& MaterialInstance::get_type() const{ return MaterialInstance::StaticType;}
