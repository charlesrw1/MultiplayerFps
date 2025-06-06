// **** GENERATED SOURCE FILE version:1 ****
#include "./IAsset.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"

PropertyInfoList IAsset::get_props()
{
	return {nullptr, 0};
}

ClassTypeInfo IAsset::StaticType = ClassTypeInfo(
                     "IAsset",
                     &ClassBase::StaticType,
                     IAsset::get_props,
                     default_class_create<IAsset>(),
                     IAsset::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& IAsset::get_type() const{ return IAsset::StaticType;}
