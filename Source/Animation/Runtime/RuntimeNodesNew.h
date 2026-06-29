#pragma once
#include "Animation/AnimationSeqAsset.h"
#include "Framework/EnumDefReflection.h"
#include "SyncTime.h"
#include "Framework/StructReflection.h"
#include "Framework/Optional.h"
#include "Framework/PropertyPtr.h"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"

#include "Framework/MapUtil.h"
#include "Framework/PoolAllocator.h"
#include <variant>
using glm::vec2;
using glm::vec3;
using std::variant;
class atValueNode;
class AnimatorInstance;
class atClipNode;
class Pose;
class atInitContext;
struct BoneIndexRetargetMap;
using std::pair;
class AnimatorObject;
class SampledAnimCurveBuffer
{
public:
	void set_curve(StringName s, float f);
	float get_curve(StringName s);
	vector<pair<StringName, float>> vals;
};

class atUpdateStack;
class atInitContext;
class atCreateInstContext;
class AnimTreePoseNode : public ClassBase
{
public:
	CLASS_BODY(AnimTreePoseNode);
	struct Inst
	{
		virtual ~Inst() {}
		virtual void get_pose(atUpdateStack& context) {}
		virtual void reset() {}
	};
	virtual Inst* create_inst(atCreateInstContext& ctx) const = 0;
};
using PoseNodeInst = AnimTreePoseNode::Inst;


class atGraphContext
{
public:
	atGraphContext(AnimatorObject& obj, float dt) : dt(dt), obj(obj), skeleton(*obj.get_skel()) {}
	SyncGroupData& find_sync_group(StringName name) const;
	//	void add_playing_clip(atClipNode::Inst* clip) { relevant_playing_clips.push_back(clip); }
	int get_num_bones() const { return skeleton.get_num_bones(); }

	float dt = 1.f;
	AnimatorObject& obj;
	SampledAnimCurveBuffer curves;
	const MSkeleton& skeleton;
	// vector<atClipNode::Inst*> relevant_playing_clips;
};
class atUpdateStack
{
public:
	atUpdateStack(atGraphContext& graph, Pool_Allocator<Pose>& allocator)
		: graph(graph), pose(allocator.allocate_scoped()) {}
	atGraphContext& graph;
	ScopedPoolPtr<Pose> pose;
	RootMotionTransform rootMotion;
	float weight = 1.f;
	atUpdateStack(const atUpdateStack& other) : graph(other.graph), pose(other.pose.get_parent().allocate_scoped()) {}
};

struct atClipNodeStruct
{
	STRUCT_BODY();
	REF AssetPtr<AnimationSeqAsset> Clip;
	REF bool loop = true;
	REF StringName SyncGroup;
	REF sync_opt SyncOption = sync_opt::Default;
	bool has_sync_group() const { return !SyncGroup.is_null(); }
	REF int start_frame = 0;
};

NEWENUM(ModifyBoneType, int){None, Meshspace, MeshspaceAdd, Localspace, LocalspaceAdd};
