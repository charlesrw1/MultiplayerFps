// **** GENERATED SOURCE FILE version:1 ****
#include "./Texture.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"

PropertyInfoList Texture::get_props()
{
	return {nullptr, 0};
}

ClassTypeInfo Texture::StaticType = ClassTypeInfo(
                     "Texture",
                     &IAsset::StaticType,
                     Texture::get_props,
                     default_class_create<Texture>(),
                     Texture::CreateDefaultObject,
                     ""
                );
const ClassTypeInfo& Texture::get_type() const{ return Texture::StaticType;}
