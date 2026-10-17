// **** GENERATED SOURCE FILE version:1 ****
#include "Framework/ReflectionProp.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/VectorReflect2.h"
#include "Framework/EnumDefReflection.h"
#include "Framework/InterfaceTypeInfo.h"
#include "Scripting/ScriptFunctionCodegen.h"
#include "Scripting/ScriptManager.h"
#include "./MyGame\bike/BikeHeaders.h"
#include "./MyGame\fps/AnimGraphTester.h"
#include "./MyGame\fps/fpsApp.h"
#include "./MyGame\fps/fpsObjects.h"
#include "./MyGame\fps/fpsTestObjects.h"

void push_lVec3_to_lua(lua_State* L, const lVec3& value);
lVec3 get_lVec3_from_lua(lua_State* L, int index);
void push_lQuat_to_lua(lua_State* L, const lQuat& value);
lQuat get_lQuat_from_lua(lua_State* L, int index);
int lua_binding_fpsIDamageable_deal_damage(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsIDamageable* myObj = obj ? obj->cast_interface<fpsIDamageable>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsIDamageable::deal_damage");  }
	myObj->deal_damage(
		get_int_from_lua(L,2) // amount
	);
	return 0;
}
InterfaceTypeInfo fpsIDamageable::StaticInterfaceType("fpsIDamageable");
ClassTypeInfo BikeObject::StaticType = ClassTypeInfo(
                     "BikeObject",
                     &Component::StaticType,
                     BikeObject::get_props,
                     default_class_create<BikeObject>(),
                     BikeObject::CreateDefaultObject,nullptr,0,nullptr,false,false);
const PropertyInfoList* BikeObject::get_props()
{
	return nullptr;
}

ClassTypeInfo BikeGameApplication::StaticType = ClassTypeInfo(
                     "BikeGameApplication",
                     &Application::StaticType,
                     BikeGameApplication::get_props,
                     default_class_create<BikeGameApplication>(),
                     BikeGameApplication::CreateDefaultObject,nullptr,0,nullptr,false,false);
const PropertyInfoList* BikeGameApplication::get_props()
{
	return nullptr;
}

static EnumIntPair enumstrsAnimGraphTestMode[] = {
	EnumIntPair("BasicIK","",(int64_t)AnimGraphTestMode::BasicIK),
	EnumIntPair("LookAt","",(int64_t)AnimGraphTestMode::LookAt),
	EnumIntPair("GunGripIK","",(int64_t)AnimGraphTestMode::GunGripIK),
	EnumIntPair("FeetIK","",(int64_t)AnimGraphTestMode::FeetIK),
	EnumIntPair("BlendMasked","",(int64_t)AnimGraphTestMode::BlendMasked),
	EnumIntPair("CopyBone","",(int64_t)AnimGraphTestMode::CopyBone),
	EnumIntPair("SlotPlaying","",(int64_t)AnimGraphTestMode::SlotPlaying),
	EnumIntPair("Additive","",(int64_t)AnimGraphTestMode::Additive),
	EnumIntPair("BlendByInt","",(int64_t)AnimGraphTestMode::BlendByInt),
	EnumIntPair("DurationEventTest","",(int64_t)AnimGraphTestMode::DurationEventTest),
	EnumIntPair("BlendSpace2D","",(int64_t)AnimGraphTestMode::BlendSpace2D),
	EnumIntPair("SpringBoneTest","",(int64_t)AnimGraphTestMode::SpringBoneTest),
	EnumIntPair("CachedPoseTest","",(int64_t)AnimGraphTestMode::CachedPoseTest)
};
EnumTypeInfo EnumTrait<AnimGraphTestMode>::StaticEnumType = EnumTypeInfo("AnimGraphTestMode",enumstrsAnimGraphTestMode,13);

ClassTypeInfo AnimGraphTester::StaticType = ClassTypeInfo(
                     "AnimGraphTester",
                     &Component::StaticType,
                     AnimGraphTester::get_props,
                     default_class_create<AnimGraphTester>(),
                     AnimGraphTester::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* AnimGraphTester::get_props()
{
    START_PROPS(AnimGraphTester)
		make_enum_property("mode",offsetof(AnimGraphTester, mode),PROP_DEFAULT,sizeof(AnimGraphTestMode),&::EnumTrait<AnimGraphTestMode>::StaticEnumType, ""),
		make_assetptr_property_new("model",offsetof(AnimGraphTester, model),PROP_DEFAULT,"",&Model::StaticType),
		make_assetptr_property_new("matoverride",offsetof(AnimGraphTester, matoverride),PROP_DEFAULT,"",&MaterialInstance::StaticType),
		make_assetptr_property_new("prop",offsetof(AnimGraphTester, prop),PROP_DEFAULT,"Optional prop: when set, a transient child MeshComponent entity is spawned and\nparented to prop_bone on the character (e.g. a rifle on the right hand) for testing.\n",&Model::StaticType),
		make_string_property("prop_bone",offsetof(AnimGraphTester, prop_bone),PROP_DEFAULT,"BoneNameString"),
		make_assetptr_property_new("clip0",offsetof(AnimGraphTester, clip0),PROP_DEFAULT,"Animation clips used by the graph modes\nprimary / idle clip\n",&AnimationSeqAsset::StaticType),
		make_assetptr_property_new("clip1",offsetof(AnimGraphTester, clip1),PROP_DEFAULT,"secondary clip (walk, upper-body, additive)\n",&AnimationSeqAsset::StaticType),
		make_assetptr_property_new("clip2",offsetof(AnimGraphTester, clip2),PROP_DEFAULT,"tertiary clip (BlendByInt state 2 / BlendSpace2D corner)\n",&AnimationSeqAsset::StaticType),
		make_assetptr_property_new("clip3",offsetof(AnimGraphTester, clip3),PROP_DEFAULT,"quaternary clip (BlendSpace2D corner)\n",&AnimationSeqAsset::StaticType),
		make_assetptr_property_new("slot_clip",offsetof(AnimGraphTester, slot_clip),PROP_DEFAULT,"one-shot clip fired into SlotPlaying slot\n",&AnimationSeqAsset::StaticType),
		make_assetptr_property_new("footstep_particle",offsetof(AnimGraphTester, footstep_particle),PROP_DEFAULT,"",&ParticleAsset::StaticType),
		make_assetptr_property_new("footstep_sfx",offsetof(AnimGraphTester, footstep_sfx),PROP_DEFAULT,"",&SoundFile::StaticType),
		make_string_property("bone_ik_upper",offsetof(AnimGraphTester, bone_ik_upper),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_ik_end",offsetof(AnimGraphTester, bone_ik_end),PROP_DEFAULT,"BoneNameString"),
		make_vec3_property("look_forward_axis",offsetof(AnimGraphTester, look_forward_axis),PROP_DEFAULT,""),
		make_bool_property_custom("use_pole_bone_for_ik",offsetof(AnimGraphTester, use_pole_bone_for_ik),PROP_DEFAULT,"",""),
		make_float_property("max_stretch",offsetof(AnimGraphTester, max_stretch),PROP_DEFAULT,""),
		make_float_property("start_stretch_ratio",offsetof(AnimGraphTester, start_stretch_ratio),PROP_DEFAULT,""),
		make_string_property("bone_upper_blend",offsetof(AnimGraphTester, bone_upper_blend),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_copy_src",offsetof(AnimGraphTester, bone_copy_src),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_copy_dst",offsetof(AnimGraphTester, bone_copy_dst),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_grip_other",offsetof(AnimGraphTester, bone_grip_other),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_grip_mask",offsetof(AnimGraphTester, bone_grip_mask),PROP_DEFAULT,"BoneNameString"),
		make_string_property("bone_spring",offsetof(AnimGraphTester, bone_spring),PROP_DEFAULT,"BoneNameString"),
		make_float_property("spring_yaw_stiffness",offsetof(AnimGraphTester, spring_yaw_stiffness),PROP_DEFAULT,""),
		make_float_property("spring_yaw_damping",offsetof(AnimGraphTester, spring_yaw_damping),PROP_DEFAULT,""),
		make_float_property("spring_pitch_stiffness",offsetof(AnimGraphTester, spring_pitch_stiffness),PROP_DEFAULT,""),
		make_float_property("spring_pitch_damping",offsetof(AnimGraphTester, spring_pitch_damping),PROP_DEFAULT,""),
		make_float_property("spring_along_stiffness",offsetof(AnimGraphTester, spring_along_stiffness),PROP_DEFAULT,""),
		make_float_property("spring_along_damping",offsetof(AnimGraphTester, spring_along_damping),PROP_DEFAULT,""),
		make_bool_property_custom("spring_allow_length_flex",offsetof(AnimGraphTester, spring_allow_length_flex),PROP_DEFAULT,"",""),
		make_float_property("spring_gravity",offsetof(AnimGraphTester, spring_gravity),PROP_DEFAULT,""),
		make_float_property("foot_trace_dist",offsetof(AnimGraphTester, foot_trace_dist),PROP_DEFAULT,""),
		make_float_property("foot_height_off",offsetof(AnimGraphTester, foot_height_off),PROP_DEFAULT,""),
		make_float_property("foot_max_tilt_deg",offsetof(AnimGraphTester, foot_max_tilt_deg),PROP_DEFAULT,""),
		make_bool_property_custom("foot_align_rot",offsetof(AnimGraphTester, foot_align_rot),PROP_DEFAULT,"",""),
		make_float_property("pelvis_interp_speed",offsetof(AnimGraphTester, pelvis_interp_speed),PROP_DEFAULT,""),
		make_integer_property("gungrip_frame_eval",offsetof(AnimGraphTester, gungrip_frame_eval),PROP_DEFAULT,sizeof(int),""),
		make_integer_property("cachepose_frame_eval_2",offsetof(AnimGraphTester, cachepose_frame_eval_2),PROP_DEFAULT,sizeof(int),""),
		make_float_property("generic_alpha",offsetof(AnimGraphTester, generic_alpha),PROP_DEFAULT,""),
		make_enum_property("transition_easing",offsetof(AnimGraphTester, transition_easing),PROP_DEFAULT,sizeof(Easing),&::EnumTrait<Easing>::StaticEnumType, ""),
		make_float_property("transition_time",offsetof(AnimGraphTester, transition_time),PROP_DEFAULT,""),
		make_float_property("bs2d_x",offsetof(AnimGraphTester, bs2d_x),PROP_DEFAULT,""),
		make_float_property("bs2d_y",offsetof(AnimGraphTester, bs2d_y),PROP_DEFAULT,""),
		make_bool_property_custom("bs2d_manual",offsetof(AnimGraphTester, bs2d_manual),PROP_DEFAULT,"",""),
		make_float_property("bs_smooth_time",offsetof(AnimGraphTester, bs_smooth_time),PROP_DEFAULT,""),
		make_float_property("bs_input_smooth",offsetof(AnimGraphTester, bs_input_smooth),PROP_DEFAULT,""),
		make_bool_property_custom("cached_use_run",offsetof(AnimGraphTester, cached_use_run),PROP_DEFAULT,"",""),
		make_bool_property_custom("cached_use_meshspace",offsetof(AnimGraphTester, cached_use_meshspace),PROP_DEFAULT,"","")
    END_PROPS(AnimGraphTester)
}

int lua_binding_fpsLuaBridge_init(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsLuaBridge* myObj = obj ? obj->cast_to<fpsLuaBridge>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsLuaBridge::init");  }
	myObj->init(
	);
	return 0;
}
int lua_binding_fpsLuaBridge_start_level_script(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsLuaBridge* myObj = obj ? obj->cast_to<fpsLuaBridge>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsLuaBridge::start_level_script");  }
	myObj->start_level_script(
	);
	return 0;
}
int lua_binding_fpsLuaBridge_update(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsLuaBridge* myObj = obj ? obj->cast_to<fpsLuaBridge>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsLuaBridge::update");  }
	myObj->update(
	);
	return 0;
}
int lua_binding_fpsLuaBridge_imgui_tick(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsLuaBridge* myObj = obj ? obj->cast_to<fpsLuaBridge>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsLuaBridge::imgui_tick");  }
	myObj->imgui_tick(
	);
	return 0;
}
FunctionInfo fpsLuaBridge_function_list[] = {{ "init",false,true, lua_binding_fpsLuaBridge_init  },
{ "start_level_script",false,true, lua_binding_fpsLuaBridge_start_level_script  },
{ "update",false,true, lua_binding_fpsLuaBridge_update  },
{ "imgui_tick",false,true, lua_binding_fpsLuaBridge_imgui_tick  }};
class ScriptImpl_fpsLuaBridge : public fpsLuaBridge  {
public:

    const ClassTypeInfo* type = nullptr;
    const ClassTypeInfo& get_type() const final { return *type; }
    	void init() final {
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
        int top = lua_gettop(L);  // save stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, myTable);
        lua_pushstring(L, "init");
        lua_rawget(L, -2); // use raw get to not look in __index
        bool is_func = lua_isfunction(L, -1);
        if(is_func) {
            lua_pushvalue(L, -2);  // duplicate object table
            
            if (safe_pcall(L, 1, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                lua_pop(L, 1);  // pop error
                lua_settop(L, top); // restore stack!
                throw LuaRuntimeError("During call fpsLuaBridge::init" + std::string("(luatype=")+type->classname+"):" + error);
            }
            else {
			lua_settop(L,top); // stack restore
			}
		}
		else{
			lua_pop(L,1);
			lua_settop(L,top); // stack restore
			return fpsLuaBridge::init();
		}
	}
	void start_level_script() final {
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
        int top = lua_gettop(L);  // save stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, myTable);
        lua_pushstring(L, "start_level_script");
        lua_rawget(L, -2); // use raw get to not look in __index
        bool is_func = lua_isfunction(L, -1);
        if(is_func) {
            lua_pushvalue(L, -2);  // duplicate object table
            
            if (safe_pcall(L, 1, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                lua_pop(L, 1);  // pop error
                lua_settop(L, top); // restore stack!
                throw LuaRuntimeError("During call fpsLuaBridge::start_level_script" + std::string("(luatype=")+type->classname+"):" + error);
            }
            else {
			lua_settop(L,top); // stack restore
			}
		}
		else{
			lua_pop(L,1);
			lua_settop(L,top); // stack restore
			return fpsLuaBridge::start_level_script();
		}
	}
	void update() final {
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
        int top = lua_gettop(L);  // save stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, myTable);
        lua_pushstring(L, "update");
        lua_rawget(L, -2); // use raw get to not look in __index
        bool is_func = lua_isfunction(L, -1);
        if(is_func) {
            lua_pushvalue(L, -2);  // duplicate object table
            
            if (safe_pcall(L, 1, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                lua_pop(L, 1);  // pop error
                lua_settop(L, top); // restore stack!
                throw LuaRuntimeError("During call fpsLuaBridge::update" + std::string("(luatype=")+type->classname+"):" + error);
            }
            else {
			lua_settop(L,top); // stack restore
			}
		}
		else{
			lua_pop(L,1);
			lua_settop(L,top); // stack restore
			return fpsLuaBridge::update();
		}
	}
	void imgui_tick() final {
        lua_State* L = ScriptManager::inst->get_lua_state();
        int myTable = get_table_registry_id();
        int top = lua_gettop(L);  // save stack
        lua_rawgeti(L, LUA_REGISTRYINDEX, myTable);
        lua_pushstring(L, "imgui_tick");
        lua_rawget(L, -2); // use raw get to not look in __index
        bool is_func = lua_isfunction(L, -1);
        if(is_func) {
            lua_pushvalue(L, -2);  // duplicate object table
            
            if (safe_pcall(L, 1, 0) != LUA_OK) {
                const char* error = lua_tostring(L, -1);
                lua_pop(L, 1);  // pop error
                lua_settop(L, top); // restore stack!
                throw LuaRuntimeError("During call fpsLuaBridge::imgui_tick" + std::string("(luatype=")+type->classname+"):" + error);
            }
            else {
			lua_settop(L,top); // stack restore
			}
		}
		else{
			lua_pop(L,1);
			lua_settop(L,top); // stack restore
			return fpsLuaBridge::imgui_tick();
		}
	}
};
ClassTypeInfo fpsLuaBridge::StaticType = ClassTypeInfo(
                     "fpsLuaBridge",
                     &ClassBase::StaticType,
                     fpsLuaBridge::get_props,
                     default_class_create<fpsLuaBridge>(),
                     fpsLuaBridge::CreateDefaultObject,fpsLuaBridge_function_list,4,get_allocate_script_impl_internal<ScriptImpl_fpsLuaBridge>(),false,false);
const PropertyInfoList* fpsLuaBridge::get_props()
{
	return nullptr;
}

int lua_binding_fpsApp_change_level(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsApp* myObj = obj ? obj->cast_to<fpsApp>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsApp::change_level");  }
	myObj->change_level(
		get_std_string_from_lua(L,2) // next_level
	);
	return 0;
}
int lua_binding_fpsApp_get_player(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsApp* myObj = obj ? obj->cast_to<fpsApp>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsApp::get_player");  }
	auto return_value = myObj->get_player(
	);
	push_object_to_lua(L,return_value);
	return 1;
}
FunctionInfo fpsApp_function_list[] = {{ "change_level",false,false, lua_binding_fpsApp_change_level  },
{ "get_player",false,false, lua_binding_fpsApp_get_player  }};
ClassTypeInfo fpsApp::StaticType = ClassTypeInfo(
                     "fpsApp",
                     &Application::StaticType,
                     fpsApp::get_props,
                     default_class_create<fpsApp>(),
                     fpsApp::CreateDefaultObject,fpsApp_function_list,2,nullptr,false,false);
const PropertyInfoList* fpsApp::get_props()
{
	return nullptr;
}

ClassTypeInfo fpsSpawnPoint::StaticType = ClassTypeInfo(
                     "fpsSpawnPoint",
                     &Component::StaticType,
                     fpsSpawnPoint::get_props,
                     default_class_create<fpsSpawnPoint>(),
                     fpsSpawnPoint::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* fpsSpawnPoint::get_props()
{
    START_PROPS(fpsSpawnPoint)
		make_string_property("other",offsetof(fpsSpawnPoint, other),PROP_DEFAULT,"EntityTarget")
    END_PROPS(fpsSpawnPoint)
}

ClassTypeInfo fpsPropPhysics::StaticType = ClassTypeInfo(
                     "fpsPropPhysics",
                     &Component::StaticType,
                     fpsPropPhysics::get_props,
                     default_class_create<fpsPropPhysics>(),
                     fpsPropPhysics::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* fpsPropPhysics::get_props()
{
    START_PROPS(fpsPropPhysics)
		make_assetptr_property_new("model",offsetof(fpsPropPhysics, model),PROP_DEFAULT,"",&Model::StaticType)
    END_PROPS(fpsPropPhysics)
}

ClassTypeInfo fpsFlickeringLightScript::StaticType = ClassTypeInfo(
                     "fpsFlickeringLightScript",
                     &Component::StaticType,
                     fpsFlickeringLightScript::get_props,
                     default_class_create<fpsFlickeringLightScript>(),
                     fpsFlickeringLightScript::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* fpsFlickeringLightScript::get_props()
{
    START_PROPS(fpsFlickeringLightScript)
		make_float_property("min_intensity",offsetof(fpsFlickeringLightScript, min_intensity),PROP_DEFAULT,""),
		make_float_property("max_intensity",offsetof(fpsFlickeringLightScript, max_intensity),PROP_DEFAULT,""),
		make_float_property("radius",offsetof(fpsFlickeringLightScript, radius),PROP_DEFAULT,""),
		make_integer_property("color",offsetof(fpsFlickeringLightScript, color),PROP_DEFAULT,sizeof(Color32),"","ColorUint"),
		make_float_property("frequency",offsetof(fpsFlickeringLightScript, frequency),PROP_DEFAULT,""),
		make_float_property("offset",offsetof(fpsFlickeringLightScript, offset),PROP_DEFAULT,""),
		make_integer_property("octaves",offsetof(fpsFlickeringLightScript, octaves),PROP_DEFAULT,sizeof(int),"")
    END_PROPS(fpsFlickeringLightScript)
}

int lua_binding_fpsPlayer_get_blah_components(lua_State* L)
{
	ClassBase* obj = get_object_from_lua(L,1);
	fpsPlayer* myObj = obj ? obj->cast_to<fpsPlayer>() : nullptr;
	if(!myObj) {  luaL_error(L,"null dereference for object calling fpsPlayer::get_blah_components");  }
	auto return_value = myObj->get_blah_components(
	);
	push_std_vector_to_lua(L,return_value,[&](Component* val) {  push_object_to_lua(L,val); });
	return 1;
}
FunctionInfo fpsPlayer_function_list[] = {{ "get_blah_components",false,false, lua_binding_fpsPlayer_get_blah_components  }};
ClassTypeInfo fpsPlayer::StaticType = ClassTypeInfo(
                     "fpsPlayer",
                     &Component::StaticType,
                     fpsPlayer::get_props,
                     default_class_create<fpsPlayer>(),
                     fpsPlayer::CreateDefaultObject,fpsPlayer_function_list,1,nullptr,false,false);
const PropertyInfoList* fpsPlayer::get_props()
{
	return nullptr;
}

ClassTypeInfo PhysicsLayerTesterTriggerScript::StaticType = ClassTypeInfo(
                     "PhysicsLayerTesterTriggerScript",
                     &Component::StaticType,
                     PhysicsLayerTesterTriggerScript::get_props,
                     default_class_create<PhysicsLayerTesterTriggerScript>(),
                     PhysicsLayerTesterTriggerScript::CreateDefaultObject,nullptr,0,nullptr,false,false);
const PropertyInfoList* PhysicsLayerTesterTriggerScript::get_props()
{
	return nullptr;
}

ClassTypeInfo PhysicsLayerTesterObject::StaticType = ClassTypeInfo(
                     "PhysicsLayerTesterObject",
                     &Component::StaticType,
                     PhysicsLayerTesterObject::get_props,
                     default_class_create<PhysicsLayerTesterObject>(),
                     PhysicsLayerTesterObject::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* PhysicsLayerTesterObject::get_props()
{
    START_PROPS(PhysicsLayerTesterObject)
		make_integer_property("collider_type",offsetof(PhysicsLayerTesterObject, collider_type),PROP_DEFAULT,sizeof(int),""),
		make_assetptr_property_new("model",offsetof(PhysicsLayerTesterObject, model),PROP_DEFAULT,"",&Model::StaticType),
		make_assetptr_property_new("trigger",offsetof(PhysicsLayerTesterObject, trigger),PROP_DEFAULT,"",&MaterialInstance::StaticType),
		make_enum_property("bodytype",offsetof(PhysicsLayerTesterObject, bodytype),PROP_DEFAULT,sizeof(BodyType),&::EnumTrait<BodyType>::StaticEnumType, ""),
		make_bool_property_custom("is_trigger",offsetof(PhysicsLayerTesterObject, is_trigger),PROP_DEFAULT,"",""),
		make_new_struct_type("simulate_toggle",offsetof(PhysicsLayerTesterObject, simulate_toggle),PROP_DEFAULT,"",&BoolButton::StructType),
		make_new_struct_type("respawn",offsetof(PhysicsLayerTesterObject, respawn),PROP_DEFAULT,"",&BoolButton::StructType),
		make_integer_property("layer",offsetof(PhysicsLayerTesterObject, layer),PROP_DEFAULT,sizeof(int),"")
    END_PROPS(PhysicsLayerTesterObject)
}

ClassTypeInfo PhysicsLayerTesterRaycast::StaticType = ClassTypeInfo(
                     "PhysicsLayerTesterRaycast",
                     &Component::StaticType,
                     PhysicsLayerTesterRaycast::get_props,
                     default_class_create<PhysicsLayerTesterRaycast>(),
                     PhysicsLayerTesterRaycast::CreateDefaultObject,nullptr,0,nullptr,false,true);
const PropertyInfoList* PhysicsLayerTesterRaycast::get_props()
{
    START_PROPS(PhysicsLayerTesterRaycast)
		make_bool_property_custom("layer0",offsetof(PhysicsLayerTesterRaycast, layer0),PROP_DEFAULT,"",""),
		make_bool_property_custom("layer1",offsetof(PhysicsLayerTesterRaycast, layer1),PROP_DEFAULT,"",""),
		make_bool_property_custom("layer2",offsetof(PhysicsLayerTesterRaycast, layer2),PROP_DEFAULT,"",""),
		make_bool_property_custom("layer3",offsetof(PhysicsLayerTesterRaycast, layer3),PROP_DEFAULT,"",""),
		make_bool_property_custom("layer4",offsetof(PhysicsLayerTesterRaycast, layer4),PROP_DEFAULT,"",""),
		make_bool_property_custom("layer5",offsetof(PhysicsLayerTesterRaycast, layer5),PROP_DEFAULT,"","")
    END_PROPS(PhysicsLayerTesterRaycast)
}


// ---- LuaSharp binding (Scripts/codegen_luasharp.py) ----
#include "Scripting/LuaSharpBinding.h"
static const int64_t ls_enum_AnimGraphTestMode[] = {static_cast<int64_t>(AnimGraphTestMode::BasicIK), static_cast<int64_t>(AnimGraphTestMode::LookAt), static_cast<int64_t>(AnimGraphTestMode::GunGripIK), static_cast<int64_t>(AnimGraphTestMode::FeetIK), static_cast<int64_t>(AnimGraphTestMode::BlendMasked), static_cast<int64_t>(AnimGraphTestMode::CopyBone), static_cast<int64_t>(AnimGraphTestMode::SlotPlaying), static_cast<int64_t>(AnimGraphTestMode::Additive), static_cast<int64_t>(AnimGraphTestMode::BlendByInt), static_cast<int64_t>(AnimGraphTestMode::DurationEventTest), static_cast<int64_t>(AnimGraphTestMode::BlendSpace2D), static_cast<int64_t>(AnimGraphTestMode::SpringBoneTest), static_cast<int64_t>(AnimGraphTestMode::CachedPoseTest)};
static const int64_t ls_enum_Easing[] = {static_cast<int64_t>(Easing::Linear), static_cast<int64_t>(Easing::CubicEaseIn), static_cast<int64_t>(Easing::CubicEaseOut), static_cast<int64_t>(Easing::CubicEaseInOut), static_cast<int64_t>(Easing::Constant)};
static const int64_t ls_enum_BodyType[] = {static_cast<int64_t>(BodyType::Static), static_cast<int64_t>(BodyType::Kinematic), static_cast<int64_t>(BodyType::Dynamic)};
static uint16_t ls_offset_AnimGraphTester_mode() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "mode"); return offset; }
static int64_t ls_native_AnimGraphTester__get_mode(void* self) { return lsbind::enum_ordinal(ls_enum_AnimGraphTestMode, std::size(ls_enum_AnimGraphTestMode), static_cast<int64_t>(lsbind::field<AnimGraphTestMode>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_mode()))); }
static void ls_native_AnimGraphTester__set_mode(void* self, int64_t value) { lsbind::field<AnimGraphTestMode>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_mode()) = static_cast<AnimGraphTestMode>(lsbind::enum_value(ls_enum_AnimGraphTestMode, std::size(ls_enum_AnimGraphTestMode), value)); }
static uint16_t ls_offset_AnimGraphTester_prop_bone() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "prop_bone"); return offset; }
static void* ls_native_AnimGraphTester__get_prop_bone(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_prop_bone())); }
static void ls_native_AnimGraphTester__set_prop_bone(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_prop_bone()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_footstep_sfx() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "footstep_sfx"); return offset; }
static void* ls_native_AnimGraphTester__get_footstep_sfx(void* self) { return lsbind::proxy_of(lsbind::field<SoundFile*>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_footstep_sfx())); }
static void ls_native_AnimGraphTester__set_footstep_sfx(void* self, void* value) { lsbind::field<SoundFile*>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_footstep_sfx()) = lsbind::object<SoundFile>(value); }
static uint16_t ls_offset_AnimGraphTester_bone_ik_upper() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_ik_upper"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_ik_upper(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_ik_upper())); }
static void ls_native_AnimGraphTester__set_bone_ik_upper(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_ik_upper()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_ik_end() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_ik_end"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_ik_end(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_ik_end())); }
static void ls_native_AnimGraphTester__set_bone_ik_end(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_ik_end()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_look_forward_axis() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "look_forward_axis"); return offset; }
static lsbind::Vec3 ls_native_AnimGraphTester__get_look_forward_axis(void* self) { return lsbind::to_ls(lsbind::field<glm::vec3>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_look_forward_axis())); }
static void ls_native_AnimGraphTester__set_look_forward_axis(void* self, lsbind::Vec3 value) { lsbind::field<glm::vec3>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_look_forward_axis()) = lsbind::to_glm(value); }
static uint16_t ls_offset_AnimGraphTester_use_pole_bone_for_ik() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "use_pole_bone_for_ik"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_use_pole_bone_for_ik(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_use_pole_bone_for_ik()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_use_pole_bone_for_ik(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_use_pole_bone_for_ik()) = (value != 0); }
static uint16_t ls_offset_AnimGraphTester_max_stretch() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "max_stretch"); return offset; }
static double ls_native_AnimGraphTester__get_max_stretch(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_max_stretch())); }
static void ls_native_AnimGraphTester__set_max_stretch(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_max_stretch()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_start_stretch_ratio() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "start_stretch_ratio"); return offset; }
static double ls_native_AnimGraphTester__get_start_stretch_ratio(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_start_stretch_ratio())); }
static void ls_native_AnimGraphTester__set_start_stretch_ratio(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_start_stretch_ratio()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_bone_upper_blend() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_upper_blend"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_upper_blend(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_upper_blend())); }
static void ls_native_AnimGraphTester__set_bone_upper_blend(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_upper_blend()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_copy_src() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_copy_src"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_copy_src(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_copy_src())); }
static void ls_native_AnimGraphTester__set_bone_copy_src(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_copy_src()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_copy_dst() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_copy_dst"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_copy_dst(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_copy_dst())); }
static void ls_native_AnimGraphTester__set_bone_copy_dst(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_copy_dst()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_grip_other() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_grip_other"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_grip_other(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_grip_other())); }
static void ls_native_AnimGraphTester__set_bone_grip_other(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_grip_other()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_grip_mask() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_grip_mask"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_grip_mask(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_grip_mask())); }
static void ls_native_AnimGraphTester__set_bone_grip_mask(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_grip_mask()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_bone_spring() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bone_spring"); return offset; }
static void* ls_native_AnimGraphTester__get_bone_spring(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_spring())); }
static void ls_native_AnimGraphTester__set_bone_spring(void* self, void* value) { lsbind::field<std::string>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bone_spring()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_AnimGraphTester_spring_yaw_stiffness() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_yaw_stiffness"); return offset; }
static double ls_native_AnimGraphTester__get_spring_yaw_stiffness(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_yaw_stiffness())); }
static void ls_native_AnimGraphTester__set_spring_yaw_stiffness(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_yaw_stiffness()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_yaw_damping() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_yaw_damping"); return offset; }
static double ls_native_AnimGraphTester__get_spring_yaw_damping(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_yaw_damping())); }
static void ls_native_AnimGraphTester__set_spring_yaw_damping(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_yaw_damping()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_pitch_stiffness() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_pitch_stiffness"); return offset; }
static double ls_native_AnimGraphTester__get_spring_pitch_stiffness(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_pitch_stiffness())); }
static void ls_native_AnimGraphTester__set_spring_pitch_stiffness(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_pitch_stiffness()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_pitch_damping() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_pitch_damping"); return offset; }
static double ls_native_AnimGraphTester__get_spring_pitch_damping(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_pitch_damping())); }
static void ls_native_AnimGraphTester__set_spring_pitch_damping(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_pitch_damping()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_along_stiffness() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_along_stiffness"); return offset; }
static double ls_native_AnimGraphTester__get_spring_along_stiffness(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_along_stiffness())); }
static void ls_native_AnimGraphTester__set_spring_along_stiffness(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_along_stiffness()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_along_damping() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_along_damping"); return offset; }
static double ls_native_AnimGraphTester__get_spring_along_damping(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_along_damping())); }
static void ls_native_AnimGraphTester__set_spring_along_damping(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_along_damping()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_spring_allow_length_flex() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_allow_length_flex"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_spring_allow_length_flex(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_allow_length_flex()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_spring_allow_length_flex(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_allow_length_flex()) = (value != 0); }
static uint16_t ls_offset_AnimGraphTester_spring_gravity() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "spring_gravity"); return offset; }
static double ls_native_AnimGraphTester__get_spring_gravity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_gravity())); }
static void ls_native_AnimGraphTester__set_spring_gravity(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_spring_gravity()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_foot_trace_dist() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "foot_trace_dist"); return offset; }
static double ls_native_AnimGraphTester__get_foot_trace_dist(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_trace_dist())); }
static void ls_native_AnimGraphTester__set_foot_trace_dist(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_trace_dist()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_foot_height_off() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "foot_height_off"); return offset; }
static double ls_native_AnimGraphTester__get_foot_height_off(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_height_off())); }
static void ls_native_AnimGraphTester__set_foot_height_off(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_height_off()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_foot_max_tilt_deg() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "foot_max_tilt_deg"); return offset; }
static double ls_native_AnimGraphTester__get_foot_max_tilt_deg(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_max_tilt_deg())); }
static void ls_native_AnimGraphTester__set_foot_max_tilt_deg(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_max_tilt_deg()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_foot_align_rot() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "foot_align_rot"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_foot_align_rot(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_align_rot()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_foot_align_rot(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_foot_align_rot()) = (value != 0); }
static uint16_t ls_offset_AnimGraphTester_pelvis_interp_speed() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "pelvis_interp_speed"); return offset; }
static double ls_native_AnimGraphTester__get_pelvis_interp_speed(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_pelvis_interp_speed())); }
static void ls_native_AnimGraphTester__set_pelvis_interp_speed(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_pelvis_interp_speed()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_gungrip_frame_eval() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "gungrip_frame_eval"); return offset; }
static int64_t ls_native_AnimGraphTester__get_gungrip_frame_eval(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_gungrip_frame_eval())); }
static void ls_native_AnimGraphTester__set_gungrip_frame_eval(void* self, int64_t value) { lsbind::field<int>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_gungrip_frame_eval()) = static_cast<int>(value); }
static uint16_t ls_offset_AnimGraphTester_cachepose_frame_eval_2() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "cachepose_frame_eval_2"); return offset; }
static int64_t ls_native_AnimGraphTester__get_cachepose_frame_eval_2(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cachepose_frame_eval_2())); }
static void ls_native_AnimGraphTester__set_cachepose_frame_eval_2(void* self, int64_t value) { lsbind::field<int>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cachepose_frame_eval_2()) = static_cast<int>(value); }
static uint16_t ls_offset_AnimGraphTester_generic_alpha() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "generic_alpha"); return offset; }
static double ls_native_AnimGraphTester__get_generic_alpha(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_generic_alpha())); }
static void ls_native_AnimGraphTester__set_generic_alpha(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_generic_alpha()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_transition_easing() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "transition_easing"); return offset; }
static int64_t ls_native_AnimGraphTester__get_transition_easing(void* self) { return lsbind::enum_ordinal(ls_enum_Easing, std::size(ls_enum_Easing), static_cast<int64_t>(lsbind::field<Easing>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_transition_easing()))); }
static void ls_native_AnimGraphTester__set_transition_easing(void* self, int64_t value) { lsbind::field<Easing>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_transition_easing()) = static_cast<Easing>(lsbind::enum_value(ls_enum_Easing, std::size(ls_enum_Easing), value)); }
static uint16_t ls_offset_AnimGraphTester_transition_time() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "transition_time"); return offset; }
static double ls_native_AnimGraphTester__get_transition_time(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_transition_time())); }
static void ls_native_AnimGraphTester__set_transition_time(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_transition_time()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_bs2d_x() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bs2d_x"); return offset; }
static double ls_native_AnimGraphTester__get_bs2d_x(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_x())); }
static void ls_native_AnimGraphTester__set_bs2d_x(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_x()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_bs2d_y() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bs2d_y"); return offset; }
static double ls_native_AnimGraphTester__get_bs2d_y(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_y())); }
static void ls_native_AnimGraphTester__set_bs2d_y(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_y()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_bs2d_manual() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bs2d_manual"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_bs2d_manual(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_manual()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_bs2d_manual(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs2d_manual()) = (value != 0); }
static uint16_t ls_offset_AnimGraphTester_bs_smooth_time() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bs_smooth_time"); return offset; }
static double ls_native_AnimGraphTester__get_bs_smooth_time(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs_smooth_time())); }
static void ls_native_AnimGraphTester__set_bs_smooth_time(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs_smooth_time()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_bs_input_smooth() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "bs_input_smooth"); return offset; }
static double ls_native_AnimGraphTester__get_bs_input_smooth(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs_input_smooth())); }
static void ls_native_AnimGraphTester__set_bs_input_smooth(void* self, double value) { lsbind::field<float>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_bs_input_smooth()) = static_cast<float>(value); }
static uint16_t ls_offset_AnimGraphTester_cached_use_run() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "cached_use_run"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_cached_use_run(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cached_use_run()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_cached_use_run(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cached_use_run()) = (value != 0); }
static uint16_t ls_offset_AnimGraphTester_cached_use_meshspace() { static const uint16_t offset = lsbind::field_offset(AnimGraphTester::get_props(), "cached_use_meshspace"); return offset; }
static uint8_t ls_native_AnimGraphTester__get_cached_use_meshspace(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cached_use_meshspace()) ? 1 : 0); }
static void ls_native_AnimGraphTester__set_cached_use_meshspace(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<AnimGraphTester>(self), ls_offset_AnimGraphTester_cached_use_meshspace()) = (value != 0); }
static void ls_native_fpsLuaBridge_init(void* self) { lsbind::object<fpsLuaBridge>(self)->init(); }
static void ls_native_fpsLuaBridge_start_level_script(void* self) { lsbind::object<fpsLuaBridge>(self)->start_level_script(); }
static void ls_native_fpsLuaBridge_update(void* self) { lsbind::object<fpsLuaBridge>(self)->update(); }
static void ls_native_fpsLuaBridge_imgui_tick(void* self) { lsbind::object<fpsLuaBridge>(self)->imgui_tick(); }
static void ls_native_fpsApp_change_level(void* self, void* next_level) { lsbind::object<fpsApp>(self)->change_level(std::string(lsbind::view(next_level))); }
static void* ls_native_fpsApp_get_player(void* self) { return lsbind::proxy_of(lsbind::object<fpsApp>(self)->get_player()); }
static uint16_t ls_offset_fpsSpawnPoint_other() { static const uint16_t offset = lsbind::field_offset(fpsSpawnPoint::get_props(), "other"); return offset; }
static void* ls_native_fpsSpawnPoint__get_other(void* self) { return lsbind::string(lsbind::field<std::string>(lsbind::object<fpsSpawnPoint>(self), ls_offset_fpsSpawnPoint_other())); }
static void ls_native_fpsSpawnPoint__set_other(void* self, void* value) { lsbind::field<std::string>(lsbind::object<fpsSpawnPoint>(self), ls_offset_fpsSpawnPoint_other()) = std::string(lsbind::view(value)); }
static uint16_t ls_offset_fpsPropPhysics_model() { static const uint16_t offset = lsbind::field_offset(fpsPropPhysics::get_props(), "model"); return offset; }
static void* ls_native_fpsPropPhysics__get_model(void* self) { return lsbind::proxy_of(lsbind::field<Model*>(lsbind::object<fpsPropPhysics>(self), ls_offset_fpsPropPhysics_model())); }
static void ls_native_fpsPropPhysics__set_model(void* self, void* value) { lsbind::field<Model*>(lsbind::object<fpsPropPhysics>(self), ls_offset_fpsPropPhysics_model()) = lsbind::object<Model>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_min_intensity() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "min_intensity"); return offset; }
static double ls_native_fpsFlickeringLightScript__get_min_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_min_intensity())); }
static void ls_native_fpsFlickeringLightScript__set_min_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_min_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_max_intensity() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "max_intensity"); return offset; }
static double ls_native_fpsFlickeringLightScript__get_max_intensity(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_max_intensity())); }
static void ls_native_fpsFlickeringLightScript__set_max_intensity(void* self, double value) { lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_max_intensity()) = static_cast<float>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_radius() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "radius"); return offset; }
static double ls_native_fpsFlickeringLightScript__get_radius(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_radius())); }
static void ls_native_fpsFlickeringLightScript__set_radius(void* self, double value) { lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_radius()) = static_cast<float>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_frequency() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "frequency"); return offset; }
static double ls_native_fpsFlickeringLightScript__get_frequency(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_frequency())); }
static void ls_native_fpsFlickeringLightScript__set_frequency(void* self, double value) { lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_frequency()) = static_cast<float>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_offset() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "offset"); return offset; }
static double ls_native_fpsFlickeringLightScript__get_offset(void* self) { return static_cast<double>(lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_offset())); }
static void ls_native_fpsFlickeringLightScript__set_offset(void* self, double value) { lsbind::field<float>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_offset()) = static_cast<float>(value); }
static uint16_t ls_offset_fpsFlickeringLightScript_octaves() { static const uint16_t offset = lsbind::field_offset(fpsFlickeringLightScript::get_props(), "octaves"); return offset; }
static int64_t ls_native_fpsFlickeringLightScript__get_octaves(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_octaves())); }
static void ls_native_fpsFlickeringLightScript__set_octaves(void* self, int64_t value) { lsbind::field<int>(lsbind::object<fpsFlickeringLightScript>(self), ls_offset_fpsFlickeringLightScript_octaves()) = static_cast<int>(value); }
static uint16_t ls_offset_PhysicsLayerTesterObject_collider_type() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterObject::get_props(), "collider_type"); return offset; }
static int64_t ls_native_PhysicsLayerTesterObject__get_collider_type(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_collider_type())); }
static void ls_native_PhysicsLayerTesterObject__set_collider_type(void* self, int64_t value) { lsbind::field<int>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_collider_type()) = static_cast<int>(value); }
static uint16_t ls_offset_PhysicsLayerTesterObject_bodytype() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterObject::get_props(), "bodytype"); return offset; }
static int64_t ls_native_PhysicsLayerTesterObject__get_bodytype(void* self) { return lsbind::enum_ordinal(ls_enum_BodyType, std::size(ls_enum_BodyType), static_cast<int64_t>(lsbind::field<BodyType>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_bodytype()))); }
static void ls_native_PhysicsLayerTesterObject__set_bodytype(void* self, int64_t value) { lsbind::field<BodyType>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_bodytype()) = static_cast<BodyType>(lsbind::enum_value(ls_enum_BodyType, std::size(ls_enum_BodyType), value)); }
static uint16_t ls_offset_PhysicsLayerTesterObject_is_trigger() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterObject::get_props(), "is_trigger"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterObject__get_is_trigger(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_is_trigger()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterObject__set_is_trigger(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_is_trigger()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterObject_layer() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterObject::get_props(), "layer"); return offset; }
static int64_t ls_native_PhysicsLayerTesterObject__get_layer(void* self) { return static_cast<int64_t>(lsbind::field<int>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_layer())); }
static void ls_native_PhysicsLayerTesterObject__set_layer(void* self, int64_t value) { lsbind::field<int>(lsbind::object<PhysicsLayerTesterObject>(self), ls_offset_PhysicsLayerTesterObject_layer()) = static_cast<int>(value); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer0() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer0"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer0(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer0()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer0(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer0()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer1() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer1"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer1(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer1()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer1(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer1()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer2() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer2"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer2(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer2()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer2(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer2()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer3() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer3"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer3(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer3()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer3(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer3()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer4() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer4"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer4(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer4()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer4(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer4()) = (value != 0); }
static uint16_t ls_offset_PhysicsLayerTesterRaycast_layer5() { static const uint16_t offset = lsbind::field_offset(PhysicsLayerTesterRaycast::get_props(), "layer5"); return offset; }
static uint8_t ls_native_PhysicsLayerTesterRaycast__get_layer5(void* self) { return static_cast<uint8_t>(lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer5()) ? 1 : 0); }
static void ls_native_PhysicsLayerTesterRaycast__set_layer5(void* self, uint8_t value) { lsbind::field<bool>(lsbind::object<PhysicsLayerTesterRaycast>(self), ls_offset_PhysicsLayerTesterRaycast_layer5()) = (value != 0); }
static const char* const ls_declarations[] = {
	R"ls(enum AnimGraphTestMode { BasicIK, LookAt, GunGripIK, FeetIK, BlendMasked, CopyBone, SlotPlaying, Additive, BlendByInt, DurationEventTest, BlendSpace2D, SpringBoneTest, CachedPoseTest }
)ls",
	R"ls(// BikeObject (BikeHeaders.h)
extern class BikeObject : Component {
}
)ls",
	R"ls(// BikeGameApplication (BikeHeaders.h)
extern class BikeGameApplication : Application {
}
)ls",
	R"ls(// AnimGraphTester (AnimGraphTester.h)
extern class AnimGraphTester : Component {
    AnimGraphTestMode mode;
    string prop_bone;
    SoundFile footstep_sfx;
    string bone_ik_upper;
    string bone_ik_end;
    Vec3 look_forward_axis;
    bool use_pole_bone_for_ik;
    float max_stretch;
    float start_stretch_ratio;
    string bone_upper_blend;
    string bone_copy_src;
    string bone_copy_dst;
    string bone_grip_other;
    string bone_grip_mask;
    string bone_spring;
    float spring_yaw_stiffness;
    float spring_yaw_damping;
    float spring_pitch_stiffness;
    float spring_pitch_damping;
    float spring_along_stiffness;
    float spring_along_damping;
    bool spring_allow_length_flex;
    float spring_gravity;
    float foot_trace_dist;
    float foot_height_off;
    float foot_max_tilt_deg;
    bool foot_align_rot;
    float pelvis_interp_speed;
    int gungrip_frame_eval;
    int cachepose_frame_eval_2;
    float generic_alpha;
    Easing transition_easing;
    float transition_time;
    float bs2d_x;
    float bs2d_y;
    bool bs2d_manual;
    float bs_smooth_time;
    float bs_input_smooth;
    bool cached_use_run;
    bool cached_use_meshspace;
    // skipped model: AssetPtr<Model>
    // skipped matoverride: AssetPtr<MaterialInstance>
    // skipped prop: AssetPtr<Model>
    // skipped clip0: AssetPtr<AnimationSeqAsset>
    // skipped clip1: AssetPtr<AnimationSeqAsset>
    // skipped clip2: AssetPtr<AnimationSeqAsset>
    // skipped clip3: AssetPtr<AnimationSeqAsset>
    // skipped slot_clip: AssetPtr<AnimationSeqAsset>
    // skipped footstep_particle: AssetPtr<ParticleAsset>
}
)ls",
	R"ls(// fpsLuaBridge (fpsApp.h)
extern class fpsLuaBridge : ClassBase {
    virtual void init();
    virtual void start_level_script();
    virtual void update();
    virtual void imgui_tick();
}
)ls",
	R"ls(// fpsApp (fpsApp.h)
extern class fpsApp : Application {
    void change_level(string next_level);
    Entity get_player();
}
)ls",
	R"ls(// fpsSpawnPoint (fpsObjects.h)
extern class fpsSpawnPoint : Component {
    string other;
}
)ls",
	R"ls(// fpsPropPhysics (fpsObjects.h)
extern class fpsPropPhysics : Component {
    Model model;
}
)ls",
	R"ls(// fpsFlickeringLightScript (fpsObjects.h)
extern class fpsFlickeringLightScript : Component {
    float min_intensity;
    float max_intensity;
    float radius;
    float frequency;
    float offset;
    int octaves;
    // skipped color: Color32
}
)ls",
	R"ls(// fpsPlayer (fpsObjects.h)
extern class fpsPlayer : Component {
    // skipped get_blah_components(): const std::vector<Component*>
}
)ls",
	R"ls(// PhysicsLayerTesterTriggerScript (fpsTestObjects.h)
extern class PhysicsLayerTesterTriggerScript : Component {
}
)ls",
	R"ls(// PhysicsLayerTesterObject (fpsTestObjects.h)
extern class PhysicsLayerTesterObject : Component {
    int collider_type;
    BodyType bodytype;
    bool is_trigger;
    int layer;
    // skipped model: AssetPtr<Model>
    // skipped trigger: AssetPtr<MaterialInstance>
    // skipped simulate_toggle: BoolButton
    // skipped respawn: BoolButton
}
)ls",
	R"ls(// PhysicsLayerTesterRaycast (fpsTestObjects.h)
extern class PhysicsLayerTesterRaycast : Component {
    bool layer0;
    bool layer1;
    bool layer2;
    bool layer3;
    bool layer4;
    bool layer5;
}
)ls",
};
static lsbind::Symbol const ls_symbols[] = {
	{"ls_native_AnimGraphTester__get_mode", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_mode)},
	{"ls_native_AnimGraphTester__set_mode", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_mode)},
	{"ls_native_AnimGraphTester__get_prop_bone", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_prop_bone)},
	{"ls_native_AnimGraphTester__set_prop_bone", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_prop_bone)},
	{"ls_native_AnimGraphTester__get_footstep_sfx", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_footstep_sfx)},
	{"ls_native_AnimGraphTester__set_footstep_sfx", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_footstep_sfx)},
	{"ls_native_AnimGraphTester__get_bone_ik_upper", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_ik_upper)},
	{"ls_native_AnimGraphTester__set_bone_ik_upper", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_ik_upper)},
	{"ls_native_AnimGraphTester__get_bone_ik_end", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_ik_end)},
	{"ls_native_AnimGraphTester__set_bone_ik_end", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_ik_end)},
	{"ls_native_AnimGraphTester__get_look_forward_axis", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_look_forward_axis)},
	{"ls_native_AnimGraphTester__set_look_forward_axis", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_look_forward_axis)},
	{"ls_native_AnimGraphTester__get_use_pole_bone_for_ik", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_use_pole_bone_for_ik)},
	{"ls_native_AnimGraphTester__set_use_pole_bone_for_ik", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_use_pole_bone_for_ik)},
	{"ls_native_AnimGraphTester__get_max_stretch", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_max_stretch)},
	{"ls_native_AnimGraphTester__set_max_stretch", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_max_stretch)},
	{"ls_native_AnimGraphTester__get_start_stretch_ratio", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_start_stretch_ratio)},
	{"ls_native_AnimGraphTester__set_start_stretch_ratio", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_start_stretch_ratio)},
	{"ls_native_AnimGraphTester__get_bone_upper_blend", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_upper_blend)},
	{"ls_native_AnimGraphTester__set_bone_upper_blend", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_upper_blend)},
	{"ls_native_AnimGraphTester__get_bone_copy_src", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_copy_src)},
	{"ls_native_AnimGraphTester__set_bone_copy_src", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_copy_src)},
	{"ls_native_AnimGraphTester__get_bone_copy_dst", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_copy_dst)},
	{"ls_native_AnimGraphTester__set_bone_copy_dst", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_copy_dst)},
	{"ls_native_AnimGraphTester__get_bone_grip_other", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_grip_other)},
	{"ls_native_AnimGraphTester__set_bone_grip_other", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_grip_other)},
	{"ls_native_AnimGraphTester__get_bone_grip_mask", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_grip_mask)},
	{"ls_native_AnimGraphTester__set_bone_grip_mask", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_grip_mask)},
	{"ls_native_AnimGraphTester__get_bone_spring", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bone_spring)},
	{"ls_native_AnimGraphTester__set_bone_spring", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bone_spring)},
	{"ls_native_AnimGraphTester__get_spring_yaw_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_yaw_stiffness)},
	{"ls_native_AnimGraphTester__set_spring_yaw_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_yaw_stiffness)},
	{"ls_native_AnimGraphTester__get_spring_yaw_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_yaw_damping)},
	{"ls_native_AnimGraphTester__set_spring_yaw_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_yaw_damping)},
	{"ls_native_AnimGraphTester__get_spring_pitch_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_pitch_stiffness)},
	{"ls_native_AnimGraphTester__set_spring_pitch_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_pitch_stiffness)},
	{"ls_native_AnimGraphTester__get_spring_pitch_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_pitch_damping)},
	{"ls_native_AnimGraphTester__set_spring_pitch_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_pitch_damping)},
	{"ls_native_AnimGraphTester__get_spring_along_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_along_stiffness)},
	{"ls_native_AnimGraphTester__set_spring_along_stiffness", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_along_stiffness)},
	{"ls_native_AnimGraphTester__get_spring_along_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_along_damping)},
	{"ls_native_AnimGraphTester__set_spring_along_damping", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_along_damping)},
	{"ls_native_AnimGraphTester__get_spring_allow_length_flex", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_allow_length_flex)},
	{"ls_native_AnimGraphTester__set_spring_allow_length_flex", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_allow_length_flex)},
	{"ls_native_AnimGraphTester__get_spring_gravity", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_spring_gravity)},
	{"ls_native_AnimGraphTester__set_spring_gravity", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_spring_gravity)},
	{"ls_native_AnimGraphTester__get_foot_trace_dist", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_foot_trace_dist)},
	{"ls_native_AnimGraphTester__set_foot_trace_dist", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_foot_trace_dist)},
	{"ls_native_AnimGraphTester__get_foot_height_off", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_foot_height_off)},
	{"ls_native_AnimGraphTester__set_foot_height_off", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_foot_height_off)},
	{"ls_native_AnimGraphTester__get_foot_max_tilt_deg", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_foot_max_tilt_deg)},
	{"ls_native_AnimGraphTester__set_foot_max_tilt_deg", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_foot_max_tilt_deg)},
	{"ls_native_AnimGraphTester__get_foot_align_rot", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_foot_align_rot)},
	{"ls_native_AnimGraphTester__set_foot_align_rot", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_foot_align_rot)},
	{"ls_native_AnimGraphTester__get_pelvis_interp_speed", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_pelvis_interp_speed)},
	{"ls_native_AnimGraphTester__set_pelvis_interp_speed", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_pelvis_interp_speed)},
	{"ls_native_AnimGraphTester__get_gungrip_frame_eval", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_gungrip_frame_eval)},
	{"ls_native_AnimGraphTester__set_gungrip_frame_eval", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_gungrip_frame_eval)},
	{"ls_native_AnimGraphTester__get_cachepose_frame_eval_2", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_cachepose_frame_eval_2)},
	{"ls_native_AnimGraphTester__set_cachepose_frame_eval_2", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_cachepose_frame_eval_2)},
	{"ls_native_AnimGraphTester__get_generic_alpha", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_generic_alpha)},
	{"ls_native_AnimGraphTester__set_generic_alpha", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_generic_alpha)},
	{"ls_native_AnimGraphTester__get_transition_easing", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_transition_easing)},
	{"ls_native_AnimGraphTester__set_transition_easing", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_transition_easing)},
	{"ls_native_AnimGraphTester__get_transition_time", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_transition_time)},
	{"ls_native_AnimGraphTester__set_transition_time", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_transition_time)},
	{"ls_native_AnimGraphTester__get_bs2d_x", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bs2d_x)},
	{"ls_native_AnimGraphTester__set_bs2d_x", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bs2d_x)},
	{"ls_native_AnimGraphTester__get_bs2d_y", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bs2d_y)},
	{"ls_native_AnimGraphTester__set_bs2d_y", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bs2d_y)},
	{"ls_native_AnimGraphTester__get_bs2d_manual", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bs2d_manual)},
	{"ls_native_AnimGraphTester__set_bs2d_manual", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bs2d_manual)},
	{"ls_native_AnimGraphTester__get_bs_smooth_time", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bs_smooth_time)},
	{"ls_native_AnimGraphTester__set_bs_smooth_time", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bs_smooth_time)},
	{"ls_native_AnimGraphTester__get_bs_input_smooth", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_bs_input_smooth)},
	{"ls_native_AnimGraphTester__set_bs_input_smooth", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_bs_input_smooth)},
	{"ls_native_AnimGraphTester__get_cached_use_run", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_cached_use_run)},
	{"ls_native_AnimGraphTester__set_cached_use_run", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_cached_use_run)},
	{"ls_native_AnimGraphTester__get_cached_use_meshspace", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__get_cached_use_meshspace)},
	{"ls_native_AnimGraphTester__set_cached_use_meshspace", reinterpret_cast<const void*>(&ls_native_AnimGraphTester__set_cached_use_meshspace)},
	{"ls_native_fpsLuaBridge_init", reinterpret_cast<const void*>(&ls_native_fpsLuaBridge_init)},
	{"ls_native_fpsLuaBridge_start_level_script", reinterpret_cast<const void*>(&ls_native_fpsLuaBridge_start_level_script)},
	{"ls_native_fpsLuaBridge_update", reinterpret_cast<const void*>(&ls_native_fpsLuaBridge_update)},
	{"ls_native_fpsLuaBridge_imgui_tick", reinterpret_cast<const void*>(&ls_native_fpsLuaBridge_imgui_tick)},
	{"ls_native_fpsApp_change_level", reinterpret_cast<const void*>(&ls_native_fpsApp_change_level)},
	{"ls_native_fpsApp_get_player", reinterpret_cast<const void*>(&ls_native_fpsApp_get_player)},
	{"ls_native_fpsSpawnPoint__get_other", reinterpret_cast<const void*>(&ls_native_fpsSpawnPoint__get_other)},
	{"ls_native_fpsSpawnPoint__set_other", reinterpret_cast<const void*>(&ls_native_fpsSpawnPoint__set_other)},
	{"ls_native_fpsPropPhysics__get_model", reinterpret_cast<const void*>(&ls_native_fpsPropPhysics__get_model)},
	{"ls_native_fpsPropPhysics__set_model", reinterpret_cast<const void*>(&ls_native_fpsPropPhysics__set_model)},
	{"ls_native_fpsFlickeringLightScript__get_min_intensity", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_min_intensity)},
	{"ls_native_fpsFlickeringLightScript__set_min_intensity", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_min_intensity)},
	{"ls_native_fpsFlickeringLightScript__get_max_intensity", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_max_intensity)},
	{"ls_native_fpsFlickeringLightScript__set_max_intensity", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_max_intensity)},
	{"ls_native_fpsFlickeringLightScript__get_radius", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_radius)},
	{"ls_native_fpsFlickeringLightScript__set_radius", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_radius)},
	{"ls_native_fpsFlickeringLightScript__get_frequency", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_frequency)},
	{"ls_native_fpsFlickeringLightScript__set_frequency", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_frequency)},
	{"ls_native_fpsFlickeringLightScript__get_offset", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_offset)},
	{"ls_native_fpsFlickeringLightScript__set_offset", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_offset)},
	{"ls_native_fpsFlickeringLightScript__get_octaves", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__get_octaves)},
	{"ls_native_fpsFlickeringLightScript__set_octaves", reinterpret_cast<const void*>(&ls_native_fpsFlickeringLightScript__set_octaves)},
	{"ls_native_PhysicsLayerTesterObject__get_collider_type", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__get_collider_type)},
	{"ls_native_PhysicsLayerTesterObject__set_collider_type", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__set_collider_type)},
	{"ls_native_PhysicsLayerTesterObject__get_bodytype", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__get_bodytype)},
	{"ls_native_PhysicsLayerTesterObject__set_bodytype", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__set_bodytype)},
	{"ls_native_PhysicsLayerTesterObject__get_is_trigger", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__get_is_trigger)},
	{"ls_native_PhysicsLayerTesterObject__set_is_trigger", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__set_is_trigger)},
	{"ls_native_PhysicsLayerTesterObject__get_layer", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__get_layer)},
	{"ls_native_PhysicsLayerTesterObject__set_layer", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterObject__set_layer)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer0", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer0)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer0", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer0)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer1", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer1)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer1", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer1)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer2", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer2)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer2", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer2)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer3", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer3)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer3", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer3)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer4", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer4)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer4", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer4)},
	{"ls_native_PhysicsLayerTesterRaycast__get_layer5", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__get_layer5)},
	{"ls_native_PhysicsLayerTesterRaycast__set_layer5", reinterpret_cast<const void*>(&ls_native_PhysicsLayerTesterRaycast__set_layer5)},
};
static lsbind::Class const ls_classes[] = {
	{"BikeObject", "Component"},
	{"BikeGameApplication", "Application"},
	{"AnimGraphTester", "Component"},
	{"fpsLuaBridge", "ClassBase"},
	{"fpsApp", "Application"},
	{"fpsSpawnPoint", "Component"},
	{"fpsPropPhysics", "Component"},
	{"fpsFlickeringLightScript", "Component"},
	{"fpsPlayer", "Component"},
	{"PhysicsLayerTesterTriggerScript", "Component"},
	{"PhysicsLayerTesterObject", "Component"},
	{"PhysicsLayerTesterRaycast", "Component"},
};
static const lsbind::Registrar ls_registrar{"MyGameMEGA", ls_declarations, ls_symbols, ls_classes};
