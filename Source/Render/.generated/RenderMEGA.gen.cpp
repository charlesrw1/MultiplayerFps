// **** GENERATED SOURCE FILE version:1 ****
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/PropertyMake.h"
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
#include "Scripting/LuaSharpBinding.h"

void push_lVec3_to_lua(lua_State* L, const lVec3& value);
lVec3 get_lVec3_from_lua(lua_State* L, int index);
void push_lQuat_to_lua(lua_State* L, const lQuat& value);
lQuat get_lQuat_from_lua(lua_State* L, int index);

// ---- LuaSharp binding values (Scripts/codegen_luasharp.py) ----
static const int64_t ls_enum_TextureCompressionType[] = {static_cast<int64_t>(TextureCompressionType::Unset), static_cast<int64_t>(TextureCompressionType::Compressed_BC1), static_cast<int64_t>(TextureCompressionType::Uncompressed), static_cast<int64_t>(TextureCompressionType::NormalMap_BC5), static_cast<int64_t>(TextureCompressionType::GreyscaleMask_BC4), static_cast<int64_t>(TextureCompressionType::HighQuality_BC7), static_cast<int64_t>(TextureCompressionType::UseSourceFile)};
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
		make_property<&PostProcessSettings::exposure, checked_offset<offsetof(PostProcessSettings, exposure)>()>("exposure",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::contrast, checked_offset<offsetof(PostProcessSettings, contrast)>()>("contrast",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::saturation, checked_offset<offsetof(PostProcessSettings, saturation)>()>("saturation",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::bloom_intensity, checked_offset<offsetof(PostProcessSettings, bloom_intensity)>()>("bloom_intensity",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::bloom_enabled, checked_offset<offsetof(PostProcessSettings, bloom_enabled)>()>("bloom_enabled",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::bloom_filter_radius, checked_offset<offsetof(PostProcessSettings, bloom_filter_radius)>()>("bloom_filter_radius",PROP_DEFAULT,"","","upsample tent-filter radius, in UV space\n"),
		make_property<&PostProcessSettings::bloom_lens_dirt, checked_offset<offsetof(PostProcessSettings, bloom_lens_dirt)>()>("bloom_lens_dirt",PROP_DEFAULT,"","","optional dirt/smudge mask\n"),
		make_property<&PostProcessSettings::bloom_lens_dirt_intensity, checked_offset<offsetof(PostProcessSettings, bloom_lens_dirt_intensity)>()>("bloom_lens_dirt_intensity",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::tonemap_type, checked_offset<offsetof(PostProcessSettings, tonemap_type)>()>("tonemap_type",PROP_DEFAULT,"","","0=linear,1=reinhard,2=aces,3=uncharted2\n"),
		make_property<&PostProcessSettings::vignette_intensity, checked_offset<offsetof(PostProcessSettings, vignette_intensity)>()>("vignette_intensity",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::vignette_falloff, checked_offset<offsetof(PostProcessSettings, vignette_falloff)>()>("vignette_falloff",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::chromatic_ab, checked_offset<offsetof(PostProcessSettings, chromatic_ab)>()>("chromatic_ab",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::grain_intensity, checked_offset<offsetof(PostProcessSettings, grain_intensity)>()>("grain_intensity",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::grain_size, checked_offset<offsetof(PostProcessSettings, grain_size)>()>("grain_size",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::sharpness, checked_offset<offsetof(PostProcessSettings, sharpness)>()>("sharpness",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::color_temp, checked_offset<offsetof(PostProcessSettings, color_temp)>()>("color_temp",PROP_DEFAULT,"","","-1=cool, +1=warm\n"),
		make_property<&PostProcessSettings::auto_exposure, checked_offset<offsetof(PostProcessSettings, auto_exposure)>()>("auto_exposure",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_method, checked_offset<offsetof(PostProcessSettings, ae_method)>()>("ae_method",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_min_ev, checked_offset<offsetof(PostProcessSettings, ae_min_ev)>()>("ae_min_ev",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_max_ev, checked_offset<offsetof(PostProcessSettings, ae_max_ev)>()>("ae_max_ev",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_speed_up, checked_offset<offsetof(PostProcessSettings, ae_speed_up)>()>("ae_speed_up",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_speed_down, checked_offset<offsetof(PostProcessSettings, ae_speed_down)>()>("ae_speed_down",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_key, checked_offset<offsetof(PostProcessSettings, ae_key)>()>("ae_key",PROP_DEFAULT,"","",""),
		make_property<&PostProcessSettings::ae_low_pct, checked_offset<offsetof(PostProcessSettings, ae_low_pct)>()>("ae_low_pct",PROP_DEFAULT,"","","fraction of darkest pixels to exclude\n"),
		make_property<&PostProcessSettings::ae_high_pct, checked_offset<offsetof(PostProcessSettings, ae_high_pct)>()>("ae_high_pct",PROP_DEFAULT,"","","fraction of brightest pixels to exclude\n")
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
		make_property<&TextureImportSettings::is_generated, checked_offset<offsetof(TextureImportSettings, is_generated)>()>("is_generated",PROP_DEFAULT,"","","was this not sourced from a file, if so skip it\n"),
		make_property<&TextureImportSettings::src_file, checked_offset<offsetof(TextureImportSettings, src_file)>()>("src_file",PROP_DEFAULT,"","","relative filepath, must be in same directory\n"),
		make_property<&TextureImportSettings::compression, checked_offset<offsetof(TextureImportSettings, compression)>()>("compression",PROP_DEFAULT,"","",""),
		make_property<&TextureImportSettings::is_srgb, checked_offset<offsetof(TextureImportSettings, is_srgb)>()>("is_srgb",PROP_DEFAULT,"","","applies to Compressed_BC1 and HighQuality_BC7 only\n"),
		make_property<&TextureImportSettings::resize_width, checked_offset<offsetof(TextureImportSettings, resize_width)>()>("resize_width",PROP_DEFAULT,"","","ie 1024, 2048 etc resize source image\n"),
		make_property<&TextureImportSettings::is_normalmap, checked_offset<offsetof(TextureImportSettings, is_normalmap)>()>("is_normalmap",PROP_DEFAULT,"","","Deprecated, kept only so old .tis files still parse; migrated into\n`compression` by migrate_legacy_tis_compression() and then ignored.\n"),
		make_property<&TextureImportSettings::make_uncompressed, checked_offset<offsetof(TextureImportSettings, make_uncompressed)>()>("make_uncompressed",PROP_DEFAULT,"","",""),
		make_property<&TextureImportSettings::load_source_file, checked_offset<offsetof(TextureImportSettings, load_source_file)>()>("load_source_file",PROP_DEFAULT,"","",""),
		make_property<&TextureImportSettings::nearest_filtering, checked_offset<offsetof(TextureImportSettings, nearest_filtering)>()>("nearest_filtering",PROP_DEFAULT,"","",""),
		make_property<&TextureImportSettings::simplifiedColor, checked_offset<offsetof(TextureImportSettings, simplifiedColor)>()>("simplifiedColor",PROP_DEFAULT,"","","")
    END_PROPS(TextureImportSettings)
}


// ---- LuaSharp binding (Scripts/codegen_luasharp.py) ----
static void* ls_native_MaterialInstance_load(void* path) { return lsbind::proxy_of(MaterialInstance::load(std::string(lsbind::view(path)))); }
static void* ls_native_MaterialInstance_alloc_dynamic_mat(void* from) { return lsbind::proxy_of(MaterialInstance::alloc_dynamic_mat(lsbind::object<MaterialInstance>(from))); }
static void ls_native_MaterialInstance_free_dynamic_mat(void* mat) { MaterialInstance::free_dynamic_mat(lsbind::object<MaterialInstance>(mat)); }
static void ls_native_MaterialInstance_set_float_parameter(void* self, void* name, double f) { lsbind::object<MaterialInstance>(self)->set_float_parameter(lsbind::name(name), static_cast<float>(f)); }
static void ls_native_MaterialInstance_set_floatvec_parameter(void* self, void* name, lsbind::Vec3 xyz, double a) { lsbind::object<MaterialInstance>(self)->set_floatvec_parameter(lsbind::name(name), lsbind::to_glm(xyz), static_cast<float>(a)); }
static void ls_native_MaterialInstance_set_tex_parameter(void* self, void* name, void* t) { lsbind::object<MaterialInstance>(self)->set_tex_parameter(lsbind::name(name), lsbind::object<Texture>(t)); }
static uint8_t ls_native_MaterialInstance_is_this_a_dynamic_material(void* self) { return static_cast<uint8_t>(lsbind::object<MaterialInstance>(self)->is_this_a_dynamic_material() ? 1 : 0); }
static void ls_native_MaterialInstance_set_physics_material(void* self, void* material) { lsbind::object<MaterialInstance>(self)->set_physics_material(lsbind::object<PhysicsMaterialWrapper>(material)); }
static void* ls_native_Model_load(void* path) { return lsbind::proxy_of(Model::load(std::string(lsbind::view(path)))); }
static void ls_native_Model_set_physics_material(void* self, void* mat) { lsbind::object<Model>(self)->set_physics_material(lsbind::object<PhysicsMaterialWrapper>(mat)); }
static void* ls_native_Model_get_physics_material(void* self) { return lsbind::proxy_of(lsbind::object<Model>(self)->get_physics_material()); }
static void* ls_native_Model_get_physics_material_to_use(void* self) { return lsbind::proxy_of(lsbind::object<Model>(self)->get_physics_material_to_use()); }
static uint8_t ls_native_Model_has_skeleton(void* self) { return static_cast<uint8_t>(lsbind::object<Model>(self)->has_skeleton() ? 1 : 0); }
static uint16_t ls_offset_PostProcessSettings_exposure() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "exposure"); return offset; }
static double ls_native_PostProcessSettings__get_exposure(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_exposure())); }
static void ls_native_PostProcessSettings__set_exposure(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_exposure()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_contrast() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "contrast"); return offset; }
static double ls_native_PostProcessSettings__get_contrast(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_contrast())); }
static void ls_native_PostProcessSettings__set_contrast(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_contrast()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_saturation() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "saturation"); return offset; }
static double ls_native_PostProcessSettings__get_saturation(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_saturation())); }
static void ls_native_PostProcessSettings__set_saturation(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_saturation()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_bloom_intensity() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "bloom_intensity"); return offset; }
static double ls_native_PostProcessSettings__get_bloom_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_intensity())); }
static void ls_native_PostProcessSettings__set_bloom_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_bloom_enabled() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "bloom_enabled"); return offset; }
static uint8_t ls_native_PostProcessSettings__get_bloom_enabled(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_enabled()) ? 1 : 0); }
static void ls_native_PostProcessSettings__set_bloom_enabled(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_enabled()) = (value != 0); }
static uint16_t ls_offset_PostProcessSettings_bloom_filter_radius() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "bloom_filter_radius"); return offset; }
static double ls_native_PostProcessSettings__get_bloom_filter_radius(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_filter_radius())); }
static void ls_native_PostProcessSettings__set_bloom_filter_radius(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_filter_radius()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_bloom_lens_dirt_intensity() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "bloom_lens_dirt_intensity"); return offset; }
static double ls_native_PostProcessSettings__get_bloom_lens_dirt_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_lens_dirt_intensity())); }
static void ls_native_PostProcessSettings__set_bloom_lens_dirt_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_bloom_lens_dirt_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_tonemap_type() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "tonemap_type"); return offset; }
static int64_t ls_native_PostProcessSettings__get_tonemap_type(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_tonemap_type())); }
static void ls_native_PostProcessSettings__set_tonemap_type(void* self, int64_t value) { lsbind::field<int>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_tonemap_type()) = static_cast<int>(value); }
static uint16_t ls_offset_PostProcessSettings_vignette_intensity() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "vignette_intensity"); return offset; }
static double ls_native_PostProcessSettings__get_vignette_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_vignette_intensity())); }
static void ls_native_PostProcessSettings__set_vignette_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_vignette_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_vignette_falloff() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "vignette_falloff"); return offset; }
static double ls_native_PostProcessSettings__get_vignette_falloff(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_vignette_falloff())); }
static void ls_native_PostProcessSettings__set_vignette_falloff(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_vignette_falloff()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_chromatic_ab() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "chromatic_ab"); return offset; }
static double ls_native_PostProcessSettings__get_chromatic_ab(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_chromatic_ab())); }
static void ls_native_PostProcessSettings__set_chromatic_ab(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_chromatic_ab()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_grain_intensity() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "grain_intensity"); return offset; }
static double ls_native_PostProcessSettings__get_grain_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_grain_intensity())); }
static void ls_native_PostProcessSettings__set_grain_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_grain_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_grain_size() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "grain_size"); return offset; }
static double ls_native_PostProcessSettings__get_grain_size(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_grain_size())); }
static void ls_native_PostProcessSettings__set_grain_size(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_grain_size()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_sharpness() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "sharpness"); return offset; }
static double ls_native_PostProcessSettings__get_sharpness(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_sharpness())); }
static void ls_native_PostProcessSettings__set_sharpness(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_sharpness()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_color_temp() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "color_temp"); return offset; }
static double ls_native_PostProcessSettings__get_color_temp(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_color_temp())); }
static void ls_native_PostProcessSettings__set_color_temp(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_color_temp()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_auto_exposure() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "auto_exposure"); return offset; }
static uint8_t ls_native_PostProcessSettings__get_auto_exposure(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_auto_exposure()) ? 1 : 0); }
static void ls_native_PostProcessSettings__set_auto_exposure(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_auto_exposure()) = (value != 0); }
static uint16_t ls_offset_PostProcessSettings_ae_method() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_method"); return offset; }
static int64_t ls_native_PostProcessSettings__get_ae_method(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_method())); }
static void ls_native_PostProcessSettings__set_ae_method(void* self, int64_t value) { lsbind::field<int>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_method()) = static_cast<int>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_min_ev() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_min_ev"); return offset; }
static double ls_native_PostProcessSettings__get_ae_min_ev(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_min_ev())); }
static void ls_native_PostProcessSettings__set_ae_min_ev(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_min_ev()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_max_ev() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_max_ev"); return offset; }
static double ls_native_PostProcessSettings__get_ae_max_ev(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_max_ev())); }
static void ls_native_PostProcessSettings__set_ae_max_ev(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_max_ev()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_speed_up() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_speed_up"); return offset; }
static double ls_native_PostProcessSettings__get_ae_speed_up(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_speed_up())); }
static void ls_native_PostProcessSettings__set_ae_speed_up(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_speed_up()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_speed_down() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_speed_down"); return offset; }
static double ls_native_PostProcessSettings__get_ae_speed_down(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_speed_down())); }
static void ls_native_PostProcessSettings__set_ae_speed_down(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_speed_down()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_key() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_key"); return offset; }
static double ls_native_PostProcessSettings__get_ae_key(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_key())); }
static void ls_native_PostProcessSettings__set_ae_key(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_key()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_low_pct() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_low_pct"); return offset; }
static double ls_native_PostProcessSettings__get_ae_low_pct(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_low_pct())); }
static void ls_native_PostProcessSettings__set_ae_low_pct(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_low_pct()) = static_cast<float>(value); }
static uint16_t ls_offset_PostProcessSettings_ae_high_pct() { static const uint16_t offset = lsbind::field_offset(PostProcessSettings::get_props(), "ae_high_pct"); return offset; }
static double ls_native_PostProcessSettings__get_ae_high_pct(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_high_pct())); }
static void ls_native_PostProcessSettings__set_ae_high_pct(void* self, double value) { lsbind::field<float>(lsbind::object<PostProcessSettings>(self), ls_offset_PostProcessSettings_ae_high_pct()) = static_cast<float>(value); }
static void* ls_native_Texture_load(void* path) { return lsbind::proxy_of(Texture::load(std::string(lsbind::view(path)))); }
static uint16_t ls_offset_TextureImportSettings_is_generated() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "is_generated"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_is_generated(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_generated()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_is_generated(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_generated()) = (value != 0); }
static uint16_t ls_offset_TextureImportSettings_src_file() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "src_file"); return offset; }
static void* ls_native_TextureImportSettings__get_src_file(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_src_file())); }
static void ls_native_TextureImportSettings__set_src_file(void* self, void* value) { lsbind::field<std::string>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_src_file()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_TextureImportSettings_compression() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "compression"); return offset; }
static int64_t ls_native_TextureImportSettings__get_compression(void* self) { return lsbind::enum_ordinal(ls_enum_TextureCompressionType, std::size(ls_enum_TextureCompressionType), static_cast<int64_t>(lsbind::field<TextureCompressionType>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_compression()))); }
static void ls_native_TextureImportSettings__set_compression(void* self, int64_t value) { lsbind::field<TextureCompressionType>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_compression()) = static_cast<TextureCompressionType>(lsbind::enum_value(ls_enum_TextureCompressionType, std::size(ls_enum_TextureCompressionType), value)); }
static uint16_t ls_offset_TextureImportSettings_is_srgb() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "is_srgb"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_is_srgb(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_srgb()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_is_srgb(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_srgb()) = (value != 0); }
static uint16_t ls_offset_TextureImportSettings_resize_width() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "resize_width"); return offset; }
static int64_t ls_native_TextureImportSettings__get_resize_width(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_resize_width())); }
static void ls_native_TextureImportSettings__set_resize_width(void* self, int64_t value) { lsbind::field<int>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_resize_width()) = static_cast<int>(value); }
static uint16_t ls_offset_TextureImportSettings_is_normalmap() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "is_normalmap"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_is_normalmap(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_normalmap()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_is_normalmap(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_is_normalmap()) = (value != 0); }
static uint16_t ls_offset_TextureImportSettings_make_uncompressed() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "make_uncompressed"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_make_uncompressed(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_make_uncompressed()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_make_uncompressed(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_make_uncompressed()) = (value != 0); }
static uint16_t ls_offset_TextureImportSettings_load_source_file() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "load_source_file"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_load_source_file(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_load_source_file()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_load_source_file(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_load_source_file()) = (value != 0); }
static uint16_t ls_offset_TextureImportSettings_nearest_filtering() { static const uint16_t offset = lsbind::field_offset(TextureImportSettings::get_props(), "nearest_filtering"); return offset; }
static uint8_t ls_native_TextureImportSettings__get_nearest_filtering(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_nearest_filtering()) ? 1 : 0); }
static void ls_native_TextureImportSettings__set_nearest_filtering(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<TextureImportSettings>(self), ls_offset_TextureImportSettings_nearest_filtering()) = (value != 0); }
static const char* const ls_declarations[] = {
	R"ls(enum BlendState { OPAQUE, BLEND, ADD, MULT, SCREEN, PREMULT_BLEND }
)ls",
	R"ls(enum TextureCompressionType { Unset, Compressed_BC1, Uncompressed, NormalMap_BC5, GreyscaleMask_BC4, HighQuality_BC7, UseSourceFile }
)ls",
	R"ls(// MaterialInstance (MaterialPublic.h)
extern class MaterialInstance : IAsset {
    static MaterialInstance load(string path);
    static MaterialInstance alloc_dynamic_mat(MaterialInstance from);
    static void free_dynamic_mat(MaterialInstance mat);
    void set_float_parameter(string name, float f);
    void set_floatvec_parameter(string name, Vec3 xyz, float a);
    void set_tex_parameter(string name, Texture t);
    bool is_this_a_dynamic_material();
    void set_physics_material(PhysicsMaterialWrapper material);
}
)ls",
	R"ls(// Model (Model.h)
extern class Model : IAsset {
    static Model load(string path);
    void set_physics_material(PhysicsMaterialWrapper mat);
    PhysicsMaterialWrapper get_physics_material();
    PhysicsMaterialWrapper get_physics_material_to_use();
    bool has_skeleton();
}
)ls",
	R"ls(// PostProcessSettings (PostProcessSettings.h)
extern class PostProcessSettings : IAsset {
    float exposure;
    float contrast;
    float saturation;
    float bloom_intensity;
    bool bloom_enabled;
    float bloom_filter_radius;
    float bloom_lens_dirt_intensity;
    int tonemap_type;
    float vignette_intensity;
    float vignette_falloff;
    float chromatic_ab;
    float grain_intensity;
    float grain_size;
    float sharpness;
    float color_temp;
    bool auto_exposure;
    int ae_method;
    float ae_min_ev;
    float ae_max_ev;
    float ae_speed_up;
    float ae_speed_down;
    float ae_key;
    float ae_low_pct;
    float ae_high_pct;
    // skipped bloom_lens_dirt: AssetPtr<Texture>
}
)ls",
	R"ls(// Texture (Texture.h)
extern class Texture : IAsset {
    static Texture load(string path);
}
)ls",
	R"ls(// TextureImportSettings (TextureEditor.h)
extern class TextureImportSettings : ClassBase {
    bool is_generated;
    string src_file;
    TextureCompressionType compression;
    bool is_srgb;
    int resize_width;
    bool is_normalmap;
    bool make_uncompressed;
    bool load_source_file;
    bool nearest_filtering;
    // skipped simplifiedColor: Color32
}
)ls",
};
static lsbind::Symbol const ls_symbols[] = {
	{"ls_native_MaterialInstance_load", reinterpret_cast<const void*>(&ls_native_MaterialInstance_load)},
	{"ls_native_MaterialInstance_alloc_dynamic_mat", reinterpret_cast<const void*>(&ls_native_MaterialInstance_alloc_dynamic_mat)},
	{"ls_native_MaterialInstance_free_dynamic_mat", reinterpret_cast<const void*>(&ls_native_MaterialInstance_free_dynamic_mat)},
	{"ls_native_MaterialInstance_set_float_parameter", reinterpret_cast<const void*>(&ls_native_MaterialInstance_set_float_parameter)},
	{"ls_native_MaterialInstance_set_floatvec_parameter", reinterpret_cast<const void*>(&ls_native_MaterialInstance_set_floatvec_parameter)},
	{"ls_native_MaterialInstance_set_tex_parameter", reinterpret_cast<const void*>(&ls_native_MaterialInstance_set_tex_parameter)},
	{"ls_native_MaterialInstance_is_this_a_dynamic_material", reinterpret_cast<const void*>(&ls_native_MaterialInstance_is_this_a_dynamic_material)},
	{"ls_native_MaterialInstance_set_physics_material", reinterpret_cast<const void*>(&ls_native_MaterialInstance_set_physics_material)},
	{"ls_native_Model_load", reinterpret_cast<const void*>(&ls_native_Model_load)},
	{"ls_native_Model_set_physics_material", reinterpret_cast<const void*>(&ls_native_Model_set_physics_material)},
	{"ls_native_Model_get_physics_material", reinterpret_cast<const void*>(&ls_native_Model_get_physics_material)},
	{"ls_native_Model_get_physics_material_to_use", reinterpret_cast<const void*>(&ls_native_Model_get_physics_material_to_use)},
	{"ls_native_Model_has_skeleton", reinterpret_cast<const void*>(&ls_native_Model_has_skeleton)},
	{"ls_native_PostProcessSettings__get_exposure", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_exposure)},
	{"ls_native_PostProcessSettings__set_exposure", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_exposure)},
	{"ls_native_PostProcessSettings__get_contrast", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_contrast)},
	{"ls_native_PostProcessSettings__set_contrast", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_contrast)},
	{"ls_native_PostProcessSettings__get_saturation", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_saturation)},
	{"ls_native_PostProcessSettings__set_saturation", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_saturation)},
	{"ls_native_PostProcessSettings__get_bloom_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_bloom_intensity)},
	{"ls_native_PostProcessSettings__set_bloom_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_bloom_intensity)},
	{"ls_native_PostProcessSettings__get_bloom_enabled", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_bloom_enabled)},
	{"ls_native_PostProcessSettings__set_bloom_enabled", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_bloom_enabled)},
	{"ls_native_PostProcessSettings__get_bloom_filter_radius", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_bloom_filter_radius)},
	{"ls_native_PostProcessSettings__set_bloom_filter_radius", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_bloom_filter_radius)},
	{"ls_native_PostProcessSettings__get_bloom_lens_dirt_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_bloom_lens_dirt_intensity)},
	{"ls_native_PostProcessSettings__set_bloom_lens_dirt_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_bloom_lens_dirt_intensity)},
	{"ls_native_PostProcessSettings__get_tonemap_type", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_tonemap_type)},
	{"ls_native_PostProcessSettings__set_tonemap_type", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_tonemap_type)},
	{"ls_native_PostProcessSettings__get_vignette_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_vignette_intensity)},
	{"ls_native_PostProcessSettings__set_vignette_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_vignette_intensity)},
	{"ls_native_PostProcessSettings__get_vignette_falloff", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_vignette_falloff)},
	{"ls_native_PostProcessSettings__set_vignette_falloff", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_vignette_falloff)},
	{"ls_native_PostProcessSettings__get_chromatic_ab", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_chromatic_ab)},
	{"ls_native_PostProcessSettings__set_chromatic_ab", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_chromatic_ab)},
	{"ls_native_PostProcessSettings__get_grain_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_grain_intensity)},
	{"ls_native_PostProcessSettings__set_grain_intensity", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_grain_intensity)},
	{"ls_native_PostProcessSettings__get_grain_size", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_grain_size)},
	{"ls_native_PostProcessSettings__set_grain_size", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_grain_size)},
	{"ls_native_PostProcessSettings__get_sharpness", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_sharpness)},
	{"ls_native_PostProcessSettings__set_sharpness", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_sharpness)},
	{"ls_native_PostProcessSettings__get_color_temp", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_color_temp)},
	{"ls_native_PostProcessSettings__set_color_temp", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_color_temp)},
	{"ls_native_PostProcessSettings__get_auto_exposure", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_auto_exposure)},
	{"ls_native_PostProcessSettings__set_auto_exposure", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_auto_exposure)},
	{"ls_native_PostProcessSettings__get_ae_method", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_method)},
	{"ls_native_PostProcessSettings__set_ae_method", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_method)},
	{"ls_native_PostProcessSettings__get_ae_min_ev", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_min_ev)},
	{"ls_native_PostProcessSettings__set_ae_min_ev", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_min_ev)},
	{"ls_native_PostProcessSettings__get_ae_max_ev", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_max_ev)},
	{"ls_native_PostProcessSettings__set_ae_max_ev", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_max_ev)},
	{"ls_native_PostProcessSettings__get_ae_speed_up", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_speed_up)},
	{"ls_native_PostProcessSettings__set_ae_speed_up", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_speed_up)},
	{"ls_native_PostProcessSettings__get_ae_speed_down", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_speed_down)},
	{"ls_native_PostProcessSettings__set_ae_speed_down", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_speed_down)},
	{"ls_native_PostProcessSettings__get_ae_key", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_key)},
	{"ls_native_PostProcessSettings__set_ae_key", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_key)},
	{"ls_native_PostProcessSettings__get_ae_low_pct", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_low_pct)},
	{"ls_native_PostProcessSettings__set_ae_low_pct", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_low_pct)},
	{"ls_native_PostProcessSettings__get_ae_high_pct", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__get_ae_high_pct)},
	{"ls_native_PostProcessSettings__set_ae_high_pct", reinterpret_cast<const void*>(&ls_native_PostProcessSettings__set_ae_high_pct)},
	{"ls_native_Texture_load", reinterpret_cast<const void*>(&ls_native_Texture_load)},
	{"ls_native_TextureImportSettings__get_is_generated", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_is_generated)},
	{"ls_native_TextureImportSettings__set_is_generated", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_is_generated)},
	{"ls_native_TextureImportSettings__get_src_file", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_src_file)},
	{"ls_native_TextureImportSettings__set_src_file", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_src_file)},
	{"ls_native_TextureImportSettings__get_compression", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_compression)},
	{"ls_native_TextureImportSettings__set_compression", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_compression)},
	{"ls_native_TextureImportSettings__get_is_srgb", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_is_srgb)},
	{"ls_native_TextureImportSettings__set_is_srgb", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_is_srgb)},
	{"ls_native_TextureImportSettings__get_resize_width", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_resize_width)},
	{"ls_native_TextureImportSettings__set_resize_width", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_resize_width)},
	{"ls_native_TextureImportSettings__get_is_normalmap", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_is_normalmap)},
	{"ls_native_TextureImportSettings__set_is_normalmap", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_is_normalmap)},
	{"ls_native_TextureImportSettings__get_make_uncompressed", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_make_uncompressed)},
	{"ls_native_TextureImportSettings__set_make_uncompressed", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_make_uncompressed)},
	{"ls_native_TextureImportSettings__get_load_source_file", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_load_source_file)},
	{"ls_native_TextureImportSettings__set_load_source_file", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_load_source_file)},
	{"ls_native_TextureImportSettings__get_nearest_filtering", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__get_nearest_filtering)},
	{"ls_native_TextureImportSettings__set_nearest_filtering", reinterpret_cast<const void*>(&ls_native_TextureImportSettings__set_nearest_filtering)},
};
static lsbind::Class const ls_classes[] = {
	{"MaterialInstance", "IAsset"},
	{"Model", "IAsset"},
	{"PostProcessSettings", "IAsset"},
	{"Texture", "IAsset"},
	{"TextureImportSettings", "ClassBase"},
};
static const std::span<lsbind::Scriptable const> ls_scriptables;
static const std::span<lsbind::StructLayout const> ls_structs;
static const lsbind::Registrar ls_registrar{"RenderMEGA", ls_declarations, ls_symbols, ls_classes, ls_scriptables, ls_structs};
