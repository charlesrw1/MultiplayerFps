#pragma once
#include "Animation/AnimationSeqAsset.h"
#include "Framework/EnumDefReflection.h"
#include "SyncTime.h"
#include "Framework/StructReflection.h"
#include "Animation/Editor/Optional.h"
#include "Framework/PropertyPtr.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
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

class AnimCurveData {
public:
};
class AnimEventBuffer {
public:
};

class atGraphContext
{
public:
	atGraphContext(AnimatorInstance& inst, AnimEventBuffer& ev, AnimCurveData& cur, const MSkeleton& skel)
		: instance(inst), events(ev), curves(cur), skeleton(skel)
	{

	}
	SyncGroupData& find_sync_group(StringName name) const;

	AnimatorInstance& instance;
	AnimEventBuffer& events;
	AnimCurveData& curves;
	const MSkeleton& skeleton;
};
class atUpdateStack
{
public:
	atUpdateStack(atGraphContext& graph) : graph(graph) {}
	atGraphContext& graph;
	atGraphContext& get_graph() { return graph; }
	vector<variant<atClipNode*>> playing_clip_nodes;
	Pose* pose = nullptr;
	float dt = 0.0;
	// if true, then clip will return that it is finished when current_time + auto_transition_time >= clip_time
	bool has_auto_transition = false;
	float automatic_transition_time = 0.0;
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

class AnimTreePoseNode : public ClassBase {
public:
	CLASS_BODY(AnimTreePoseNode);
	struct Inst {
		virtual ~Inst() {}
		virtual void get_pose(atUpdateStack& context) {}
		virtual void reset() {}
	};
	virtual Inst* create_inst(atInitContext& ctx) const = 0;
};
using PoseNodeInst = AnimTreePoseNode::Inst;

class atInitContext {
public:
	template<typename T>
	T* find_node(int nodeid) {
		return nullptr;
	}

	PoseNodeInst* create_inst(int nodeid) {
		return nullptr;
	}
	atValueNode* find_value(int nodeidx) {
		return nullptr;
	}

	AnimatorInstance* animatorInstance = nullptr;
	const ClassTypeInfo* whatType = nullptr;
};

class atClipNode : public AnimTreePoseNode {
public:
	CLASS_BODY(atClipNode);
	struct Inst : public PoseNodeInst {
		Inst(const atClipNode& o, atInitContext& ctx) : owner(o) {
			this->speed = ctx.find_value(o.speedId);
			const AnimationSeqAsset& seq = *o.data.Clip;
			this->clip = seq.seq;
			MSkeleton* skel = (MSkeleton*)ctx.animatorInstance->get_skel();
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
	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(*this, ctx); }
	REF atClipNodeStruct data;
	REF int speedId = 0;
};

// Both add and blend nodes
class atComposePoses : public AnimTreePoseNode {
public:
	CLASS_BODY(atComposePoses);
	struct Inst : public PoseNodeInst {
		Inst(const atComposePoses& owner,atInitContext& ctx) {
			pose0 = ctx.create_inst(owner.pose0Id);
			pose1 = ctx.create_inst(owner.pose1Id);
			alpha = ctx.find_value(owner.alphaId);
		}
		void get_pose(atUpdateStack& context) final {}

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

	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(*this,ctx); }
};

class atIk2Bone : public AnimTreePoseNode {
public:
	CLASS_BODY(atIk2Bone);
	struct Inst : public PoseNodeInst {
		PoseNodeInst* input = nullptr;
		atValueNode* target = nullptr;
		atValueNode* pole = nullptr;
		atValueNode* alpha = nullptr;

		void get_pose(atUpdateStack& context) final {}
	};
	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(); }
	REF int inputId = 0;
	REF int targetId = 0;
	REF int poleId = 0;
	REF int alphaId = 0;
};
class atModifyBone : public AnimTreePoseNode {
public:
	CLASS_BODY(atModifyBone);
	struct Inst : public PoseNodeInst {
		Inst(const atModifyBone& owner, atInitContext& ctx) :owner(owner) {
			input = ctx.create_inst(owner.inputId);
			translation = ctx.find_value(owner.translationId);
			rotation = ctx.find_value(owner.rotationId);
			alpha = ctx.find_value(owner.alphaId);
		}
		const atModifyBone& owner;
		PoseNodeInst* input = nullptr;
		atValueNode* translation = nullptr;
		atValueNode* rotation = nullptr;
		atValueNode* alpha = nullptr;

		void get_pose(atUpdateStack& context) final {}
	};
	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(*this,ctx); }
	REF ModifyBoneType translation={};
	REF ModifyBoneType rotation={};
	REF int inputId = 0;
	REF int translationId = 0;
	REF int rotationId = 0;
	REF int alphaId = 0;
};

class atBlendByInt : public AnimTreePoseNode {
public:
	CLASS_BODY(atBlendByInt);
	struct Inst : public PoseNodeInst {
		Inst(const atBlendByInt& owner, atInitContext& ctx) {
			value = ctx.find_value(owner.valudId);
			for (int i : owner.inputs)
				this->inputs.push_back(ctx.create_inst(i));
		}
		void get_pose(atUpdateStack& context) final {}
		void reset() final {}

		vector<PoseNodeInst*> inputs;
		opt<int> current_input;
		atValueNode* value = nullptr;
	};
	REF vector<int> inputs;
	REF int valudId = 0;
	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(*this, ctx); }
};
struct atSmTransition {
	STRUCT_BODY();
	REF int transition_condition = 0;
	REF bool interruptable = true;
	REF float transition_time = 0.2;
	REF bool is_auto_transition = false;

};
struct atSmState {
	STRUCT_BODY();
	REF int graph_root_node = 0;
	REF vector<int> transitions;
};

class atAnimStatemachine : public AnimTreePoseNode {
public:
	CLASS_BODY(atAnimStatemachine);
	struct Inst : public PoseNodeInst {
		Inst(const atAnimStatemachine& owner, atInitContext& ctx) :owner(owner) {
			for (auto& t : owner.transitions)
				transition_conds.push_back(ctx.find_value(t.transition_condition));
			for (auto& s : owner.states)
				state_pose_insts.push_back(ctx.create_inst(s.graph_root_node));
		}
		void get_pose(atUpdateStack& context) final {}
		void reset() final {}

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

	PoseNodeInst* create_inst(atInitContext& ctx) const final { return new Inst(*this, ctx); }
};