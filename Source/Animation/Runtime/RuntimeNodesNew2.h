#pragma once
#include "RuntimeNodesNew.h"

// inline constant values or StringName which sources a value from variables/curves

class agGetPoseCtx;
struct ValueType
{
	ValueType(float f) : value(f) {}
	ValueType(bool b) : value(b) {}
	ValueType(int i) : value(i) {}
	ValueType(glm::vec3 v) : value(v) {}
	ValueType(glm::quat q) : value(q) {}
	ValueType(StringName name) : value(name) {}
	ValueType(const char* name) : value(StringName(name)) {}


	// throws on failure
	float get_float(agGetPoseCtx& ctx);
	int get_int(agGetPoseCtx& ctx);
	int get_bool(agGetPoseCtx& ctx);
	glm::vec3 get_vec3(agGetPoseCtx& ctx);
	// Resolves to a quaternion. An inline/variable vec3 is treated as euler radians.
	glm::quat get_quat(agGetPoseCtx& ctx);

	variant<bool, int, float, glm::vec3, glm::quat, StringName> value;
};


//
class agClipNode;
class agGetPoseCtx : public ClassBase
{
public:
	CLASS_BODY(agGetPoseCtx);

	agGetPoseCtx(AnimatorObject& obj, Pool_Allocator<Pose>& allocator, float dt)
		: object(obj), pose(allocator.allocate_scoped()), dt(dt) {}
	agGetPoseCtx(const agGetPoseCtx& other)
		: object(other.object), pose(other.pose.get_parent().allocate_scoped()), dt(other.dt) {}

	agGetPoseCtx& operator=(const agGetPoseCtx& other) = delete;

	float get_float_var(StringName name) const;
	glm::vec3 get_vec3_var(StringName name) const;
	glm::quat get_quat_var(StringName name) const;
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
	// agSampledAnimEvents& events;
	float weight = 1.f;
	float dt = 0.f;
};

class agBaseNode : public ClassBase
{
public:
	CLASS_BODY(agBaseNode);
	virtual void reset() = 0;
	virtual void get_pose(agGetPoseCtx& ctx) = 0;

	// Called directly on every node in agBuilder::get_all_nodes() (see
	// AnimatorObject::refresh_after_model_reload) when a Model that the animator
	// references (its own model or any clipFrom) has been hot-reloaded. No tree
	// descent needed -- the flat node list already reaches every leaf, including
	// ones on inactive statemachine branches. Only leaf nodes that cache pointers
	// (seq, remap) or bone indices need to override; default is a no-op.
	virtual void refresh_after_model_reload(Model* reloaded) {}
};
struct BoneIndexRetargetMap;
class agClipNode : public agBaseNode
{
public:
	CLASS_BODY(agClipNode);

	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	void refresh_after_model_reload(Model* reloaded) final;
	REF void set_clip(const Model* m, string clipName);
	void set_clip(const AnimationSeqAsset* asset);
	REF void set_looping(bool b) { looping = b; }

	StringName syncGroup;
	sync_opt syncType = sync_opt::Default;
	ValueType speed = 1.f;
	bool looping = true;

private:
	const Model* clipFrom = nullptr;
	const AnimationSeq* seq = nullptr;
	const BoneIndexRetargetMap* remap = nullptr;
	string clip_name; // remembered so refresh_after_model_reload can re-find_clip() after reload
	float anim_time = 0.f;
	bool has_init = false;
};
class agEvaluateClip : public agBaseNode
{
public:
	CLASS_BODY(agEvaluateClip);

	void reset() final {}
	void get_pose(agGetPoseCtx& ctx) final;
	void refresh_after_model_reload(Model* reloaded) final;
	void set_clip(const AnimationSeqAsset* asset);

	int frame = 0;

	const Model* clipFrom = nullptr;
	const AnimationSeq* seq = nullptr;
	const BoneIndexRetargetMap* remap = nullptr;
	string clip_name; // remembered so refresh_after_model_reload can re-find_clip() after reload
	bool has_init = false;
};

// Leaf node that outputs the skeleton's bind (reference) pose. Useful as the base
// of an agAddNode when previewing an additive clip (additive delta on top of bind).
class agBindPose : public agBaseNode
{
public:
	CLASS_BODY(agBindPose);
	void reset() final {}
	void get_pose(agGetPoseCtx& ctx) final;
};

class agBlendNode : public agBaseNode
{
public:
	CLASS_BODY(agBlendNode);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	REF void set_inputs(agBaseNode* inp0, agBaseNode* inp1) {
		this->input0 = inp0;
		this->input1 = inp1;
	}
	REF void set_alpha_const(float f) { alpha = f; }
	REF void set_alpha_var(string name) { alpha = StringName(name.c_str()); }

	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
class agBlendMasked : public agBaseNode
{
public:
	CLASS_BODY(agBlendMasked);

	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	REF void set_inputs(agBaseNode* inp0, agBaseNode* inp1) {
		this->input0 = inp0;
		this->input1 = inp1;
	}
	REF void set_alpha_const(float f) { alpha = f; }
	REF void set_alpha_var(string name) { alpha = StringName(name.c_str()); }
	REF void set_meshspace_blend(bool b) { this->meshspace_blend = b; }

	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
	bool meshspace_blend = false;

	REF void init_mask_for_model(const Model* model, float default_weight);
	REF void set_all_children_weights(const Model* model, string bone, float weight);
	REF void set_one_bone_weight(const Model* model, string bone, float weight);

private:
	std::vector<float> maskWeights;
};
class agAddNode : public agBaseNode
{
public:
	CLASS_BODY(agAddNode);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	agBaseNode* input0 = nullptr;
	agBaseNode* input1 = nullptr;
	ValueType alpha = 0.f;
};
// Builds an additive (delta) pose at runtime: output = input - reference, where
// `reference` is typically the first frame of the same motion clip (agEvaluateClip
// frame 0). The delta can then be applied with agAddNode. Bones flagged via
// mask_bone_and_children() have their delta zeroed (identity rot, zero pos/scale),
// so adding the result leaves those bones untouched -- used to keep an additive
// upper-body motion off the IK arm chain.
// FIXME: the reference subtraction should be baked into the asset (is_additive_clip)
//        instead of recomputed every frame.
class agMakeAdditive : public agBaseNode
{
public:
	CLASS_BODY(agMakeAdditive);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	REF void init_mask(const Model* model);                            // all bones contribute by default
	REF void mask_bone_and_children(const Model* model, string bone);  // zero the delta for bone + descendants

	agBaseNode* input = nullptr;      // the motion clip
	agBaseNode* reference = nullptr;  // pose subtracted from input (e.g. first frame of the motion)

private:
	std::vector<uint8_t> masked;      // 1 = delta zeroed (bone left untouched by the add)
};
class agIk2Bone : public agBaseNode
{
public:
	CLASS_BODY(agIk2Bone);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	void refresh_after_model_reload(Model* reloaded) final;

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
}; //
class agModifyBone : public agBaseNode
{
public:
	CLASS_BODY(agModifyBone);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	void refresh_after_model_reload(Model* reloaded) final;

	agBaseNode* input = nullptr;
	ValueType translationVal = glm::vec3(0.f);
	ValueType rotationVal = glm::vec3(0.f);
	ValueType scaleVal = glm::vec3(1.f);
	ModifyBoneType translation = {};
	ModifyBoneType rotation = {};
	ModifyBoneType scale = {};
	ValueType alpha = 0.f;
	StringName boneName;

private:
	bool has_init = false;
	int bone_index = -1;
};
class agCopyBone : public agBaseNode
{
public:
	CLASS_BODY(agCopyBone);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	void refresh_after_model_reload(Model* reloaded) final;
	agBaseNode* input = nullptr;
	StringName sourceBone;
	StringName targetBone;
	ValueType copyTranslation = false;
	ValueType copyRotation = false;
	ValueType copyScale = false;
	ValueType alpha = 1.f;
	bool copyBonespace = false;

private:
	bool has_init = false;
	int source_bone_idx = -1;
	int target_bone_idx = -1;
};

class agBlendspace : public agBlendNode
{
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

class agStatemachineBase : public agBaseNode
{
public:
	CLASS_BODY(agStatemachineBase, scriptable);
	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;

	REF virtual void update(agGetPoseCtx* ctx, bool wantsReset) {} // ABSTRACT CLASS

	// each update, use set_pose to set what state is active
	// ...and use set_transition before a pose change to set how it transitions
	REF void set_pose(agBaseNode* pose);
	REF void set_transition_parameters(Easing easing, float blend_time);

	// various getters to use in your transition logic
	REF bool is_transitioning() const { return blendingOut != nullptr; }
	REF float get_transition_time_left() const { return curTransitionDuration - curTransitionTime; }
	REF float get_transition_percent() const { return curTransitionTime / curTransitionDuration; }
	REF float get_state_duration() const { return curTime; }

	REF virtual void append_input(agBaseNode* tree) {
		for (int i = 0; i < trees.size(); i++)
			ASSERT(trees[i] != tree);

		trees.push_back(tree);
	}

private:
	std::vector<agBaseNode*> trees;

	float curTime = 0.0;
	agBaseNode* currentTree = nullptr;
	// cur transition
	Easing curTransition{};
	float curTransitionDuration = 0.0;
	float curTransitionTime = 0.0;
	Pose* blendingOut = nullptr;
};

struct DirectAnimationSlot;
class agSlotClipInternal : public agBaseNode
{
public:
	CLASS_BODY(agSlotClipInternal);

	void reset() final;
	void get_pose(agGetPoseCtx& ctx) final;
	DirectAnimationSlot* slot = nullptr;
};
// manual playback of animation in the graph
class agSlotPlayer : public agStatemachineBase
{
public:
	CLASS_BODY(agSlotPlayer);
	void update(agGetPoseCtx* ctx, bool wantsReset) final;
	bool updateChildrenWhenPlaying = false;
	StringName slotName;
	agBaseNode* input = nullptr;

private:
	agSlotClipInternal clipPlayer;
	Pose* fadingOutPose = nullptr;
};
class agBlendByInt : public agStatemachineBase
{
public:
	CLASS_BODY(agBlendByInt);
	void update(agGetPoseCtx* ctx, bool wantsReset) final;

	REF void set_transition_data(Easing easing, float duration) {
		this->easing = easing;
		this->blending_duration = duration;
	}
	void append_input(agBaseNode* node) final {
		agStatemachineBase::append_input(node);
		inputs.push_back(node);
	}
	REF void set_integer_var(string str) { integer = StringName(str.c_str()); }

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