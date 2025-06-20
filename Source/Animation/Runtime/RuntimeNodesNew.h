#pragma once
#include "Animation/AnimationSeqAsset.h"
#include "Framework/EnumDefReflection.h"
#include "SyncTime.h"
#include "Framework/StructReflection.h"
#include "Animation/Editor/Optional.h"

class AnimCurveData {
public:
};
class AnimEventBuffer {
public:
};

class AnimatorInstance;
class AnimTreeGraphContext
{
public:
	AnimatorInstance* instance = nullptr;
	SyncGroupData* find_sync_group(StringName name) const;
	const MSkeleton* get_skeleton() const;
	AnimEventBuffer* event_buffer = nullptr;
};

class Pose;
class AnimTreeUpdateStack
{
public:
	AnimTreeUpdateStack(AnimTreeGraphContext& graph) : graph(graph) {}
	AnimTreeGraphContext& graph;
	AnimTreeGraphContext& get_graph() { return graph; }
	vector<void*> playing_clip_nodes;
	AnimCurveData* curves = nullptr;
	Pose* pose = nullptr;
	float dt = 0.0;
	// if true, then clip will return that it is finished when current_time + auto_transition_time >= clip_time
	bool has_auto_transition = false;
	float automatic_transition_time = 0.0;
};

struct stClipNode {
	STRUCT_BODY(ClipNode_SData);
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
using glm::vec3;
using glm::vec2;

class AnimTreePoseNode : public ClassBase {
public:
	CLASS_BODY(AnimTreePoseNode);
	struct Inst {
		virtual ~Inst() {}
		virtual void get_pose(AnimTreeUpdateStack& context) {}
		virtual void reset() {}
	};
	virtual Inst* create_inst() { return nullptr; }
};

using PoseNodeInst = AnimTreePoseNode::Inst;

class GraphValueNode : public ClassBase {
public:
	virtual bool get_bool() const;
	virtual int get_int() const;
	virtual float get_float(int index) const;
	virtual vec2 get_vector2() const;
	virtual vec3 get_vector3() const;
	virtual StringName get_name() const;
};
class GraphValueConstant : public GraphValueNode {
public:
};
class GraphValueOperator : public GraphValueNode {
public:
};
class GraphValueVariable : public GraphValueNode {
public:
};
class GraphValueFunction : public GraphValueNode {
public:
};

struct BoneIndexRetargetMap;
class ClipNode : public AnimTreePoseNode {
public:
	CLASS_BODY(ClipNode);
	struct Inst : public PoseNodeInst {
		Inst(ClipNode& o) : owner(o) {}

		ClipNode& owner;
		const AnimationSeq* clip = nullptr;
		const BoneIndexRetargetMap* remap = nullptr;
		float anim_time = 0.0;

		void get_pose(AnimTreeUpdateStack& context) override;
		void reset() override;
		float get_clip_length() const;
		bool has_sync_group() const;
		float get_speed() const;
	};
	PoseNodeInst* create_inst() final { return new Inst(*this); }
	REF stClipNode data;
};


class Ik2Bone : public AnimTreePoseNode {
public:
	CLASS_BODY(Ik2Bone);
	struct Inst : public PoseNodeInst {
		PoseNodeInst* input = nullptr;
		GraphValueNode* target = nullptr;
		GraphValueNode* pole = nullptr;
		GraphValueNode* alpha = nullptr;

		void get_pose(AnimTreeUpdateStack& context) override {}
	};
	PoseNodeInst* create_inst() final { return new Inst(); }
};
class ModifyBone : public AnimTreePoseNode {
public:
	CLASS_BODY(ModifyBone);
	struct Inst : public PoseNodeInst {
		Inst(ModifyBone& owner) :owner(owner) {}
		ModifyBone& owner;
		PoseNodeInst* input = nullptr;
		GraphValueNode* translation = nullptr;
		GraphValueNode* rotation = nullptr;
		GraphValueNode* alpha = nullptr;

		void get_pose(AnimTreeUpdateStack& context) override {}
	};
	PoseNodeInst* create_inst() final { return new Inst(*this); }
	REF ModifyBoneType translation={};
	REF ModifyBoneType rotation={};
};

class BlendByInt : public AnimTreePoseNode {
public:
	CLASS_BODY(BlendByInt);
	struct Inst : public PoseNodeInst {
		Inst(BlendByInt& owner) :owner(owner) {}
		BlendByInt& owner;
		PoseNodeInst* default_input = nullptr;
		vector<PoseNodeInst*> inputs;
		opt<int> current_input;
		GraphValueNode* value = nullptr;
	};
};
struct AnimStatemachineTransition
{
	STRUCT_BODY(AnimStatemachineTransition);
	REF int transition_condition = -1;
	REF bool interruptable = true;
	REF float transition_time = 0.2;
	REF bool is_auto_transition = false;

};
struct AnimStatemachineState
{
	STRUCT_BODY(AnimStatemachineState);
	REF int graph_root_node = -1;
	REF vector<int> transitions;
};

class AnimStatemachine : public AnimTreePoseNode {
public:
	CLASS_BODY(AnimStatemachine);
	struct Inst : public PoseNodeInst {
		Inst(AnimStatemachine& owner) :owner(owner) {}
		AnimStatemachine& owner;
	};
	REF vector<AnimStatemachineTransition> transitions;
	REF vector<AnimStatemachineState> states;
	REF vector<int> entry_transitions;
};