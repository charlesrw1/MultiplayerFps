#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Framework/EnumDefReflection.h"
#include "Assets/IAsset.h"
#include "Animation/AnimationSeqAsset.h"
#include "Render/Model.h"
#include "../../Game/Particles/ParticleAsset.h"
#include "../../Render/MaterialPublic.h"
#include <vector>
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
    BlendSpace2D,   // agBlendSpace2D: clip0..clip3 at the 4 corners of a square parameter grid
    SpringBoneTest, // plays clip0 and applies a spring bone to bone_spring, tuned by spring_stiffness/spring_damping
    // agSaveCachedPose/agUseCachedPose: lower body (clip0/clip1 blend-by-int) and upper body
    // (clip2/clip3 single-frame poses blended by generic_alpha) are each evaluated once and
    // cached, then read by TWO agBlendMasked combines (mesh-space vs local-space rotation
    // blend on the masked bones) which a top-level blend-by-int cross-fades between.
    CachedPoseTest,
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
    REF AssetPtr<AnimationSeqAsset> clip2;       // tertiary clip (BlendByInt state 2 / BlendSpace2D corner)
    REF AssetPtr<AnimationSeqAsset> clip3;       // quaternary clip (BlendSpace2D corner)
    REF AssetPtr<AnimationSeqAsset> slot_clip;   // one-shot clip fired into SlotPlaying slot
	REF AssetPtr<ParticleAsset> footstep_particle;

    // Bone names — use the dropdown picker to select from the attached model's skeleton
    REFLECT(type = BoneNameString)
    std::string bone_ik_upper    = "mixamorig:RightForeArm";
    REFLECT(type = BoneNameString)
    std::string bone_ik_end      = "mixamorig:RightHand";

    // LookAt: the head bone's local "face" axis. Skeleton dependent (a head has no intrinsic
    // computable facing), so it is exposed for in-editor tuning -- the look-at aims THIS local
    // axis at the target. If the head is 90 deg off, try the X axes; if 180, negate; if it
    // rolls, the perpendicular axis is slightly off. This model wants +X.

    // ALSO: used for IK as offset for pole bone
    REF glm::vec3 look_forward_axis = glm::vec3(1.f, 0.f, 0.f);
   

    REF bool use_pole_bone_for_ik = true;

    // Two-bone IK limb stretching. <= 1.0 disables stretching (limb stays fixed-length and
    // stops short when unreachable); above 1.0 is the max length multiplier allowed.
    REF float max_stretch = 1.f;
    // Fraction (0-1) of natural reach at which stretching starts ramping in, before the limb
    // is fully extended. Lower than 1 avoids a pop/lock right at full extension. Only matters
    // when max_stretch > 1.
    REF float start_stretch_ratio = 1.f;


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

    // SpringBoneTest
    REFLECT(type = BoneNameString)
    std::string bone_spring = "mixamorig:Spine2";
    REF float spring_yaw_stiffness   = 100.f;
    REF float spring_yaw_damping     = 8.f;
    REF float spring_pitch_stiffness = 100.f;
    REF float spring_pitch_damping   = 8.f;
    REF float spring_along_stiffness = 100.f;
    REF float spring_along_damping   = 8.f;
    REF bool  spring_allow_length_flex = false;
    REF float spring_gravity   = 0.f;

    // FeetIK grounding tuning
    REF float foot_trace_dist   = 0.5f;   // half-length of the downward probe (meters), centered on foot
    REF float foot_height_off   = 0.05f;  // keep foot this far above the hit surface (meters)
    REF float foot_max_tilt_deg = 45.f;   // clamp surface-align rotation
    REF bool  foot_align_rot    = true;   // rotate foot to match surface normal (modify-bone after IK)
    REF float pelvis_interp_speed = 10.f; // pelvis-drop smoothing rate (higher = snappier)

    REF int gungrip_frame_eval = 0;
	REF int cachepose_frame_eval_2 = 0;


    REF float generic_alpha = 1.f;

    REF Easing transition_easing = Easing::CubicEaseIn;
	REF float transition_time = 0.4f;

    // BlendSpace2D: clip0=(-1,-1) clip1=(1,-1) clip2=(-1,1) clip3=(1,1). When bs2d_manual is
    // false the input auto-sweeps in a circle; the editor preview UI (see
    // AnimGraphTesterEditorUI) sets bs2d_manual=true and writes bs2d_x/y directly when the
    // user drags the marker.
    REF float bs2d_x = 0.f;
    REF float bs2d_y = 0.f;
    REF bool bs2d_manual = false;

	REF float bs_smooth_time = 0.0;
	REF float bs_input_smooth = 0.0;

    // CachedPoseTest: manual toggles (instead of auto-cycling on a timer).
    REF bool cached_use_run       = false; // lower body: false = clip0 (walk), true = clip1 (run)
    REF bool cached_use_meshspace = false; // final toggle: false = local-space masked blend, true = mesh-space

	void start_ik_dump(); // called externally (console command)

#ifdef EDITOR_BUILD
    std::unique_ptr<IComponentEditorUi> create_editor_ui() final;
    void editor_on_draw_gizmos_selected() final;
#endif

private:



    void rebuild_graph();
    void rebuild_prop();
    void dump_ik_frame(int frame_num);

    MeshComponent* mesh = nullptr;
    EntityPtr target_entity;
    class BillboardComponent* target_billboard = nullptr; // blue_poi marker on target_entity; only relevant to IK modes that use the target
    EntityPtr prop_entity;

    AnimGraphTestMode last_mode = AnimGraphTestMode::BasicIK;
    float cycle_timer = 0.f;
    float slot_timer  = 0.f;
    int   blend_state = 0;
    float pelvis_offset = 0.f;   // FeetIK: current interpolated pelvis Y drop (<= 0)

    float ik_dump_remaining = 0.f;
    int   ik_dump_frame     = 0;

    // Pole/joint-target config of each agIk2Bone allocated by the last rebuild_graph(),
    // mirroring agIk2Bone::pole/pole_bone/pole_in_bone_space. Populated at the same spot
    // each IK node's pole is set so it can't drift out of sync; consumed by
    // editor_on_draw_gizmos_selected() to draw where the pole vector actually resolves to.
    struct PoleTargetVis {
        StringName pole_bone;      // unused when in_bone_space is false
        glm::vec3  pole = glm::vec3(0.f);
        bool       in_bone_space  = false;
    };
    std::vector<PoleTargetVis> pole_vis;
};
