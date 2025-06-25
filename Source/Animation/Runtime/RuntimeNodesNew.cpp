#include "RuntimeNodesNew.h"
#include "Animation/SkeletonData.h"
#include "Animation/AnimationUtil.h"
#include "RuntimeValueNodes.h"

struct OutGetClip
{
	float next_time = 0.0;
};

static OutGetClip get_clip_pose_shared_new(atUpdateStack& context, const AnimationSeq* clip,
	bool has_sync_group, StringName sync_group_name, sync_opt SyncOption, bool loop, const BoneIndexRetargetMap* remap,
	float speed, const float prev_anim_time, const PoseNodeInst* owner)
{
	const float EPSILON = 0.001f;
	assert(context.pose);
	assert(clip);

	const atGraphContext& graph = context.get_graph();
	const MSkeleton& skeleton = graph.skeleton;

	// synced update
	if (has_sync_group) {
		SyncGroupData& sync = graph.find_sync_group(sync_group_name);

		float next_anim_time = prev_anim_time;
		if (sync.is_this_first_update()) {
			// do nothing
		}
		else {
			next_anim_time = sync.time.get() * clip->duration;	// normalized time, TODO: sync markers
		}
		const float time_to_evaluate_sequence = next_anim_time;

		if (sync.should_write_new_update_weight(SyncOption, 0.5/*TODO*/)) {

			next_anim_time += context.dt * speed * 0.8f;	// HACK !!!!!!! fixme, should be 24 fps instead of 30 but setting it breaks stuff, just do this for now 

			if (next_anim_time > clip->duration || next_anim_time < 0.f) {
				if (loop)
					next_anim_time = fmod(fmod(next_anim_time, clip->duration) + clip->duration, clip->duration);
				else {
					next_anim_time = clip->duration - EPSILON;
				}
			}
			assert(0);
			sync.write_to_update_time(SyncOption, 0.5/*TODO*/, nullptr/*FIXME*/, Percentage(next_anim_time, clip->duration));
			util_calc_rotations(&skeleton, clip, time_to_evaluate_sequence, remap, *context.pose);
		}
		return { next_anim_time };
	}
	// unsynced update
	else {
		const float time_to_evaluate_sequence = prev_anim_time;
		
		float next_anim_time = prev_anim_time;
		next_anim_time += context.dt * speed * 0.8f;	// see above
		if (next_anim_time > clip->duration || next_anim_time < 0.f) {
			if (loop)
				next_anim_time = fmod(fmod(next_anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				next_anim_time = clip->duration - EPSILON;
			}
		}
		util_calc_rotations(&skeleton, clip, time_to_evaluate_sequence, remap, *context.pose);

		return { next_anim_time };
	}
}


void atClipNode::Inst::get_pose(atUpdateStack& context)
{
	if (!clip) {
		util_set_to_bind_pose(*context.pose, &context.graph.skeleton);
		return;
	}
	auto [next_anim_time] = get_clip_pose_shared_new(context, clip, has_sync_group(), owner.data.SyncGroup, owner.data.SyncOption, owner.data.loop, remap, get_speed(context), anim_time, this);
	anim_time = next_anim_time;

}
float atClipNode::Inst::get_clip_length() const {
	return clip->get_duration();
}
bool atClipNode::Inst::has_sync_group() const {
	return owner.data.has_sync_group();
}
float atClipNode::Inst::get_speed(atUpdateStack& context) const {
	return speed->get_float(context);
}
void atClipNode::Inst::reset() {
	anim_time = 0.0;
}

SyncGroupData& atGraphContext::find_sync_group(StringName name) const
{
	return instance.find_or_create_sync_group(name);
}
