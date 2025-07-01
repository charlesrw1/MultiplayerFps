#pragma once
#include "RuntimeNodesNew.h"

// inline constant values or StringName which sources a value from variables/curves
using ValueType = variant<bool, int, float, glm::vec3, StringName>;

class agClipNode;
class agGetPoseCtx {
public:
	agGetPoseCtx(const agGetPoseCtx& other);
	agGetPoseCtx(const Model& model);
	~agGetPoseCtx();
	agGetPoseCtx& operator=(const agGetPoseCtx& other) = delete;

	float get_float_var(StringName name) const;
	glm::vec3 get_vec3_var(StringName name) const;
	bool get_bool_var(StringName name) const;
	int get_int_var(StringName name) const;
	bool is_event_active(const ClassTypeInfo& info) const;
	bool did_event_start(const ClassTypeInfo& info) const;
	bool did_event_end(const ClassTypeInfo& info) const;
	float get_time_remaining() const;
	SyncGroupData& find_sync_group(StringName name) const;
	int get_num_bones() const { return get_skeleton().get_num_bones(); }
	void add_playing_clip(agClipNode& clip);

	const MSkeleton& get_skeleton() const;
	const Model& model;
	ScopedPoolPtr<Pose> pose;
	AnimatorObject& object;
	float weight = 1.f;
	float dt = 0.f;
};

class agBaseNode : public ClassBase {
public:
	virtual void reset()=0;
	virtual void get_pose(agGetPoseCtx& ctx)=0;
};
class agClipNode : public agBaseNode {
public:
	AnimationSeqAsset* sequence = nullptr;
	StringName syncGroup;
	sync_opt syncType = sync_opt::Default;
	ValueType speed = 1.f;
	bool looping = true;
	float anim_time = 0.f;
};
class agBlendNode : public agBaseNode {
public:
	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
class agBlendMasked : public agBaseNode {
public:
	agBaseNode* base = nullptr;
	agBaseNode* layered = nullptr;
	ValueType alpha = 0.f;
	// mask
};
class agAddNode : public agBaseNode {
public:
	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
class agIk2Bone : public agBaseNode {
public:
	agBaseNode* input = nullptr;
	ValueType target = glm::vec3(0.f);
	ValueType pole = glm::vec3(0.f);
	ValueType alpha = 0.f;
	StringName bone_name;
	StringName other_bone;
	bool take_rotation_of_other = false;
	bool ik_in_bone_space = false;
};
class agModifyBone : public agBaseNode {
public:
	agBaseNode* input = nullptr;
	ValueType translationVal = glm::vec3(0.f);
	ValueType rotationVal = glm::vec3(0.f);
	ModifyBoneType translation = {};
	ModifyBoneType rotation = {};
	ValueType alpha = 0.f;
	StringName boneName;
};
class agCopyBone : public agBaseNode {
public:
	agBaseNode* input = nullptr;
	StringName sourceBone;
	StringName targetBone;
	ValueType copyTranslation = false;
	ValueType copyRotation = false;
	bool copyBonespace = false;
};

class agBlendByInt : public agBaseNode {
public:
	opt<int> current_input;
	float blending_duration = 0.0;
	float blendTime = 0.2;
	std::vector<agBaseNode*> inputs;
	ValueType integer = 0;
};
class agBlendspace : public agBlendNode {
public:
	ValueType vecInput = 0;
};

// a state machine. override this class to use it
class agStatemachineBase : public agBaseNode {
public:
	virtual void update(bool wantsReset) = 0;
	// each update, use set_pose to set what state is active
	void set_pose(agBaseNode* pose);
	// use set_transition before a pose change to set how it transitions
	void set_transition_parameters(Easing easing, float blend_time);
	// various getters to use in your logic
	bool is_transitioning() const;
	float get_transition_time_left() const;
	bool can_interrupt_transition() const;
	float get_state_duration() const;
private:
	agBaseNode* currentTree = nullptr;
	Pose* blendingOut = nullptr;
	// cur transition
};

// manual playback of animation in the graph
class agSlotPlayer : public agBaseNode {
public:
	bool updateChildrenWhenPlaying = false;
	StringName slotName;
	agBaseNode* input = nullptr;
private:
	Pose* fadingOutPose = nullptr;
};
agStatemachineBase* make_statemachine();
agBaseNode* construct() {
	agStatemachineBase* statemachine = make_statemachine();
	agClipNode* clip = new agClipNode;
	clip->speed = StringName("flMoveSpeed");
	agBlendNode* blender = new agBlendNode;
	blender->input0 = statemachine;
	blender->input1 = clip;

	agSlotPlayer* slot = new agSlotPlayer;
	slot->slotName = StringName("UpperSlot");
	slot->input = blender;

	agIk2Bone* ik = new agIk2Bone;
	ik->input = slot;
	ik->alpha = StringName("flCurveIkHandR");

	agIk2Bone* ik2 = new agIk2Bone;
	ik2->input = ik;
	ik2->alpha = StringName("flCurveIkHandL");

	agSlotPlayer* additive = new agSlotPlayer;

	agAddNode* additiveLayer = new agAddNode;
	additiveLayer->input0 = ik2;
	additiveLayer->input1 = additive;

	return additiveLayer;
}