#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Framework/EnumDefReflection.h"
#include "Assets/IAsset.h"
#include "Animation/AnimationSeqAsset.h"
#include "Render/Model.h"
#include "../../Game/Particles/ParticleAsset.h"
#include "../../Render/MaterialPublic.h"
NEWENUM(AnimGraphTestMode, int) {
    BasicIK,      // IK right hand to moveable target entity
    LookAt,       // agModifyBone head look-at moveable target
    GunGripIK,    // IK right hand constrained to left hand bone with offset
    FeetIK,       // Foot grounding: IK each foot to a target = animated foot pos with Y regrounded by a downward raycast (+ optional surface-tilt)
    BlendMasked,  // Upper/lower body masked blend between two clips
    CopyBone,     // agCopyBone: copy spine rotation to neck bone
    SlotPlaying,  // agSlotPlayer: base idle + periodic one-shot slot clips
    Additive,     // agAddNode: pulsing additive layer
    BlendByInt,   // agBlendByInt: auto-cycling integer state machine
    DurationEventTest,
};

class MeshComponent;
class AnimatorObject;

class AnimGraphTester : public Component
{
public:
    CLASS_BODY(AnimGraphTester, spawnable);

    AnimGraphTester();
    void start()  override;
    void stop()   override;
    void update() override;
    void editor_start() override;
    void editor_on_change_property() override;

    REF AnimGraphTestMode mode = AnimGraphTestMode::BasicIK;

    REF AssetPtr<Model> model;
	REF AssetPtr<MaterialInstance> matoverride;

    // Optional prop: when set, a transient child MeshComponent entity is spawned and
    // parented to prop_bone on the character (e.g. a rifle on the right hand) for testing.
    REF AssetPtr<Model> prop;
    REFLECT(type = BoneNameString)
    std::string prop_bone = "mixamorig:RightHand";

    // Animation clips used by the graph modes
    REF AssetPtr<AnimationSeqAsset> clip0;       // primary / idle clip
    REF AssetPtr<AnimationSeqAsset> clip1;       // secondary clip (walk, upper-body, additive)
    REF AssetPtr<AnimationSeqAsset> clip2;       // tertiary clip (BlendByInt state 2)
    REF AssetPtr<AnimationSeqAsset> slot_clip;   // one-shot clip fired into SlotPlaying slot
	REF AssetPtr<ParticleAsset> footstep_particle;

    // Bone names — use the dropdown picker to select from the attached model's skeleton
    REFLECT(type = BoneNameString)
    std::string bone_ik_upper    = "mixamorig:RightForeArm";
    REFLECT(type = BoneNameString)
    std::string bone_ik_end      = "mixamorig:RightHand";
    REFLECT(type = BoneNameString)
    std::string bone_head        = "mixamorig:Head";
    // LookAt: the head bone's local "face" axis. Skeleton dependent (a head has no intrinsic
    // computable facing), so it is exposed for in-editor tuning -- the look-at aims THIS local
    // axis at the target. If the head is 90 deg off, try the X axes; if 180, negate; if it
    // rolls, the perpendicular axis is slightly off. This model wants +X.
    REF glm::vec3 look_forward_axis = glm::vec3(1.f, 0.f, 0.f);
    REFLECT(type = BoneNameString)
    std::string bone_foot_l      = "mixamorig:LeftFoot";
    REFLECT(type = BoneNameString)
    std::string bone_foot_r      = "mixamorig:RightFoot";

    REFLECT(type = BoneNameString)
	std::string bone_hand_l;
	REFLECT(type = BoneNameString)
	std::string bone_hand_r;
	REFLECT(type = BoneNameString)
	std::string ik_hand_l;
	REFLECT(type = BoneNameString)
	std::string ik_hand_r;

    REFLECT(type = BoneNameString)
	std::string ik_foot_l = "mixamorig:LeftFoot";
	REFLECT(type = BoneNameString)
	std::string ik_foot_r = "mixamorig:RightFoot";

    REFLECT(type = BoneNameString)
    std::string bone_pelvis = "mixamorig:Hips";   // dropped down so reaching feet don't over-extend

    REFLECT(type = BoneNameString)
    std::string bone_upper_blend = "mixamorig:Spine2";
    REFLECT(type = BoneNameString)
    std::string bone_copy_src    = "mixamorig:Spine1";
    REFLECT(type = BoneNameString)
    std::string bone_copy_dst    = "mixamorig:Neck";
    REFLECT(type = BoneNameString)
    std::string bone_grip_other  = "mixamorig:LeftHand";
    // GunGripIK: the additive upper-body motion is masked off this bone + its children
    // (the right arm chain the grip IK drives), so the additive doesn't fight the IK.
    REFLECT(type = BoneNameString)
    std::string bone_grip_mask   = "mixamorig:RightArm";

    // FeetIK grounding tuning
    REF float foot_trace_dist   = 0.5f;   // half-length of the downward probe (meters), centered on foot
    REF float foot_height_off   = 0.05f;  // keep foot this far above the hit surface (meters)
    REF float foot_max_tilt_deg = 45.f;   // clamp surface-align rotation
    REF bool  foot_align_rot    = true;   // rotate foot to match surface normal (modify-bone after IK)
    REF float pelvis_interp_speed = 10.f; // pelvis-drop smoothing rate (higher = snappier)

    REF int gungrip_frame_eval = 0;

    REF float generic_alpha = 1.f;

    REF Easing transition_easing = Easing::CubicEaseIn;
	REF float transition_time = 0.4f;

    void start_ik_dump();  // called externally (console command)

private:



    void rebuild_graph();
    void rebuild_prop();
    void dump_ik_frame(int frame_num);

    MeshComponent* mesh = nullptr;
    EntityPtr target_entity;
    EntityPtr prop_entity;

    AnimGraphTestMode last_mode = AnimGraphTestMode::BasicIK;
    float cycle_timer = 0.f;
    float slot_timer  = 0.f;
    int   blend_state = 0;
    float pelvis_offset = 0.f;   // FeetIK: current interpolated pelvis Y drop (<= 0)

    float ik_dump_remaining = 0.f;
    int   ik_dump_frame     = 0;
};
