// **** GENERATED SOURCE FILE version:1 ****
#include "./ClassBase.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"

PropertyInfoList ClassBase::get_props()
{
	return {nullptr, 0};
}

ClassTypeInfo ClassBase::StaticType = ClassTypeInfo(
                     "ClassBase",
                     &::StaticType,
                     ClassBase::get_props,
                     default_class_create<ClassBase>(),
                     ClassBase::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& ClassBase::get_type() const{ return ClassBase::StaticType;}
