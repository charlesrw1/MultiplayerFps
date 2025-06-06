// **** GENERATED SOURCE FILE version:1 ****
#include "./BaseUpdater.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"

PropertyInfoList BaseUpdater::get_props()
{
	PropertyInfo* properties[] = {
		
	};
	return {properties, sizeof(properties)/sizeof(PropertyInfo)};
}

ClassTypeInfo BaseUpdater::StaticType = ClassTypeInfo(
                     "BaseUpdater",
                     &ClassBase::StaticType,
                     BaseUpdater::get_props,
                     default_class_create<BaseUpdater>(),
                     BaseUpdater::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& BaseUpdater::get_type() const{ return BaseUpdater::StaticType;}
