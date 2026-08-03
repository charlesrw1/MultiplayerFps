// **** GENERATED SOURCE FILE version:1 2026-08-03 10:43:13 ****
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Scripting/ScriptManager.h"
#include "./Render/DrawTypedefs.h"
#include "./Render/MaterialPublic.h"
#include "Physics/Physics2.h"
#include "./Render/Model.h"
#include "./Render/PostProcessSettings.h"
#include "./Render/Texture.h"
#include "./Render\Editor/TextureEditor.h"

void push_lVec3_to_lua(lua_State* L, const lVec3& value);
lVec3 get_lVec3_from_lua(lua_State* L, int index);
void push_lQuat_to_lua(lua_State* L, const lQuat& value);
lQuat get_lQuat_from_lua(lua_State* L, int index);
static EnumIntPair enumstrsBlendState[] = {
	EnumIntPair("OPAQUE","",(int64_t)BlendState::OPAQUE),
	EnumIntPair("BLEND","",(int64_t)BlendState::BLEND),
	EnumIntPair("ADD","",(int64_t)BlendState::ADD),
	EnumIntPair("MULT","",(int64_t)BlendState::MULT),
	EnumIntPair("SCREEN","",(int64_t)BlendState::SCREEN),
	EnumIntPair("PREMULT_BLEND","",(int64_t)BlendState::PREMULT_BLEND)
};
EnumTypeInfo EnumTrait<BlendState>::StaticEnumType = EnumTypeInfo("BlendState",enumstrsBlendState,6);

int lua_binding_MaterialInstance_load(lua_State* L)
{
	auto return_value = MaterialInstance::load(
		get_std_string_from_lua(L,1) // path
	);
	push_object_to_lua(L,return_value);
	return 1;
}
int lua_binding_MaterialInstance_alloc_dynamic_mat(lua_State* L)
{
	auto return_value = MaterialInstance::alloc_dynamic_mat(
		class_cast<MaterialInstance>(get_object_from_lua(L,1)) // from
	);
	push_object_to_lua(L,return_value);
	return 1;
}
int lua_binding_MaterialInstance_free_dynamic_mat(lua_State* L)
{
	MaterialInstance::free_dynamic_mat(
		class_cast<MaterialInstance>(get_object_from_lua(L,1)) // mat
	);
	return 0;
}
int lua_binding_MaterialInstance_set_float_parameter(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	MaterialInstance* myObj = obj ? obj->cast_to<MaterialInstance>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling MaterialInstance::set_float_parameter");  }
	myObj->set_float_parameter(
		get_stringname_from_lua(L,2), // name
		get_float_from_lua(L,3) // f
	);
	return 0;
}
int lua_binding_MaterialInstance_set_floatvec_parameter(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	MaterialInstance* myObj = obj ? obj->cast_to<MaterialInstance>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling MaterialInstance::set_floatvec_parameter");  }
	myObj->set_floatvec_parameter(
		get_stringname_from_lua(L,2), // name
		get_lVec3_from_lua(L,3), // xyz
		get_float_from_lua(L,4) // a
	);
	return 0;
}
int lua_binding_MaterialInstance_set_tex_parameter(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	MaterialInstance* myObj = obj ? obj->cast_to<MaterialInstance>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling MaterialInstance::set_tex_parameter");  }
	myObj->set_tex_parameter(
		get_stringname_from_lua(L,2), // name
		class_cast<Texture>(get_object_from_lua(L,3)) // t
	);
	return 0;
}
int lua_binding_MaterialInstance_is_this_a_dynamic_material(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	MaterialInstance* myObj = obj ? obj->cast_to<MaterialInstance>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling MaterialInstance::is_this_a_dynamic_material");  }
	auto return_value = myObj->is_this_a_dynamic_material(
	);
	push_bool_to_lua(L,return_value);
	return 1;
}
int lua_binding_MaterialInstance_set_physics_material(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	MaterialInstance* myObj = obj ? obj->cast_to<MaterialInstance>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling MaterialInstance::set_physics_material");  }
	myObj->set_physics_material(
		class_cast<PhysicsMaterialWrapper>(get_object_from_lua(L,2)) // material
	);
	return 0;
}
FunctionInfo MaterialInstance_function_list[] = {{ "load",true,false, lua_binding_MaterialInstance_load  },
{ "alloc_dynamic_mat",true,false, lua_binding_MaterialInstance_alloc_dynamic_mat  },
{ "free_dynamic_mat",true,false, lua_binding_MaterialInstance_free_dynamic_mat  },
{ "set_float_parameter",false,false, lua_binding_MaterialInstance_set_float_parameter  },
{ "set_floatvec_parameter",false,false, lua_binding_MaterialInstance_set_floatvec_parameter  },
{ "set_tex_parameter",false,false, lua_binding_MaterialInstance_set_tex_parameter  },
{ "is_this_a_dynamic_material",false,false, lua_binding_MaterialInstance_is_this_a_dynamic_material  },
{ "set_physics_material",false,false, lua_binding_MaterialInstance_set_physics_material  }};
ClassTypeInfo MaterialInstance::StaticType = ClassTypeInfo(
                     "MaterialInstance",
                     &IAsset::StaticType,
                     MaterialInstance::get_props,
                     default_class_create<MaterialInstance>(),
                     MaterialInstance::CreateDefaultObject,MaterialInstance_function_list,8,nullptr,false,false);
const PropertyInfoList* MaterialInstance::get_props()
{
	return nullptr;
}

int lua_binding_Model_load(lua_State* L)
{
	auto return_value = Model::load(
		get_std_string_from_lua(L,1) // path
	);
	push_object_to_lua(L,return_value);
	return 1;
}
int lua_binding_Model_set_physics_material(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	Model* myObj = obj ? obj->cast_to<Model>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling Model::set_physics_material");  }
	myObj->set_physics_material(
		class_cast<PhysicsMaterialWrapper>(get_object_from_lua(L,2)) // mat
	);
	return 0;
}
int lua_binding_Model_get_physics_material(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	Model* myObj = obj ? obj->cast_to<Model>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling Model::get_physics_material");  }
	auto return_value = myObj->get_physics_material(
	);
	push_object_to_lua(L,return_value);
	return 1;
}
int lua_binding_Model_get_physics_material_to_use(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	Model* myObj = obj ? obj->cast_to<Model>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling Model::get_physics_material_to_use");  }
	auto return_value = myObj->get_physics_material_to_use(
	);
	push_object_to_lua(L,return_value);
	return 1;
}
int lua_binding_Model_has_skeleton(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	Model* myObj = obj ? obj->cast_to<Model>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling Model::has_skeleton");  }
	auto return_value = myObj->has_skeleton(
	);
	push_bool_to_lua(L,return_value);
	return 1;
}
FunctionInfo Model_function_list[] = {{ "load",true,false, lua_binding_Model_load  },
{ "set_physics_material",false,false, lua_binding_Model_set_physics_material  },
{ "get_physics_material",false,false, lua_binding_Model_get_physics_material  },
{ "get_physics_material_to_use",false,false, lua_binding_Model_get_physics_material_to_use  },
{ "has_skeleton",false,false, lua_binding_Model_has_skeleton  }};
ClassTypeInfo Model::StaticType = ClassTypeInfo(
                     "Model",
                     &IAsset::StaticType,
                     Model::get_props,
                     default_class_create<Model>(),
                     Model::CreateDefaultObject,Model_function_list,5,nullptr,false,false);
const PropertyInfoList* Model::get_props()
{
	return nullptr;
}

ClassTypeInfo PostProcessSettings::StaticType = ClassTypeInfo(
                     "PostProcessSettings",
                     &IAsset::StaticType,
                     PostProcessSettings::get_props,
                     default_class_create<PostProcessSettings>(),
                     PostProcessSettings::CreateDefaultObject,nullptr,0,nullptr,false,false);
const PropertyInfoList* PostProcessSettings::get_props()
{
    START_PROPS(PostProcessSettings)
		make_float_property("exposure",offsetof(PostProcessSettings, exposure),PROP_DEFAULT,""),
		make_float_property("contrast",offsetof(PostProcessSettings, contrast),PROP_DEFAULT,""),
		make_float_property("saturation",offsetof(PostProcessSettings, saturation),PROP_DEFAULT,""),
		make_float_property("bloom_intensity",offsetof(PostProcessSettings, bloom_intensity),PROP_DEFAULT,""),
		make_bool_property_custom("bloom_enabled",offsetof(PostProcessSettings, bloom_enabled),PROP_DEFAULT,"",""),
		make_float_property("bloom_filter_radius",offsetof(PostProcessSettings, bloom_filter_radius),PROP_DEFAULT,""),
		make_assetptr_property_new("bloom_lens_dirt",offsetof(PostProcessSettings, bloom_lens_dirt),PROP_DEFAULT,"optional dirt/smudge mask\n",&Texture::StaticType),
		make_float_property("bloom_lens_dirt_intensity",offsetof(PostProcessSettings, bloom_lens_dirt_intensity),PROP_DEFAULT,""),
		make_integer_property("tonemap_type",offsetof(PostProcessSettings, tonemap_type),PROP_DEFAULT,sizeof(int),""),
		make_float_property("vignette_intensity",offsetof(PostProcessSettings, vignette_intensity),PROP_DEFAULT,""),
		make_float_property("vignette_falloff",offsetof(PostProcessSettings, vignette_falloff),PROP_DEFAULT,""),
		make_float_property("chromatic_ab",offsetof(PostProcessSettings, chromatic_ab),PROP_DEFAULT,""),
		make_float_property("grain_intensity",offsetof(PostProcessSettings, grain_intensity),PROP_DEFAULT,""),
		make_float_property("grain_size",offsetof(PostProcessSettings, grain_size),PROP_DEFAULT,""),
		make_float_property("sharpness",offsetof(PostProcessSettings, sharpness),PROP_DEFAULT,""),
		make_float_property("color_temp",offsetof(PostProcessSettings, color_temp),PROP_DEFAULT,""),
		make_bool_property_custom("auto_exposure",offsetof(PostProcessSettings, auto_exposure),PROP_DEFAULT,"",""),
		make_integer_property("ae_method",offsetof(PostProcessSettings, ae_method),PROP_DEFAULT,sizeof(int),""),
		make_float_property("ae_min_ev",offsetof(PostProcessSettings, ae_min_ev),PROP_DEFAULT,""),
		make_float_property("ae_max_ev",offsetof(PostProcessSettings, ae_max_ev),PROP_DEFAULT,""),
		make_float_property("ae_speed_up",offsetof(PostProcessSettings, ae_speed_up),PROP_DEFAULT,""),
		make_float_property("ae_speed_down",offsetof(PostProcessSettings, ae_speed_down),PROP_DEFAULT,""),
		make_float_property("ae_key",offsetof(PostProcessSettings, ae_key),PROP_DEFAULT,""),
		make_float_property("ae_low_pct",offsetof(PostProcessSettings, ae_low_pct),PROP_DEFAULT,""),
		make_float_property("ae_high_pct",offsetof(PostProcessSettings, ae_high_pct),PROP_DEFAULT,"")
    END_PROPS(PostProcessSettings)
}

int lua_binding_Texture_load(lua_State* L)
{
	auto return_value = Texture::load(
		get_std_string_from_lua(L,1) // path
	);
	push_object_to_lua(L,return_value);
	return 1;
}
FunctionInfo Texture_function_list[] = {{ "load",true,false, lua_binding_Texture_load  }};
ClassTypeInfo Texture::StaticType = ClassTypeInfo(
                     "Texture",
                     &IAsset::StaticType,
                     Texture::get_props,
                     default_class_create<Texture>(),
                     Texture::CreateDefaultObject,Texture_function_list,1,nullptr,false,false);
const PropertyInfoList* Texture::get_props()
{
	return nullptr;
}

static EnumIntPair enumstrsTextureCompressionType[] = {
	EnumIntPair("Unset","",(int64_t)TextureCompressionType::Unset),
	EnumIntPair("Compressed_BC1","",(int64_t)TextureCompressionType::Compressed_BC1),
	EnumIntPair("Uncompressed","",(int64_t)TextureCompressionType::Uncompressed),
	EnumIntPair("NormalMap_BC5","",(int64_t)TextureCompressionType::NormalMap_BC5),
	EnumIntPair("GreyscaleMask_BC4","",(int64_t)TextureCompressionType::GreyscaleMask_BC4),
	EnumIntPair("HighQuality_BC7","",(int64_t)TextureCompressionType::HighQuality_BC7),
	EnumIntPair("UseSourceFile","",(int64_t)TextureCompressionType::UseSourceFile)
};
EnumTypeInfo EnumTrait<TextureCompressionType>::StaticEnumType = EnumTypeInfo("TextureCompressionType",enumstrsTextureCompressionType,7);

ClassTypeInfo TextureImportSettings::StaticType = ClassTypeInfo(
                     "TextureImportSettings",
                     &ClassBase::StaticType,
                     TextureImportSettings::get_props,
                     default_class_create<TextureImportSettings>(),
                     TextureImportSettings::CreateDefaultObject,nullptr,0,nullptr,false,false);
const PropertyInfoList* TextureImportSettings::get_props()
{
    START_PROPS(TextureImportSettings)
		make_bool_property_custom("is_generated",offsetof(TextureImportSettings, is_generated),PROP_DEFAULT,"",""),
		make_string_property("src_file",offsetof(TextureImportSettings, src_file),PROP_DEFAULT,""),
		make_enum_property("compression",offsetof(TextureImportSettings, compression),PROP_DEFAULT,sizeof(TextureCompressionType),&::EnumTrait<TextureCompressionType>::StaticEnumType, ""),
		make_bool_property_custom("is_srgb",offsetof(TextureImportSettings, is_srgb),PROP_DEFAULT,"",""),
		make_integer_property("resize_width",offsetof(TextureImportSettings, resize_width),PROP_DEFAULT,sizeof(int),""),
		make_bool_property_custom("is_normalmap",offsetof(TextureImportSettings, is_normalmap),PROP_DEFAULT,"",""),
		make_bool_property_custom("make_uncompressed",offsetof(TextureImportSettings, make_uncompressed),PROP_DEFAULT,"",""),
		make_bool_property_custom("load_source_file",offsetof(TextureImportSettings, load_source_file),PROP_DEFAULT,"",""),
		make_bool_property_custom("nearest_filtering",offsetof(TextureImportSettings, nearest_filtering),PROP_DEFAULT,"",""),
		make_integer_property("simplifiedColor",offsetof(TextureImportSettings, simplifiedColor),PROP_DEFAULT,sizeof(Color32),"","ColorUint")
    END_PROPS(TextureImportSettings)
}

