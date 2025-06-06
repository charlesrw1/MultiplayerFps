// **** GENERATED SOURCE FILE version:1 ****
#include "./MeshComponent.h"
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Game/AssetPtrMacro.h"
#include "Game/AssetPtrArrayMacro.h"
#include "Game/EntityPtrMacro.h"
#include "Scripting/FunctionReflection.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"
#include "Render/Model.h"
#include "Render/MaterialPublic.h"
#include "Animation/AnimationTreePublic.h"

PropertyInfoList Serializer::get_props()
{
	return {nullptr, 0};
}

StructTypeInfo Serializer::StructType = StructTypeInfo(
                 "Serializer",
nullptr
);
static void SomeStruct_serialize_private(void* p, Serializer& s)
{
	((SomeStruct*)p)->serialize(s);
}
PropertyInfoList SomeStruct::get_props()
{
	PropertyInfo* properties[] = {
		make_int_property("x",offsetof(SomeStruct, x),"",PROP_DEFAULT,sizeof(int)),
		make_int_property("y",offsetof(SomeStruct, y),"",PROP_DEFAULT,sizeof(int)),
		
	};
	return {properties, sizeof(properties)/sizeof(PropertyInfo)};
}

StructTypeInfo SomeStruct::StructType = StructTypeInfo(
                 "SomeStruct",
SomeStruct_serialize_private
);
PropertyInfoList MeshComponent::get_props()
{
	static StdVectorCallback<AssetPtr<MaterialInstance>> vectorcallback_eMaterialOverride(make_asset_ptr_property("",0,PROP_DEFAULT,"array of material overrides for the model\n",&MaterialInstance::StaticType));
	PropertyInfo* properties[] = {
		,
		,
		,
		make_asset_ptr_property("model",offsetof(MeshComponent, model),"the model of this\n",PROP_DEFAULT,&Model::StaticType),
		make_bool_property("is_visible",offsetof(MeshComponent, is_visible),"disables drawing\n",PROP_DEFAULT),
		make_bool_property("cast_shadows",offsetof(MeshComponent, cast_shadows),"does model cast shadows\n",PROP_DEFAULT),
		make_bool_property("is_skybox",offsetof(MeshComponent, is_skybox),"is this mesh a skybox? used for skylight emission\n",PROP_DEFAULT),
		make_array_property("eMaterialOverride",offsetof(MeshComponent, eMaterialOverride),"array of material overrides for the model\n",PROP_DEFAULT, &vectorcallback_eMaterialOverride)
	};
	return {properties, sizeof(properties)/sizeof(PropertyInfo)};
}

ClassTypeInfo MeshComponent::StaticType = ClassTypeInfo(
                     "MeshComponent",
                     &Component::StaticType,
                     MeshComponent::get_props,
                     default_class_create<MeshComponent>(),
                     MeshComponent::CreateDefaultObject,
                     "Represents a drawable item in the world\n"
                );
const ClassTypeInfo& MeshComponent::get_type() const{ return MeshComponent::StaticType;}
