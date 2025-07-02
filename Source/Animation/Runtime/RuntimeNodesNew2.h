#pragma once
#include "RuntimeNodesNew.h"

// inline constant values or StringName which sources a value from variables/curves

class agGetPoseCtx;
struct ValueType {
	ValueType(float f) : value(f) {}
	ValueType(bool b) : value(b) {}
	ValueType(int i) : value(i) {}
	ValueType(glm::vec3 v) : value(v) {}
	ValueType(StringName name) : value(name) {}

	// throws on failure
	float get_float(agGetPoseCtx& ctx);
	int get_int(agGetPoseCtx& ctx);
	int get_bool(agGetPoseCtx& ctx);
	glm::vec3 get_vec3(agGetPoseCtx& ctx);

	variant<bool, int, float, glm::vec3, StringName> value;
};

class agSampledAnimEvents {
public:
	struct Sampled {
		enum DurationType {
			Started,
			Ended,
			Active,
		};
		struct DurationEv {
			const AnimDurationEvent* ptr = nullptr;
			Percentage thru;
			DurationType type{};
		};

		Sampled(const AnimationEvent& ev);
		Sampled(DurationEv sampled);
		float weight = 1.f;
		bool is_duration = false;
		bool is_nameid_event = false;
		bool ignore = false;
		union {
			DurationEv durEvent;
			const AnimationEvent* instEvent;
		};
	};
	void blend_weights(int start, int end, float weight);
	void mark_as_ignored(int start, int end);
	using EventInfo = variant<StringName, const ClassTypeInfo*>;
	bool did_event_start(EventInfo what);
	bool did_event_end(EventInfo what);
	bool is_duration_event_active(EventInfo what);
	Percentage get_duration_event_thru(EventInfo what);

	vector<Sampled> events;
};


class agClipNode;
class agGetPoseCtx {
public:

	agGetPoseCtx(AnimatorObject& obj, Pool_Allocator<Pose>& allocator, float dt)
		: object(obj), pose(allocator.allocate_scoped()), dt(dt) {
	}
	agGetPoseCtx(const agGetPoseCtx& other)
	: object(other.object), pose(other.pose.get_parent().allocate_scoped()), dt(other.dt){

	}

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
	void add_playing_clip(agClipNode* clip) { object.add_playing_clip(clip); }
	MSkeleton& get_skeleton() const { return *object.get_model().get_skel(); }
	void debug_enter(string msg) { object.debug_enter_node(msg); }
	void debug_exit() { object.debug_exit_node(); }

	ScopedPoolPtr<Pose> pose;
	AnimatorObject& object;
	//agSampledAnimEvents& events;
	float weight = 1.f;
	float dt = 0.f;
};

class agBaseNode : public ClassBase {
public:
	virtual void reset()=0;
	virtual void get_pose(agGetPoseCtx& ctx)=0;
};
struct BoneIndexRetargetMap;
class agClipNode : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	void set_clip(const Model& m, const string& clipName);
	void set_clip(const AnimationSeqAsset* asset);

	StringName syncGroup;
	sync_opt syncType = sync_opt::Default;
	ValueType speed = 1.f;
	bool looping = true;
private:
	const Model* clipFrom = nullptr;
	const AnimationSeq* seq = nullptr;
	const BoneIndexRetargetMap* remap = nullptr;
	float anim_time = 0.f;
	bool has_init = false;
};

class agBlendNode : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
class agBlendMasked : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
	bool meshspace_blend = false;

	void init_mask_for_model(const Model& model, float default_weight);
	void set_all_children_weights(const Model& model, StringName bone, float weight);
	void set_one_bone_weight(const Model& model, StringName bone, float weight);
private:
	std::vector<float> maskWeights;
};
class agAddNode : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
class agIk2Bone : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	agBaseNode* input = nullptr;
	ValueType target = glm::vec3(0.f);
	ValueType pole = glm::vec3(0.f);
	ValueType alpha = 0.f;
	StringName bone_name;
	StringName other_bone;
	bool take_rotation_of_other = false;
	bool ik_in_bone_space = false;

private:
	bool has_init = false;
	int bone_idx = -1;
	int other_bone_idx = -1;
};
class agModifyBone : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	agBaseNode* input = nullptr;
	ValueType translationVal = glm::vec3(0.f);
	ValueType rotationVal = glm::vec3(0.f);
	ModifyBoneType translation = {};
	ModifyBoneType rotation = {};
	ValueType alpha = 0.f;
	StringName boneName;
private:
	bool has_init = false;
	int bone_index = -1;
};
class agCopyBone : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	agBaseNode* input = nullptr;
	StringName sourceBone;
	StringName targetBone;
	ValueType copyTranslation = false;
	ValueType copyRotation = false;
	bool copyBonespace = false;
private:
	bool has_init = false;
	int source_bone_idx = -1;
	int target_bone_idx = -1;

};

class agBlendspace : public agBlendNode {
public:
	ValueType vecInput = 0;
};

// a state machine. override this class to use it ( or use blend by int or slot )
// flow:
//		get_pose()
//			evaluates current tree if not null
//			blends cur transition if transitioning
//			update() -> sets new tree
//			checks for transition starts

class agStatemachineBase : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	virtual void update(agGetPoseCtx& ctx, bool wantsReset) = 0;
	// each update, use set_pose to set what state is active
	void set_pose(agBaseNode* pose);
	// use set_transition before a pose change to set how it transitions
	void set_transition_parameters(Easing easing, float blend_time);
	// various getters to use in your logic
	bool is_transitioning() const { return blendingOut != nullptr; }
	float get_transition_time_left() const { return curTransitionDuration - curTransitionTime; }
	float get_transition_percent() const { return curTransitionTime / curTransitionDuration; }
	float get_state_duration() const { return curTime; }
private:
	float curTime = 0.0;
	agBaseNode* currentTree = nullptr;
	// cur transition
	Easing curTransition{};
	float curTransitionDuration = 0.0;
	float curTransitionTime = 0.0;
	Pose* blendingOut = nullptr;
};

struct DirectAnimationSlot;
class agSlotClipInternal : public agBaseNode {
public:
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	DirectAnimationSlot* slot = nullptr;
};
// manual playback of animation in the graph
class agSlotPlayer : public agStatemachineBase {
public:
	void update(agGetPoseCtx& ctx, bool wantsReset) final;
	bool updateChildrenWhenPlaying = false;
	StringName slotName;
	agBaseNode* input = nullptr;
private:
	agSlotClipInternal clipPlayer;
	Pose* fadingOutPose = nullptr;
};
class agBlendByInt : public agStatemachineBase {
public:
	void update(agGetPoseCtx& ctx, bool wantsReset) final;
	Easing easing = Easing::CubicEaseIn;
	float blending_duration = 0.5;
	std::vector<agBaseNode*> inputs;
	ValueType integer = 0;
};

#if 0
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
#endif