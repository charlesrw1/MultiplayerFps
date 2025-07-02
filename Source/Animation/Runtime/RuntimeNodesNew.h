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
