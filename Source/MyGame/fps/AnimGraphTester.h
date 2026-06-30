#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include "Framework/EnumDefReflection.h"
#include "Assets/IAsset.h"
#include "Animation/AnimationSeqAsset.h"

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
class Model;

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

    // Clip names — set to clips that exist on your model
    REF std::string clip0 = "idle";
    REF std::string clip1 = "walk";
    REF std::string clip2 = "run_forward_unequip";

    // Bone names — default to Mixamo rig; adjust to match animman.cmdl
    REF std::string bone_ik_upper  = "mixamorig:RightForeArm";
    REF std::string bone_ik_end    = "mixamorig:RightHand";
    REF std::string bone_head      = "mixamorig:Head";
    REF std::string bone_foot_l    = "mixamorig:LeftFoot";
    REF std::string bone_foot_r    = "mixamorig:RightFoot";
    REF std::string bone_upper_blend = "mixamorig:Spine2";
    REF std::string bone_copy_src  = "mixamorig:Spine1";
    REF std::string bone_copy_dst  = "mixamorig:Neck";
    REF std::string bone_grip_other = "mixamorig:LeftHand";  // for GunGripIK other_bone
    REF AssetPtr<AnimationSeqAsset> slot_clip;               // clip played into slot on timer

private:
    void rebuild_graph();

    // returns name if it exists in the model's skeleton, else the first available clip name
    std::string safe_clip(const Model* m, const std::string& name) const;

    MeshComponent* mesh = nullptr;
    EntityPtr target_entity;

    AnimGraphTestMode last_mode = AnimGraphTestMode::BasicIK;
    float cycle_timer = 0.f;
    float slot_timer  = 0.f;
    int   blend_state = 0;
};
