--- GENERATED LUA FILE FROM C++ CLASSES v1 2025-07-02 18:17:32
---@class AnimationSeqAsset : IAsset
AnimationSeqAsset = {
}
---@class AnimationEvent : ClassBase
---@field frame integer
---@field frame_duration integer
---@field editor_layer integer
AnimationEvent = {
}
---@class AnimDurationEvent : ClassBase
---@field frame_begin integer
---@field frame_duration integer
---@field editor_layer integer
AnimDurationEvent = {
}
ROOTMOTION_SETTING_KEEP = 0
ROOTMOTION_SETTING_REMOVE = 1
ROOTMOTION_SETTING_ADD_VELOCITY = 2
EASING_LINEAR = 0
EASING_CUBICEASEIN = 1
EASING_CUBICEASEOUT = 2
EASING_CUBICEASEINOUT = 3
EASING_CONSTANT = 4
---@class AnimTreePoseNode : ClassBase
AnimTreePoseNode = {
}
---@class atClipNodeStruct
---@field Clip AnimationSeqAsset|nil
---@field loop boolean
---@field SyncGroup string
---@field SyncOption integer
---@field start_frame integer
atClipNodeStruct = {
}
MODIFYBONETYPE_NONE = 0
MODIFYBONETYPE_MESHSPACE = 1
MODIFYBONETYPE_MESHSPACEADD = 2
MODIFYBONETYPE_LOCALSPACE = 3
MODIFYBONETYPE_LOCALSPACEADD = 4
AASDF_ASDF = 0
SYNC_OPT_DEFAULT = 0
SYNC_OPT_ALWAYSLEADER = 1
SYNC_OPT_ALWAYSFOLLOWER = 2
---@class BoneRenameContainer
---@field remap any
BoneRenameContainer = {
}
---@class BoneReparentContainer
---@field remap any
BoneReparentContainer = {
}
---@class AnimImportSettings
---@field clipName string
---@field otherClipToSubtract string
---@field hasEndCrop boolean
---@field hasStartCrop boolean
---@field cropStart integer
---@field cropEnd integer
---@field fixLoop boolean
---@field makeAdditive boolean
---@field additiveFromSelf boolean
---@field removeLinearVelocity boolean
---@field enableRootMotion boolean
---@field setRootToFirstPose boolean
AnimImportSettings = {
}
---@class ModelImportSettings : ClassBase
---@field srcGlbFile string
---@field lodScreenSpaceSizes any
---@field myMaterials any
---@field useSharedSkeleton boolean
---@field shareSkeletonWithThis Model|nil
---@field keepBones any
---@field curveNames any
---@field additionalAnimationGlbFiles any
---@field animations any
---@field animations_set_fps number
---@field bone_rename BoneRenameContainer
---@field bone_reparent BoneReparentContainer
ModelImportSettings = {
}
---@class BoneMirror
---@field boneA string
---@field boneB string
BoneMirror = {
}
---@class SkeletonMirror : ClassBase
---@field mirrors any
SkeletonMirror = {
}
---@class BoneMaskValue
---@field bone string
---@field weight number
BoneMaskValue = {
}
---@class SkeletonMask : ClassBase
---@field masks any
SkeletonMask = {
}
---@class HackedAsyncAssetRegReindex : IAsset
HackedAsyncAssetRegReindex = {
}
---@class IAsset : ClassBase
IAsset = {
}
---@class BoolButton
BoolButton = {
}
---@class ClassBase
ClassBase = {
}
---@return ClassTypeInfo|nil
function ClassBase:my_type() end
---@return boolean
---@param type ClassTypeInfo|nil
function ClassBase:is_subclass_of(type) end
---@class ClassTypeInfo : ClassBase
ClassTypeInfo = {
}
---@return boolean
---@param info ClassTypeInfo|nil
function ClassTypeInfo:is_subclass_of(info) end
---@return string
function ClassTypeInfo:get_classname() end
---@return ClassTypeInfo|nil
function ClassTypeInfo:get_super_type() end
CURVEPOINTTYPE_LINEAR = 0
CURVEPOINTTYPE_CONSTANT = 1
CURVEPOINTTYPE_SPLITTANGENTS = 2
CURVEPOINTTYPE_ALIGNED = 3
---@class Serializer
Serializer = {
}
LOGCATEGORY_CORE = 0
LOGCATEGORY_GAME = 1
LOGCATEGORY_RENDER = 2
LOGCATEGORY_ANIM = 3
LOGCATEGORY_EDITOR = 4
LOGCATEGORY_UI = 5
LOGCATEGORY_SCRIPT = 6
LOGCATEGORY_PHYSICS = 7
LOGCATEGORY_INPUT = 8
---@class BaseUpdater : ClassBase
BaseUpdater = {
}
function BaseUpdater:destroy_deferred() end
---@class EntityTagString
---@field name string
EntityTagString = {
}
---@class EntityBoneParentString
---@field name string
EntityBoneParentString = {
}
---@class Entity : BaseUpdater
---@field tag EntityTagString
---@field parent_bone EntityBoneParentString
---@field position Vec3
---@field rotation Quat
---@field scale Vec3
---@field editor_name string
Entity = {
}
function Entity:destroy() end
---@return Component|nil
---@param ti ClassTypeInfo|nil
function Entity:get_component(ti) end
---@return Entity|nil
function Entity:get_parent() end
---@return Component|nil
---@param info ClassTypeInfo|nil
function Entity:create_component(info) end
---@return Entity|nil
function Entity:create_child_entity() end
---@param v Vec3
function Entity:set_ls_position(v) end
---@param euler Vec3
function Entity:set_ls_euler_rotation(euler) end
---@param scale Vec3
function Entity:set_ls_scale(scale) end
---@return Vec3
function Entity:get_ws_position() end
---@return Vec3
function Entity:get_ws_scale() end
---@param v Vec3
function Entity:set_ws_position(v) end
---@param parentEntity Entity|nil
function Entity:parent_to(parentEntity) end
---@return boolean
function Entity:has_parent_bone() end
---@return string
function Entity:get_parent_bone() end
---@class Component : BaseUpdater
Component = {
}
function Component:pre_start() end
function Component:start() end
function Component:update() end
function Component:end() end
---@param shouldTick boolean
function Component:set_ticking(shouldTick) end
function Component:destroy() end
---@return Entity|nil
function Component:get_owner() end
---@class SceneAsset : IAsset
SceneAsset = {
}
---@class PrefabAsset : IAsset
PrefabAsset = {
}
---@return PrefabAsset|nil
---@param name string
function PrefabAsset.load(name) end
---@class CarGameMode : Entity
---@field on_player_damaged any
CarGameMode = {
}
---@class WheelComponent : Component
---@field front boolean
---@field left boolean
WheelComponent = {
}
---@class CarComponent : Component
CarComponent = {
}
---@class CarSoundMaker : Component
---@field engineSoundAsset PrefabAsset|nil
---@field tireSoundAsset PrefabAsset|nil
CarSoundMaker = {
}
---@class CarDriver : Component
CarDriver = {
}
---@class ArrowComponent : Component
ArrowComponent = {
}
---@class BillboardComponent : Component
---@field visible boolean
---@field texture Texture|nil
BillboardComponent = {
}
---@class CameraComponent : Component
CameraComponent = {
}
---@class DecalComponent : Component
---@field material MaterialInstance|nil
DecalComponent = {
}
---@class SpotLightComponent : Component
---@field color any
---@field intensity number
---@field radius number
---@field cone_angle number
---@field inner_cone number
---@field cookie_asset Texture|nil
---@field visible boolean
SpotLightComponent = {
}
---@class PointLightComponent : Component
---@field color any
---@field intensity number
---@field radius number
---@field visible boolean
PointLightComponent = {
}
---@class SunLightComponent : Component
---@field color any
---@field intensity number
---@field fit_to_scene boolean
---@field log_lin_lerp_factor number
---@field max_shadow_dist number
---@field epsilon number
---@field z_dist_scaling number
---@field visible boolean
SunLightComponent = {
}
---@class SkylightComponent : Component
---@field recapture_skylight BoolButton
SkylightComponent = {
}
---@class CubemapAnchor
---@field worldspace boolean
---@field p Vec3
CubemapAnchor = {
}
---@class CubemapComponent : Component
---@field recapture BoolButton
---@field anchor CubemapAnchor
CubemapComponent = {
}
---@class MeshBuilderComponent : Component
MeshBuilderComponent = {
}
---@class MeshComponent : Component
---@field model Model|nil
---@field is_visible boolean
---@field cast_shadows boolean
---@field is_skybox boolean
---@field eMaterialOverride any
MeshComponent = {
}
---@param model Model|nil
function MeshComponent:set_model(model) end
---@return Model|nil
function MeshComponent:get_model() end
---@param mi MaterialInstance|nil
function MeshComponent:set_material_override(mi) end
---@return MaterialInstance|nil
function MeshComponent:get_material_override() end
---@param b boolean
function MeshComponent:set_is_visible(b) end
---@class ParticleComponent : Component
---@field continious_rate number
---@field burst_time number
---@field burst_min integer
---@field burst_max integer
---@field gravity number
---@field drag number
---@field life_time_min number
---@field life_time_max number
---@field size_min number
---@field size_max number
---@field size_decay number
---@field start_color any
---@field end_color any
---@field alpha_start number
---@field alpha_end number
---@field intensity number
---@field intensity_decay number
---@field rotation_min number
---@field rotation_max number
---@field rot_vel_min number
---@field rot_vel_max number
---@field spawn_sphere_min number
---@field spawn_sphere_max number
---@field bias Vec3
---@field init_particle_vel_sphere boolean
---@field speed_min number
---@field speed_max number
---@field speed_exp number
---@field radial_vel Vec3
---@field orbital_strength number
---@field inherit_transform boolean
---@field inherit_velocity boolean
---@field particle_mat MaterialInstance|nil
ParticleComponent = {
}
---@class PhysicsBody : Component
---@field on_trigger_start any
---@field physics_layer integer
---@field enabled boolean
---@field simulate_physics boolean
---@field is_static boolean
---@field is_trigger boolean
---@field send_overlap boolean
---@field send_hit boolean
---@field interpolate_visuals boolean
---@field density number
PhysicsBody = {
}
---@class CapsuleComponent : PhysicsBody
---@field height number
---@field radius number
---@field height_offset number
CapsuleComponent = {
}
---@class BoxComponent : PhysicsBody
BoxComponent = {
}
---@class SphereComponent : PhysicsBody
---@field radius number
SphereComponent = {
}
---@class MeshColliderComponent : PhysicsBody
MeshColliderComponent = {
}
---@class JointAnchor
---@field p Vec3
---@field q Quat
JointAnchor = {
}
---@class PhysicsJointComponent : PhysicsBody
---@field target Entity|nil
---@field anchor JointAnchor
---@field local_joint_axis integer
PhysicsJointComponent = {
}
---@class HingeJointComponent : PhysicsJointComponent
HingeJointComponent = {
}
---@class BallSocketJointComponent : PhysicsJointComponent
BallSocketJointComponent = {
}
JM_LOCKED = 0
JM_LIMITED = 1
JM_FREE = 2
---@class AdvancedJointComponent : PhysicsJointComponent
---@field x_motion integer
---@field y_motion integer
---@field z_motion integer
---@field ang_x_motion integer
---@field ang_y_motion integer
---@field ang_z_motion integer
---@field linear_distance_max number
---@field linear_damp number
---@field linear_stiff number
---@field twist_limit_min number
---@field twist_limit_max number
---@field ang_y_limit number
---@field ang_z_limit number
---@field twist_damp number
---@field twist_stiff number
---@field cone_damp number
---@field cone_stiff number
AdvancedJointComponent = {
}
---@class TileMap : Component
---@field tile_size number
---@field show_built_map boolean
TileMap = {
}
---@class DefaultTileMap : TileMap
DefaultTileMap = {
}
---@class BikeEntity : Component
BikeEntity = {
}
---@class HitResult
---@field what Entity|nil
---@field pos Vec3
---@field hit boolean
HitResult = {
}
---@class GameplayStatic : ClassBase
GameplayStatic = {
}
---@return Entity|nil
---@param name string
function GameplayStatic.find_entity(name) end
---@return Entity|nil
---@param prefab PrefabAsset|nil
function GameplayStatic.spawn_prefab(prefab) end
---@return Entity|nil
function GameplayStatic.spawn_entity() end
function GameplayStatic.change_level() end
---@return HitResult
function GameplayStatic.cast_ray() end
---@class LuaInput : ClassBase
LuaInput = {
}
---@return boolean
---@param key integer
function LuaInput.is_key_down(key) end
---@return boolean
---@param key integer
function LuaInput.was_key_pressed(key) end
---@return boolean
---@param key integer
function LuaInput.was_key_released(key) end
---@return boolean
---@param con_button integer
function LuaInput.is_con_button_down(con_button) end
---@return number
---@param con_axis integer
function LuaInput.get_con_axis(con_axis) end
---@class Player : Component
Player = {
}
function Player:do_something() end
---@class TDChest : Component
---@field soundfx SoundFile|nil
---@field openanim AnimationSeqAsset|nil
---@field delayedLoadAnim any
TDChest = {
}
function TDChest:open_chest() end
---@class MyStruct
---@field x number
---@field y number
---@field s string
MyStruct = {
}
---@param d Serializer
function MyStruct:serialize(d) end
---@class TopDownWeaponDataNew : Component
---@field name string
---@field model Model|nil
---@field damage integer
---@field fire_rate number
---@field type integer
---@field pellets integer
---@field accuracy number
---@field bullet_speed number
---@field special_projectile PrefabAsset|nil
---@field values any
TopDownWeaponDataNew = {
}
---@class TopDownPlayer : Component
---@field numbers MyStruct
---@field myarray any
---@field runToStar AnimationSeqAsset|nil
---@field idleToRun AnimationSeqAsset|nil
---@field particlePrefab PrefabAsset|nil
---@field shotgunSoundAsset PrefabAsset|nil
---@field jumpAnim AnimationSeqAsset|nil
---@field jumpSeq AnimationSeqAsset|nil
---@field move_speed number
---@field fov number
---@field shoot_cooldown number
---@field using_third_person_movement boolean
TopDownPlayer = {
}
---@class ComponentWithStruct : Component
---@field target Vec3
---@field things MyStruct
---@field is_happening boolean
---@field list_of_things any
ComponentWithStruct = {
}
---@class TopDownGameManager : Component
---@field player_prefab PrefabAsset|nil
---@field what_component Component|nil
---@field my2nd Component|nil
---@field my3rd Component|nil
TopDownGameManager = {
}
---@class TopDownHealthComponent : Component
---@field on_take_damage any
---@field max_health integer
TopDownHealthComponent = {
}
---@class ProjectileComponent : Component
---@field speed number
---@field damage integer
ProjectileComponent = {
}
---@class EnableRagdollScript : Component
EnableRagdollScript = {
}
---@class TopDownEnemyComponent : Component
TopDownEnemyComponent = {
}
---@class TopDownCameraReg : Component
TopDownCameraReg = {
}
---@class TopDownSpawner : Component
---@field prefab PrefabAsset|nil
---@field count integer
TopDownSpawner = {
}
function TopDownSpawner:enable_object() end
---@class TDSpawnOverTime : Component
---@field prefab PrefabAsset|nil
---@field spawn_interval number
---@field max_count integer
TDSpawnOverTime = {
}
---@class TopDownSpawnPoint : Component
TopDownSpawnPoint = {
}
---@class TPGameMode : Component
---@field playerPrefab PrefabAsset|nil
TPGameMode = {
}
---@return TPPlayer|nil
function TPGameMode:get_player() end
---@class TPPlayer : Component
---@field jump_seq AnimationSeqAsset|nil
---@field idle_to_run_seq AnimationSeqAsset|nil
---@field run_to_idle_seq AnimationSeqAsset|nil
---@field projectile PrefabAsset|nil
---@field shotgunSound PrefabAsset|nil
TPPlayer = {
}
---@class TPProjectile : Component
---@field speed number
---@field damage integer
TPProjectile = {
}
---@class LevelSerializationContext : ClassBase
LevelSerializationContext = {
}
---@class SerializeEntitiesContainer : ClassBase
SerializeEntitiesContainer = {
}
PL_DEFAULT = 0
PL_STATICOBJECT = 1
PL_DYNAMICOBJECT = 2
PL_PHYSICSOBJECT = 3
PL_CHARACTER = 4
PL_VISIBLITY = 5
---@class MaterialInstance : IAsset
MaterialInstance = {
}
---@class Model : IAsset
Model = {
}
---@class Texture : IAsset
Texture = {
}
---@class TextureImportSettings : ClassBase
---@field is_generated boolean
---@field src_file string
---@field is_normalmap boolean
---@field is_srgb boolean
TextureImportSettings = {
}
---@class ScriptComponent : Component
---@field ctor string
ScriptComponent = {
}
---@class SoundComponent : Component
---@field minRadius number
---@field maxRadius number
---@field attenuation integer
---@field attenuate boolean
---@field spatialize boolean
---@field looping boolean
---@field sound SoundFile|nil
---@field enable_on_start boolean
---@field editor_test_sound BoolButton
SoundComponent = {
}
function SoundComponent:play_one_shot() end
---@param f number
function SoundComponent:set_pitch(f) end
---@param b boolean
function SoundComponent:set_play(b) end
SNDATN_LINEAR = 0
SNDATN_CUBIC = 1
SNDATN_INVCUBIC = 2
---@class SoundFile : IAsset
SoundFile = {
}
GUIALIGNMENT_LEFT = 0
GUIALIGNMENT_CENTER = 1
GUIALIGNMENT_RIGHT = 2
GUIALIGNMENT_FILL = 3
GUIANCHOR_TOPLEFT = 0
GUIANCHOR_TOPRIGHT = 1
GUIANCHOR_BOTLEFT = 2
GUIANCHOR_BOTRIGHT = 3
GUIANCHOR_CENTER = 4
GUIANCHOR_TOP = 5
GUIANCHOR_BOTTOM = 6
GUIANCHOR_RIGHT = 7
GUIANCHOR_LEFT = 8
---@class GuiFont : IAsset
GuiFont = {
}
---@class MyTestStruct
---@field str string
---@field integer integer
---@field cond boolean
MyTestStruct = {
}
---@class TestClass : ClassBase
---@field structure MyTestStruct
---@field someint integer
TestClass = {
}
