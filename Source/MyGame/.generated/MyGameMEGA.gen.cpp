// **** GENERATED SOURCE FILE version:1 2026-07-19 15:34:09 ****
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

