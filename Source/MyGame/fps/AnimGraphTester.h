#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Framework/EnumDefReflection.h"
#include "Assets/IAsset.h"
#include "Animation/AnimationSeqAsset.h"
#include "Render/Model.h"

NEWENUM(AnimGraphTestMode, int) {
    BasicIK,      // IK right hand to moveable target entity
    LookAt,       // agModifyBone head look-at moveable target
    GunGripIK,    // IK right hand constrained to left hand bone with offset
    FeetIK,       // Foot IK via downward raycasts
    BlendMasked,  // Upper/lower body masked blend between two clips
    CopyBone,     // agCopyBone: copy spine rotation to neck bone
    SlotPlaying,  // agSlotPlayer: base idle + periodic one-shot slot clips
    Additive,     // agAddNode: pulsing additive layer
    BlendByInt,   // agBlendByInt: auto-cycling integer state machine
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

    // Animation clips used by the graph modes
    REF AssetPtr<AnimationSeqAsset> clip0;       // primary / idle clip
    REF AssetPtr<AnimationSeqAsset> clip1;       // secondary clip (walk, upper-body, additive)
    REF AssetPtr<AnimationSeqAsset> clip2;       // tertiary clip (BlendByInt state 2)
    REF AssetPtr<AnimationSeqAsset> slot_clip;   // one-shot clip fired into SlotPlaying slot

    // Bone names — adjust to match your model's rig
    REF std::string bone_ik_upper    = "mixamorig:RightForeArm";
    REF std::string bone_ik_end      = "mixamorig:RightHand";
    REF std::string bone_head        = "mixamorig:Head";
    REF std::string bone_foot_l      = "mixamorig:LeftFoot";
    REF std::string bone_foot_r      = "mixamorig:RightFoot";
    REF std::string bone_upper_blend = "mixamorig:Spine2";
    REF std::string bone_copy_src    = "mixamorig:Spine1";
    REF std::string bone_copy_dst    = "mixamorig:Neck";
    REF std::string bone_grip_other  = "mixamorig:LeftHand";

private:
    void rebuild_graph();

    MeshComponent* mesh = nullptr;
    EntityPtr target_entity;

    AnimGraphTestMode last_mode = AnimGraphTestMode::BasicIK;
    float cycle_timer = 0.f;
    float slot_timer  = 0.f;
    int   blend_state = 0;
};
