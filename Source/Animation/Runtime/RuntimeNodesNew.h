#pragma once
#include "Animation/AnimationSeqAsset.h"
#include "Framework/EnumDefReflection.h"
#include "SyncTime.h"
#include "Framework/StructReflection.h"
#include "Animation/Editor/Optional.h"
#include "Framework/PropertyPtr.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "../AnimationTreePublic.h"
#include "Framework/MapUtil.h"
#include "Framework/PoolAllocator.h"
#include "RuntimeNodesBase.h"
#include <variant>
using std::variant;
using glm::vec3;
using glm::vec2;
class atValueNode;
class AnimatorInstance;
class atClipNode;
class Pose;
class atInitContext;
struct BoneIndexRetargetMap;
using std::pair;
class AnimatorObject;
class SampledAnimCurveBuffer {
public:
	void set_curve(StringName s, float f);
	float get_curve(StringName s);
	vector<pair<StringName, float>> vals;
};
class SampledAnimEventBuffer {
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

class atGraphContext
{
public:
	atGraphContext(AnimatorObject& obj, float dt) 
		: dt(dt),obj(obj), skeleton(*obj.get_skel())
	{

	}
	SyncGroupData& find_sync_group(StringName name) const;
//	void add_playing_clip(atClipNode::Inst* clip) { relevant_playing_clips.push_back(clip); }
	int get_num_bones() const { return skeleton.get_num_bones(); }

	float dt = 1.f;
	AnimatorObject& obj;
	SampledAnimEventBuffer events;
	SampledAnimCurveBuffer curves;
	const MSkeleton& skeleton;
	//vector<atClipNode::Inst*> relevant_playing_clips;
};
class atUpdateStack
{
public:
	atUpdateStack(atGraphContext& graph, Pool_Allocator<Pose>& allocator) 
		: graph(graph),pose(allocator.allocate_scoped()) {
	}
	atGraphContext& graph;
	ScopedPoolPtr<Pose> pose;
	RootMotionTransform rootMotion;
	float weight = 1.f;
	atUpdateStack(const atUpdateStack& other) 
		: graph(other.graph),pose(other.pose.get_parent().allocate_scoped()) {
	}
};

struct atClipNodeStruct {
	STRUCT_BODY();
	REF AssetPtr<AnimationSeqAsset> Clip;
	REF bool loop = true;
	REF StringName SyncGroup;
	REF sync_opt SyncOption = sync_opt::Default;
	bool has_sync_group() const {
		return !SyncGroup.is_null();
	}
	REF int start_frame = 0;
};

NEWENUM(ModifyBoneType, int)
{
	None,
	Meshspace,
	MeshspaceAdd,
	Localspace,
	LocalspaceAdd
};

class atCreateInstContext {
public:
	atCreateInstContext(const Animation_Tree_CFG& tree, AnimatorObject& obj) 
		: object(obj), tree(tree) {}
	PoseNodeInst* create_inst(int nodeid);
	atValueNode* find_value(int nodeid);
	ClassBase* find_node_in_tree_nodes(int nodeid) { return MapUtil::get_or_null(tree.get_nodes(), nodeid);  }
	AnimatorObject& object;
	const Animation_Tree_CFG& tree;
	vector<uptr<PoseNodeInst>> created_nodes;
};

class atInitContext {
public:
	atInitContext(const Animation_Tree_CFG& tree) : tree(tree) {} 
	atValueNode* find_value(int nodeid);
	ClassBase* find_node_in_tree_nodes(int nodeid) { return MapUtil::get_or_null(tree.get_nodes(), nodeid); }
	const ClassTypeInfo& get_instance_type() const { return tree.get_animator_class(); }
	const Animation_Tree_CFG& tree;
	AnimatorInstance* animatorInstance = nullptr;
};

class atClipNode : public AnimTreePoseNode {
public:
	CLASS_BODY(atClipNode);
	struct Inst : public PoseNodeInst {
		Inst(const atClipNode& o, atCreateInstContext& ctx) : owner(o) {
			this->speed = ctx.find_value(o.speedId);
			const AnimationSeqAsset& seq = *o.data.Clip;
			this->clip = seq.seq;
			MSkeleton* skel = (MSkeleton*)ctx.object.get_skel();
			this->remap = skel->get_remap(seq.srcModel->get_skel());
		}

		const atClipNode& owner;
		const AnimationSeq* clip = nullptr;
		const BoneIndexRetargetMap* remap = nullptr;
		float anim_time = 0.0;
		atValueNode* speed = nullptr;

		void get_pose(atUpdateStack& context) final;
		void reset() final;
		float get_clip_length() const;
		bool has_sync_group() const;
		float get_speed(atUpdateStack& ctx) const;
	};
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }
	REF atClipNodeStruct data;
	REF int speedId = 0;
};

// Both add and blend nodes
class atComposePoses : public AnimTreePoseNode {
public:
	CLASS_BODY(atComposePoses);
	struct Inst : public PoseNodeInst {
		Inst(const atComposePoses& owner, atCreateInstContext& ctx) : is_additive(owner.type==atComposePoses::Additive) {
			pose0 = ctx.create_inst(owner.pose0Id);
			pose1 = ctx.create_inst(owner.pose1Id);
			alpha = ctx.find_value(owner.alphaId);
		}
		void get_pose(atUpdateStack& context) final;
		void reset() final {
			pose0->reset();
			pose1->reset();
		}

		const bool is_additive = false;
		PoseNodeInst* pose0 = nullptr;
		PoseNodeInst* pose1 = nullptr;
		atValueNode* alpha = nullptr;
	};
	REF int pose0Id = 0;
	REF int pose1Id = 0;
	REF int alphaId = 0;
	
	enum Type {
		Blend,Additive
	};
	REF int8_t type = Blend;

	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this,ctx); }
};
class atSubtract : public AnimTreePoseNode {
public:
	CLASS_BODY(atSubtract);
	struct Inst : PoseNodeInst {
		Inst(int p0, int p1, atCreateInstContext& ctx) {
			base = ctx.create_inst(p0);
			sub = ctx.create_inst(p1);
		}
		void reset() final {
			base->reset();
			sub->reset();
		}
		void get_pose(atUpdateStack& context) final;

		PoseNodeInst* base = nullptr;
		PoseNodeInst* sub = nullptr;
	};
	REF int pose0Id = 0;
	REF int pose1Id = 0;
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(pose0Id,pose1Id,ctx); }
};

class atIk2Bone : public AnimTreePoseNode {
public:
	CLASS_BODY(atIk2Bone);
	struct Inst : public PoseNodeInst {
		Inst(const atIk2Bone& owner, atCreateInstContext& ctx) : owner(owner) {
			input = ctx.create_inst(owner.inputId);
			target = ctx.find_value(owner.targetId);
			pole = ctx.find_value(owner.poleId);
			alpha = ctx.find_value(owner.alphaId);
			bone_index = ctx.object.get_skel()->get_bone_index(owner.bone_name);
			if(owner.ik_in_bone_space)
				other_bone_index = ctx.object.get_skel()->get_bone_index(owner.other_bone);
		}
		const atIk2Bone& owner;
		PoseNodeInst* input = nullptr;
		atValueNode* target = nullptr;
		atValueNode* pole = nullptr;
		atValueNode* alpha = nullptr;
		int bone_index = -1;
		int other_bone_index = -1;
		void get_pose(atUpdateStack& context) final;
		void reset() {
			input->reset();
		}
	};
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }

	REF int inputId = 0;
	REF int targetId = 0;
	REF int poleId = 0;
	REF int alphaId = 0;
	REF StringName bone_name;
	REF StringName other_bone;
	REF bool take_rotation_of_other_bone = false;
	REF bool ik_in_bone_space = false;
};
class atModifyBone : public AnimTreePoseNode {
public:
	CLASS_BODY(atModifyBone);
	struct Inst : public PoseNodeInst {
		Inst(const atModifyBone& owner, atCreateInstContext& ctx) :owner(owner) {
			input = ctx.create_inst(owner.inputId);
			translation = ctx.find_value(owner.translationId);
			rotation = ctx.find_value(owner.rotationId);
			alpha = ctx.find_value(owner.alphaId);
			bone_index = ctx.object.get_skel()->get_bone_index(owner.boneName);
		}
		const atModifyBone& owner;
		PoseNodeInst* input = nullptr;
		atValueNode* translation = nullptr;
		atValueNode* rotation = nullptr;
		atValueNode* alpha = nullptr;
		int bone_index = -1;

		void get_pose(atUpdateStack& context) final;
		void reset() final {
			input->reset();
		}
	};
	REF ModifyBoneType translation={};
	REF ModifyBoneType rotation={};
	REF int inputId = 0;
	REF int translationId = 0;
	REF int rotationId = 0;
	REF int alphaId = 0;
	REF StringName boneName;

	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this,ctx); }
};
class atCopyBone : public AnimTreePoseNode {
public:
	CLASS_BODY(atCopyBone);
	struct Inst : public PoseNodeInst {
		Inst(const atCopyBone& owner, atCreateInstContext& ctx) : owner(owner) {

		}
		const atCopyBone& owner;
		PoseNodeInst* input = nullptr;
		atValueNode* copyTranslation = nullptr;
		atValueNode* copyRotation = nullptr;
		int source_bone = -1;
		int target_bone = -1;
		void reset() final {
			input->reset();
		}
		void get_pose(atUpdateStack& context) final;
	};
	REF int inputId = 0;
	REF StringName sourceBone;
	REF StringName targetBone;
	REF int copyTranslationId = 0;
	REF int copyRotationId = 0;
	REF bool copyBonespace = false;
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }
};


class atBlendByInt : public AnimTreePoseNode {
public:
	CLASS_BODY(atBlendByInt);
	struct Inst : public PoseNodeInst {
		Inst(const atBlendByInt& owner, atCreateInstContext& ctx) {
			value = ctx.find_value(owner.valudId);
			for (int i : owner.inputs)
				this->inputs.push_back(ctx.create_inst(i));
		}
		void get_pose(atUpdateStack& context) final {}
		void reset() final {
			current_input = std::nullopt;
			blending_duration = 0.0;
		}

		vector<PoseNodeInst*> inputs;
		opt<int> current_input;
		atValueNode* value = nullptr;
		Pose* fading_out_pose = nullptr;
		float blending_duration = 0.0;
	};
	REF vector<int> inputs;
	REF int valudId = 0;
	REF float blendTime = 0.2;
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }
};
struct atSmTransition {
	STRUCT_BODY();
	REF int transition_condition = 0;
	REF float transition_time = 0.2;
	REF int transition_to = 0;
	REF bool interruptable = true;
	REF bool is_auto_transition = false;
	int8_t temp_priority = 0;// used for sorting
};
struct atSmState {
	STRUCT_BODY();
	REF int graph_root_node = 0;
	REF vector<int> transitions;
};

struct atBlendOptionsShared {
	~atBlendOptionsShared() {
		//g_pose_pool.free(transition_pose);
	}

	void reset() {
		active = std::nullopt;
	}
	
	bool is_transitioning() const { return transition_pose != nullptr; }
private:
	opt<int> active;
	Pose* transition_pose=nullptr;
};

class atAnimStatemachine : public AnimTreePoseNode {
public:
	CLASS_BODY(atAnimStatemachine);
	struct Inst : public PoseNodeInst {
		Inst(const atAnimStatemachine& owner, atCreateInstContext& ctx) :owner(owner) {
			for (auto& t : owner.transitions)
				transition_conds.push_back(ctx.find_value(t.transition_condition));
			for (auto& s : owner.states)
				state_pose_insts.push_back(ctx.create_inst(s.graph_root_node));
		}
		void get_pose(atUpdateStack& context) final;
		void reset() final {
			active_state = std::nullopt;
			fading_out_state = std::nullopt;
			active_transition = std::nullopt;
		}
		const atAnimStatemachine& owner;
		opt<int> active_state;
		opt<int> fading_out_state;
		opt<int> active_transition;
		Pose* cached_pose_from_transition = nullptr;
		float blend_duration = 0.0;
		float blend_percentage = 0.0;

		vector<PoseNodeInst*> state_pose_insts;
		vector<atValueNode*> transition_conds;
	};
	REF vector<atSmTransition> transitions;
	REF vector<atSmState> states;
	REF vector<int> entry_transitions;
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }
};
class atSlotPlay : public AnimTreePoseNode {
public:
	CLASS_BODY(atSlotPlay);
	struct Inst : public PoseNodeInst {
		Pose* fadingOutPose = nullptr;
		Inst(const atSlotPlay& owner, atCreateInstContext& ctx) : owner(owner) {
			input = ctx.create_inst(owner.inputId);
		}
		const atSlotPlay& owner;
		PoseNodeInst* input = nullptr;
		void reset() final {
			input->reset();
		}
		void get_pose(atUpdateStack& ctx) final;
	};
	REF int inputId = 0;
	REF bool updateChildrenWhenPlaying = false;
	REF StringName slotName;
	PoseNodeInst* create_inst(atCreateInstContext& ctx) const final { return new Inst(*this, ctx); }
};