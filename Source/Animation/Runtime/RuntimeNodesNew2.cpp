#include "RuntimeNodesNew2.h"
#include "Animation/AnimationUtil.h"
#include "AnimationTreeLocal.h"
#include "Render/Model.h"
#include "Framework/Util.h"
#include "Animation.h"
#include <algorithm>
#include <limits>

void agClipNode::reset() {
	anim_time = 0.0;
}

void agClipNode::refresh_after_model_reload(Model* reloaded) {
	// remap was cached against ctx.get_skeleton()'s remaps[] vector, which is
	// wiped by MSkeleton::uninstall on any Model reload (the animator's model
	// or clipFrom).  Always invalidate; next get_pose regenerates.
	remap = nullptr;
	has_init = false;
	if (clipFrom == reloaded) {
		// `seq` lived in clipFrom->get_skel()->clips, now wiped. Re-find it by
		// name against the reloaded skeleton instead of requiring the caller
		// to re-issue set_clip.
		seq = reloaded && reloaded->get_skel() && !clip_name.empty() ? reloaded->get_skel()->find_clip(clip_name)
																	   : nullptr;
		if (!seq) {
			sys_print(Error, "agClipNode: clipFrom '%s' reloaded; clip '%s' no longer exists. set_clip must be called again.\n",
					  reloaded ? reloaded->get_name().c_str() : "<null>", clip_name.c_str());
			clipFrom = nullptr;
		}
	}
}

void agEvaluateClip::refresh_after_model_reload(Model* reloaded) {
	remap = nullptr;
	has_init = false;
	if (clipFrom == reloaded) {
		seq = reloaded && reloaded->get_skel() && !clip_name.empty() ? reloaded->get_skel()->find_clip(clip_name)
																	   : nullptr;
		if (!seq) {
			sys_print(Error, "agEvaluateClip: clipFrom '%s' reloaded; clip '%s' no longer exists. set_clip must be called again.\n",
					  reloaded ? reloaded->get_name().c_str() : "<null>", clip_name.c_str());
			clipFrom = nullptr;
		}
	}
}

void agIk2Bone::refresh_after_model_reload(Model* reloaded) {
	// bone_idx / other_bone_idx were resolved against the animator's skel; the
	// skel address is stable but bone names may have moved.  Force re-resolve.
	has_init = false;
	bone_idx = -1;
	other_bone_idx = -1;
	pole_bone_idx = -1;
}
void agModifyBone::refresh_after_model_reload(Model* reloaded) {
	has_init = false;
	bone_index = -1;
}
void agCopyBone::refresh_after_model_reload(Model* reloaded) {
	has_init = false;
	source_bone_idx = -1;
	target_bone_idx = -1;
}

// Sample all AnimEvent crossings that occurred in [prev_time, curr_time).
// When the clip looped this frame pass looped=true; the window wraps as
// [prev_time, duration) ++ [0, curr_time).
static void sample_events_from_clip(agGetPoseCtx& ctx, const AnimationSeq* seq,
                                    float prev_time, float curr_time,
                                    bool looped, bool b_mirrored)
{
    if (!seq || seq->anim_events.empty()) return;
    auto& out    = ctx.object.sampled_events;
    const float w = ctx.weight;

    auto check_window = [&](const AnimEvent& ev, float p, float c) {
        if (!ev.is_duration) {
            if (ev.time_start >= p && ev.time_start < c)
                out.push_back({&ev, ev.time_start, w, b_mirrored, AnimEventTrigger::Entered});
            return;
        }
        const bool entered = ev.time_start >= p && ev.time_start < c;
        const bool left    = ev.time_end > ev.time_start && ev.time_end >= p && ev.time_end < c;
        if (entered)
            out.push_back({&ev, ev.time_start, w, b_mirrored, AnimEventTrigger::Entered});
        if (left)
            out.push_back({&ev, ev.time_end, w, b_mirrored, AnimEventTrigger::Left});
        // Still inside the duration window and didn't just cross a boundary this frame.
        if (!entered && !left && c > ev.time_start && c < ev.time_end)
            out.push_back({&ev, c, w, b_mirrored, AnimEventTrigger::Active});
    };

    if (!looped) {
        for (auto& ev : seq->anim_events)
            check_window(ev, prev_time, curr_time);
    } else {
        const float dur = seq->duration;
        for (auto& ev : seq->anim_events) {
            check_window(ev, prev_time, dur);
            check_window(ev, 0.f,       curr_time);
        }
    }
}

// One currently-blended sample inside a blend space: its clip, the retarget map to evaluate
// it with, and its normalized contribution weight (weights across all actives sum to 1).
struct ActiveBlendSample
{
	const AnimationSeq* seq = nullptr;
	const BoneIndexRetargetMap* remap = nullptr;
	float weight = 0.f;
};

// Advances a blend space's own normalized [0,1) playhead. Participates in an optional
// SyncGroup exactly like a virtual clip of duration 1 -- SyncGroupData::time is already a
// plain ratio, so a blend space's normalized time and a clip's normalized time interoperate
// with no rescaling. Unlike get_clip_pose_shared, the pose is evaluated every frame
// regardless of leader/follower election; only whether *this* node advances/writes the
// group's time is gated on winning the election. Returns the normalized time to evaluate
// samples at this frame; `animTime` is updated to hold next frame's starting time.
static float advance_blendspace_time(agGetPoseCtx& ctx, StringName syncGroup, sync_opt syncType, bool loop,
									 float speed, float avg_duration, float& animTime, bool& stopped) {
	const float rate = speed / glm::max(avg_duration, 0.0001f);
	float eval_time = animTime;
	auto step = [&](float from) {
		float next = from + ctx.dt * rate;
		if (next > 1.f || next < 0.f) {
			if (loop)
				next = fmod(fmod(next, 1.f) + 1.f, 1.f);
			else {
				next = 0.999f;
				stopped = true;
			}
		}
		return next;
	};
	if (!syncGroup.is_null()) {
		SyncGroupData& sync = ctx.find_sync_group(syncGroup);
		if (!sync.is_this_first_update())
			eval_time = sync.time.get();
		if (sync.should_write_new_update_weight(syncType, ctx.weight)) {
			const float next = step(eval_time);
			sync.write_to_update_time(syncType, ctx.weight, nullptr, Percentage(next, 1.f));
			animTime = next;
		} else {
			animTime = eval_time; // another node in the group owns advancing this frame
		}
	} else {
		animTime = step(eval_time);
	}
	return eval_time;
}

// Shared blend-space evaluation: normalizes weights, advances the shared playhead, samples
// each active clip at that playhead scaled by its own duration, and combines them into
// ctx.pose via successive weighted lerps (a running convex combination -- correct regardless
// of how many samples are active, so agBlendSpace1D's pairs and agBlendSpace2D's triangle
// corners share this same code).
static void blend_space_evaluate(agGetPoseCtx& ctx, std::vector<ActiveBlendSample>& actives, StringName syncGroup,
								 sync_opt syncType, bool loop, float speed, float& animTime) {
	actives.erase(std::remove_if(actives.begin(), actives.end(), [](const ActiveBlendSample& a) { return a.seq == nullptr; }),
				 actives.end());
	if (actives.empty()) {
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}
	float weight_sum = 0.f;
	for (auto& a : actives) weight_sum += a.weight;
	if (weight_sum > 0.00001f)
		for (auto& a : actives) a.weight /= weight_sum;
	else
		for (auto& a : actives) a.weight = 1.f / actives.size();

	float avg_duration = 0.f;
	for (auto& a : actives) avg_duration += a.weight * a.seq->get_duration();

	const float prev_time = animTime;
	bool stopped = false;
	const float eval_time = advance_blendspace_time(ctx, syncGroup, syncType, loop, speed, avg_duration, animTime, stopped);
	const bool looped = loop && (animTime < prev_time);

	size_t dominant = 0;
	float running_weight = 0.f;
	for (size_t i = 0; i < actives.size(); i++) {
		ActiveBlendSample& a = actives[i];
		const float dur = a.seq->get_duration();
		const float local_time = glm::clamp(eval_time * dur, 0.f, dur);

		if (i == 0) {
			util_calc_rotations(&ctx.get_skeleton(), a.seq, local_time, a.remap, *ctx.pose, loop);
			running_weight = a.weight;
		} else {
			agGetPoseCtx other(ctx);
			util_calc_rotations(&ctx.get_skeleton(), a.seq, local_time, a.remap, *other.pose, loop);
			const float total = running_weight + a.weight;
			const float lerp_alpha = total > 0.00001f ? a.weight / total : 0.f;
			util_blend(ctx.get_num_bones(), *other.pose, *ctx.pose, lerp_alpha);
			running_weight = total;
			if (a.weight > actives[dominant].weight)
				dominant = i;
		}
	}

	// Matches Unreal's default: notifies/events only fire from the single highest-weighted
	// sample in the blend, not scaled-and-fired from every active sample (which would double
	// up footstep/etc events during a transition between two clips).
	const ActiveBlendSample& dom = actives[dominant];
	const float dom_dur = dom.seq->get_duration();
	const float dom_local = glm::clamp(eval_time * dom_dur, 0.f, dom_dur);
	const float dom_prev_local = glm::clamp(prev_time * dom_dur, 0.f, dom_dur);
	sample_events_from_clip(ctx, dom.seq, dom_prev_local, dom_local, looped, false);
}

static void get_clip_pose_shared(agGetPoseCtx& ctx, const AnimationSeq* clip, bool has_sync_group,
								 StringName syncgroupname, sync_opt SyncOption, bool loop,
								 const BoneIndexRetargetMap* remap, float speed, float& anim_time, bool& stopped_flag,
								 const Node_CFG* owner) {
	const float speed_modify = 1.0f;
	// synced update
	if (has_sync_group) {
		SyncGroupData& sync = ctx.find_sync_group(syncgroupname);
		if (sync.is_this_first_update()) {
			// do nothing
		} else {
			anim_time = sync.time.get() * clip->duration; // normalized time, TODO: sync markers
		}
		const float time_to_evaluate_sequence = anim_time;
		if (sync.should_write_new_update_weight(SyncOption, 0.5 /*TODO*/)) {
			anim_time += ctx.dt * speed * speed_modify; // fixme
			if (anim_time > clip->duration || anim_time < 0.f) {
				if (loop)
					anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
				else {
					anim_time = clip->duration - 0.001f;
					stopped_flag = true;
				}
			}
			sync.write_to_update_time(SyncOption, 0.5 /*TODO*/, owner, Percentage(anim_time, clip->duration));
			util_calc_rotations(&ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *ctx.pose, loop);
		}
	}
	// unsynced update
	else {
		const float time_to_evaluate_sequence = anim_time;
		anim_time += ctx.dt * speed * speed_modify; // see above
		if (anim_time > clip->duration || anim_time < 0.f) {
			if (loop)
				anim_time = fmod(fmod(anim_time, clip->duration) + clip->duration, clip->duration);
			else {
				anim_time = clip->duration - 0.001f;
				stopped_flag = true;
			}
		}
		util_calc_rotations(&ctx.get_skeleton(), clip, time_to_evaluate_sequence, remap, *ctx.pose, loop);
	}
}

void agClipNode::get_pose(agGetPoseCtx& ctx) {
	if (!has_init) {
		if (!seq || !clipFrom) {
			sys_print(Error, "agClipNode::get_pose: no sequence set\n");
			has_init = true;
			util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton()); // pool buffer is garbage; reset
			return;
		}
		if (!clipFrom->get_skel()) {
			sys_print(Error, "agClipNode::get_pose: clip model has no skeleton\n");
			has_init = true;
			util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
			return;
		}
		remap = ctx.get_skeleton().get_remap(clipFrom->get_skel());
		has_init = true;
	}
	if (!seq) { // was set but later cleared (e.g. hot-reload)
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}

	const float playSpeed = speed.get_float(ctx);
	const float prev_time = anim_time;

	bool stopped_flag = false;
	get_clip_pose_shared(ctx, seq, !syncGroup.is_null(), syncGroup, syncType, looping, remap, playSpeed, anim_time,
						 stopped_flag, nullptr);

	const bool looped = looping && (anim_time < prev_time);
	sample_events_from_clip(ctx, seq, prev_time, anim_time, looped, false);

	ctx.add_playing_clip(this);

	ctx.debug_enter("agClip: " + std::to_string(anim_time) + "/" + std::to_string(seq->get_duration()));
	ctx.debug_exit();
}

void agEvaluateClip::get_pose(agGetPoseCtx& ctx) {
	if (!has_init) {
		if (!seq || !clipFrom) {
			sys_print(Error, "agEvaluateClip::get_pose: no sequence set\n");
			has_init = true;
			util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton()); // pool buffer is garbage; reset
			return;
		}
		if (!clipFrom->get_skel()) {
			sys_print(Error, "agEvaluateClip::get_pose: clip model has no skeleton\n");
			has_init = true;
			util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
			return;
		}
		remap = ctx.get_skeleton().get_remap(clipFrom->get_skel());
		has_init = true;
	}
	if (!seq) { // was set but later cleared (e.g. hot-reload)
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}

	bool stopped_flag = false;
	float time_to_play = seq->get_time_of_keyframe(frame);
	time_to_play = glm::clamp(time_to_play, 0.f, seq->duration);
	get_clip_pose_shared(ctx, seq, false, "", {}, false, remap, 1.0, time_to_play, stopped_flag, nullptr);
}
void agEvaluateClip::set_clip(const AnimationSeqAsset* asset) {
	seq = asset->seq;
	clipFrom = asset->srcModel.get();
	clip_name = asset->get_clip_name();
}

void agBindPose::get_pose(agGetPoseCtx& ctx) {
	util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
}

void agClipNode::set_clip(const Model* m, string clipName) {
	assert(m->get_skel());
	seq = m->get_skel()->find_clip(clipName);
	clipFrom = m;
	clip_name = std::move(clipName);
}

void agClipNode::set_clip(const AnimationSeqAsset* asset) {
	seq = asset->seq;
	clipFrom = asset->srcModel.get();
	clip_name = asset->get_clip_name();
}

void agBlendNode::reset() {
	input0->reset();
	input1->reset();
}

void agBlendNode::get_pose(agGetPoseCtx& ctx) {
	const float alphaVal = alpha.get_float(ctx);
	ctx.debug_enter("agBlend: " + std::to_string(alphaVal));

	if (alphaVal <= 0.00001f) {
		// input1 contributes nothing; input0 keeps full parent weight
		input0->get_pose(ctx);
	} else if (alphaVal >= 0.99999f) {
		// input0 contributes nothing; input1 keeps full parent weight
		input1->get_pose(ctx);
	} else {
		const float parent_weight = ctx.weight;
		agGetPoseCtx other(ctx);
		ctx.weight   = parent_weight * (1.f - alphaVal); // input0 share
		other.weight = parent_weight * alphaVal;          // input1 share
		input0->get_pose(ctx);
		input1->get_pose(other);
		ctx.weight = parent_weight; // restore for any parent accumulation
		util_blend(ctx.get_num_bones(), *other.pose, *ctx.pose, alphaVal);
	}
	ctx.debug_exit();
}

void agAddNode::reset() {
	input0->reset();
	input1->reset();
}

void agAddNode::get_pose(agGetPoseCtx& ctx) {
	const float alphaVal = alpha.get_float(ctx);
	ctx.debug_enter("agAddNode: " + std::to_string(alphaVal));
	if (alphaVal <= 0.00001f) {
		input0->get_pose(ctx);
	} else {
		agGetPoseCtx other(ctx);
		// Additive layer events scale with alpha; base events keep full parent weight.
		other.weight = ctx.weight * alphaVal;
		input0->get_pose(ctx);
		input1->get_pose(other);
		util_add(ctx.get_num_bones(), *other.pose, *ctx.pose, alphaVal);
	}
	ctx.debug_exit();
}

void agMakeAdditive::reset() {
	if (input)     input->reset();
	if (reference) reference->reset();
}

void agMakeAdditive::get_pose(agGetPoseCtx& ctx) {
	if (!input || !reference) {
		sys_print(Error, "agMakeAdditive::get_pose: missing input or reference\n");
		// This node outputs an additive delta; the neutral value is the identity delta
		// (zero translation/scale, identity rotation) so a downstream agAddNode is a no-op.
		// The pool buffer is garbage, so write it explicitly rather than leaving UB.
		const int nb = ctx.get_num_bones();
		Pose& p = *ctx.pose;
		for (int i = 0; i < nb; i++) {
			p.q[i]     = glm::quat(1.f, 0.f, 0.f, 0.f);
			p.pos[i]   = glm::vec3(0.f);
			p.scale[i] = 0.f;
		}
		return;
	}
	ctx.debug_enter("agMakeAdditive");

	// Motion clip into ctx.pose; reference (e.g. first frame) into a sibling pose.
	input->get_pose(ctx);
	agGetPoseCtx refCtx(ctx);
	reference->get_pose(refCtx);

	const int nb = ctx.get_num_bones();
	Pose& delta = *ctx.pose;
	util_subtract(nb, *refCtx.pose, delta); // delta = motion - reference

	// Zero the delta on masked bones so a later agAddNode leaves them untouched
	// (identity rotation, zero translation, zero scale-delta == no change in util_add).
	for (int i = 0; i < nb && i < (int)masked.size(); i++) {
		if (masked[i]) {
			delta.q[i]     = glm::quat(1.f, 0.f, 0.f, 0.f);
			delta.pos[i]   = glm::vec3(0.f);
			delta.scale[i] = 0.f;
		}
	}
	ctx.debug_exit();
}

void agMakeAdditive::init_mask(const Model* model) {
	assert(model && model->get_skel());
	masked.assign(model->get_skel()->get_num_bones(), 0);
}

void agMakeAdditive::mask_bone_and_children(const Model* model, string bone) {
	assert(model && model->get_skel());
	if (masked.empty())
		init_mask(model);
	const int myIndex = model->bone_for_name(StringName(bone.c_str()));
	if (myIndex == -1)
		throw std::runtime_error("agMakeAdditive::mask_bone_and_children: invalid bone");
	const int num_bones = model->get_skel()->get_num_bones();
	masked.at(myIndex) = 1;
	// Same contiguous-children walk as agBlendMasked::set_all_children_weights:
	// bones are stored parent-before-child, so a child has index > parent.
	for (int i = myIndex + 1; i < num_bones; i++) {
		const int parent = model->get_skel()->get_bone_parent(i);
		if (parent < myIndex)
			break;
		masked.at(i) = 1;
	}
}

SyncGroupData& agGetPoseCtx::find_sync_group(StringName name) const {
	// TODO: insert return statement here
	return object.find_or_create_sync_group(name);
}

float ValueType::get_float(agGetPoseCtx& ctx) {
	if (std::holds_alternative<float>(value))
		return std::get<float>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_float_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_float: doesn't hold float");
}

int ValueType::get_int(agGetPoseCtx& ctx) {
	if (std::holds_alternative<int>(value))
		return std::get<int>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_int_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_int: doesn't hold int");
}

int ValueType::get_bool(agGetPoseCtx& ctx) {
	if (std::holds_alternative<bool>(value))
		return std::get<bool>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_bool_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_bool: doesn't hold bool");
}

glm::vec3 ValueType::get_vec3(agGetPoseCtx& ctx) {
	if (std::holds_alternative<glm::vec3>(value))
		return std::get<glm::vec3>(value);
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_vec3_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_vec3: doesn't hold vec3");
}

glm::quat ValueType::get_quat(agGetPoseCtx& ctx) {
	if (std::holds_alternative<glm::quat>(value))
		return std::get<glm::quat>(value);
	else if (std::holds_alternative<glm::vec3>(value))
		return glm::quat(std::get<glm::vec3>(value)); // inline euler radians
	else if (std::holds_alternative<StringName>(value))
		return ctx.get_quat_var(std::get<StringName>(value));
	throw std::runtime_error("ValueType::get_quat: doesn't hold a rotation");
}

void agIk2Bone::reset() {
	input->reset();
}

static glm::mat4 build_global_transform_for_bone_index(Pose* pose, const MSkeleton* skel, int index) {
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];

	int count = 0;
	while (index != -1) {
		assert(count < 36);
		glm::mat4x4 matrix = glm::mat4_cast(pose->q[index]);
		matrix[3] = glm::vec4(pose->pos[index], 1.0);
		mats[count++] = matrix;
		index = skel->get_bone_parent(index);
	}
	for (int i = count - 2; i >= 0; i--) {
		mats[i] = mats[i + 1] * mats[i];
	}
	glm::mat4 final_ = mats[0];
	return final_;
}
#include "../../Debug.h"
#include "../../Game/Entity.h"
ConfigVar a_draw_ik_debug("a_draw_ik_debug", "0", CVAR_BOOL | CVAR_DEV, "");
void agIk2Bone::get_pose(agGetPoseCtx& ctx) {
	if (!has_init) {
		bone_idx = ctx.get_skeleton().get_bone_index(bone_name);
		if (bone_idx == -1)
			sys_print(Error, " agIk2Bone::get_pose: model doesnt have bone '%s'\n", bone_name.get_c_str());
		if (ik_in_bone_space) {
			other_bone_idx = ctx.get_skeleton().get_bone_index(other_bone);
			if (other_bone_idx == -1)
				sys_print(Error, " agIk2Bone::get_pose: model doesnt have other bone '%s'\n", other_bone.get_c_str());
		}
		if (pole_in_bone_space) {
			pole_bone_idx = ctx.get_skeleton().get_bone_index(pole_bone);
			if (pole_bone_idx == -1)
				sys_print(Error, " agIk2Bone::get_pose: model doesnt have pole bone '%s'\n", pole_bone.get_c_str());
		}
		has_init = true;
	}
	if (bone_idx == -1 || (ik_in_bone_space && other_bone_idx == -1) || (pole_in_bone_space && pole_bone_idx == -1)) {
		input->get_pose(ctx);
		return;
	}
	const float alphaVal = alpha.get_float(ctx);
	vec3 tagetVec = target.get_vec3(ctx);
	vec3 poleVec = pole.get_vec3(ctx);

	if (alphaVal <= 0.00001f) {
		input->get_pose(ctx);
		return;
	}

	input->get_pose(ctx);

	// Capture pre-IK pose for alpha blend-back; avoid re-evaluating input.
	Pose prePose;
	const bool partial = alphaVal < 0.99999f;
	if (partial) {
		const int nb = ctx.get_num_bones();
		Pose& cur = *ctx.pose;
		std::memcpy(prePose.q,     cur.q,     sizeof(glm::quat) * nb);
		std::memcpy(prePose.pos,   cur.pos,   sizeof(glm::vec3) * nb);
		std::memcpy(prePose.scale, cur.scale, sizeof(float)      * nb);
	}
	auto& pose = *ctx.pose;
	// build up global matrix when needed instead of recreating it every step
	// not sure if this is optimal, should profile different ways to pass around pose
	const int ALLOCED_MATS = 36;
	glm::mat4 mats[ALLOCED_MATS];
	int indicies[36];

	auto& skel = ctx.get_skeleton();
	int index = bone_idx;
	int count = 0;
	while (index != -1) {
		assert(count < ALLOCED_MATS);
		glm::mat4x4 matrix = glm::mat4_cast(pose.q[index]);
		matrix[3] = glm::vec4(pose.pos[index], 1.0);
		mats[count++] = matrix;
		indicies[count - 1] = index;
		index = skel.get_bone_parent(index);
	}
	for (int i = count - 2; i >= 0; i--) {
		mats[i] = mats[i + 1] * mats[i];
	}

	if (count <= 2) {
		sys_print(Error, "agIk2Bone::get_pose: ik attempted on some root bone %s\n", bone_name.get_c_str());
		throw std::runtime_error("ik error");
	}
	auto ent_transform = ctx.object.get_owner()->get_ws_transform();
	auto ikfunctor = [&](glm::quat& outlocal1, glm::quat& outlocal2, vec3 target, vec3 pole_target, bool print = false) {
		const float dist_eps = 0.0001f;
		// GLOBAL positions
		vec3 a = mats[2] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		vec3 b = mats[1] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		vec3 c = mats[0] * glm::vec4(0.0, 0.0, 0.0, 1.0);
		float dist = length(c - target);
		if (dist <= dist_eps) {
			return;
		}

		glm::quat a_global = glm::quat_cast(mats[2]);
		glm::quat b_global = glm::quat_cast(mats[1]);
		util_twobone_ik(a, b, c, target, pole_target, a_global, b_global, outlocal2, outlocal1);

		if (a_draw_ik_debug.get_bool()) {
			Debug::add_sphere(ent_transform * glm::vec4(a, 1.0), 0.01, COLOR_GREEN, 0.0, true);
			Debug::add_sphere(ent_transform * glm::vec4(b, 1.0), 0.01, COLOR_BLUE, 0.0, true);
			Debug::add_sphere(ent_transform * glm::vec4(c, 1.0), 0.01, COLOR_CYAN, 0.0, true);
		}
	};

	glm::quat target_rotation = {};

	if (ik_in_bone_space) {
		glm::mat4 matrix = build_global_transform_for_bone_index(&pose, &skel, other_bone_idx);
		tagetVec = matrix * glm::vec4(tagetVec, 1.0);
		if (take_rotation_of_other)
			target_rotation = glm::quat_cast(matrix);
	}
	if (pole_in_bone_space) {
		glm::mat4 matrix = build_global_transform_for_bone_index(&pose, &skel, pole_bone_idx);
		poleVec = matrix * glm::vec4(poleVec, 1.0);
	}

	int index1 = indicies[1];
	int index2 = indicies[2];

	ikfunctor(pose.q[index1], pose.q[index2], tagetVec, poleVec, false);

	if (ik_in_bone_space && take_rotation_of_other) {
		// compute the global rotation now
		glm::quat q = {};
		if (count >= 4)
			q = glm::quat_cast(mats[3]);
		q = q * pose.q[index2];
		q = q * pose.q[index1];

		pose.q[bone_idx] = glm::inverse(q) * target_rotation;
	}

	if (partial)
		util_blend(ctx.get_num_bones(), prePose, *ctx.pose, alphaVal);
}

void agModifyBone::reset() {
	input->reset();
}

void agModifyBone::get_pose(agGetPoseCtx& ctx) {
	if (!has_init) {
		bone_index = ctx.get_skeleton().get_bone_index(boneName);
		if (bone_index == -1)
			sys_print(Error, "agModifyBone::get_pose: no bone found '%s'\n", boneName.get_c_str());
		has_init = true;
	}
	if (bone_index == -1) {
		input->get_pose(ctx);
		return;
	}

	const float alphaVal = alpha.get_float(ctx);
	if (alphaVal <= 0.00001f) {
		input->get_pose(ctx);
		return;
	}

	input->get_pose(ctx);
	const int B = bone_index;
	const MSkeleton& skel = ctx.get_skeleton();
	Pose& pose = *ctx.pose;

	const glm::quat pre_q     = pose.q[B];
	const glm::vec3 pre_pos   = pose.pos[B];
	const float     pre_scale = pose.scale[B];

	const glm::vec3 set_pos   = translationVal.get_vec3(ctx);
	const glm::quat set_rot   = rotationVal.get_quat(ctx);
	const glm::vec3 set_scale = scaleVal.get_vec3(ctx);

	// Build global matrix chain only when meshspace channels are active.
	const bool needs_global = (translation == ModifyBoneType::Meshspace || translation == ModifyBoneType::MeshspaceAdd ||
	                           rotation    == ModifyBoneType::Meshspace || rotation    == ModifyBoneType::MeshspaceAdd);
	glm::mat4 mats[36];
	int count = 0;
	bool more_than_one = false;
	if (needs_global) {
		int index = B;
		while (index != -1) {
			assert(count < 36);
			glm::mat4 m = glm::mat4_cast(pose.q[index]);
			m[3] = glm::vec4(pose.pos[index], 1.f);
			mats[count++] = m;
			index = skel.get_bone_parent(index);
		}
		for (int i = count - 2; i >= 0; i--)
			mats[i] = mats[i + 1] * mats[i];
		more_than_one = count > 1;
	}

	// Translation
	switch (translation) {
	case ModifyBoneType::Localspace:    pose.pos[B] = set_pos; break;
	case ModifyBoneType::LocalspaceAdd: pose.pos[B] += set_pos; break;
	// BonespaceAdd: offset along bone's own axes (post-rotate offset into parent frame)
	case ModifyBoneType::Bonespace:     pose.pos[B] = pose.q[B] * set_pos; break;
	case ModifyBoneType::BonespaceAdd:  pose.pos[B] += pose.q[B] * set_pos; break;
	case ModifyBoneType::Meshspace:     mats[0][3] = glm::vec4(set_pos, 1.f); break;
	case ModifyBoneType::MeshspaceAdd:  mats[0][3] = glm::vec4(glm::vec3(mats[0][3]) + set_pos, 1.f); break;
	default: break;
	}

	// Rotation — key distinction: LocalspaceAdd pre-multiplies (rotates in parent frame),
	//            BonespaceAdd post-multiplies (rotates around the bone's own axes).
	switch (rotation) {
	case ModifyBoneType::Localspace:
	case ModifyBoneType::Bonespace:     pose.q[B] = set_rot; break;
	case ModifyBoneType::LocalspaceAdd: pose.q[B] = set_rot * pose.q[B]; break;
	case ModifyBoneType::BonespaceAdd:  pose.q[B] = pose.q[B] * set_rot; break;
	case ModifyBoneType::Meshspace: {
		glm::vec4 col3 = mats[0][3];
		mats[0] = glm::mat4_cast(set_rot);
		mats[0][3] = col3;
		break;
	}
	case ModifyBoneType::MeshspaceAdd: {
		glm::vec4 col3 = mats[0][3];
		mats[0] = glm::mat4_cast(set_rot * glm::quat_cast(mats[0]));
		mats[0][3] = col3;
		break;
	}
	default: break;
	}

	// Scale — bone/mesh space has no meaningful distinction; treat as local.
	switch (scale) {
	case ModifyBoneType::Localspace:
	case ModifyBoneType::Bonespace:
	case ModifyBoneType::Meshspace:     pose.scale[B] = set_scale.x; break;
	case ModifyBoneType::LocalspaceAdd:
	case ModifyBoneType::BonespaceAdd:
	case ModifyBoneType::MeshspaceAdd:  pose.scale[B] += set_scale.x; break;
	default: break;
	}

	// Convert meshspace result back to local.
	if (needs_global) {
		if (more_than_one)
			mats[0] = glm::inverse(mats[1]) * mats[0];
		if (rotation    == ModifyBoneType::Meshspace || rotation    == ModifyBoneType::MeshspaceAdd)
			pose.q[B]   = glm::quat_cast(mats[0]);
		if (translation == ModifyBoneType::Meshspace || translation == ModifyBoneType::MeshspaceAdd)
			pose.pos[B] = mats[0][3];
	}

	if (alphaVal < 0.99999f) {
		pose.q[B]     = glm::slerp(pre_q,     pose.q[B],     alphaVal);
		pose.pos[B]   = glm::mix  (pre_pos,   pose.pos[B],   alphaVal);
		pose.scale[B] = glm::mix  (pre_scale, pose.scale[B], alphaVal);
	}
}

void agCopyBone::reset() {
	input->reset();
}

void agCopyBone::get_pose(agGetPoseCtx& ctx) {
	if (!has_init) {
		source_bone_idx = ctx.get_skeleton().get_bone_index(sourceBone);
		target_bone_idx = ctx.get_skeleton().get_bone_index(targetBone);
		if (source_bone_idx == -1 || target_bone_idx == -1)
			sys_print(Error, "agCopyBone::get_pose: couldn't find bone (src='%s' dst='%s')\n",
				sourceBone.get_c_str(), targetBone.get_c_str());
		has_init = true;
	}
	if (source_bone_idx == -1 || target_bone_idx == -1) {
		input->get_pose(ctx);
		return;
	}

	const float alphaVal = alpha.get_float(ctx);
	if (alphaVal <= 0.00001f) {
		input->get_pose(ctx);
		return;
	}

	input->get_pose(ctx);

	const bool doRot   = copyRotation.get_bool(ctx);
	const bool doTrans = copyTranslation.get_bool(ctx);
	const bool doScale = copyScale.get_bool(ctx);
	if (!doRot && !doTrans && !doScale)
		return;

	const MSkeleton& skel = ctx.get_skeleton();
	Pose& pose = *ctx.pose;

	const glm::quat pre_q     = pose.q[target_bone_idx];
	const glm::vec3 pre_pos   = pose.pos[target_bone_idx];
	const float     pre_scale = pose.scale[target_bone_idx];

	if (copyBonespace) {
		if (doRot)   pose.q[target_bone_idx]     = pose.q[source_bone_idx];
		if (doTrans) pose.pos[target_bone_idx]   = pose.pos[source_bone_idx];
		if (doScale) pose.scale[target_bone_idx] = pose.scale[source_bone_idx];
	} else {
		glm::mat4 mat = build_global_transform_for_bone_index(&pose, &skel, source_bone_idx);
		if (doRot)   pose.q[target_bone_idx]     = glm::quat_cast(mat);
		if (doTrans) pose.pos[target_bone_idx]   = mat[3];
		if (doScale) pose.scale[target_bone_idx] = pose.scale[source_bone_idx]; // scale has no global-space meaning
	}

	if (alphaVal < 0.99999f) {
		if (doRot)   pose.q[target_bone_idx]     = glm::slerp(pre_q,     pose.q[target_bone_idx],     alphaVal);
		if (doTrans) pose.pos[target_bone_idx]   = glm::mix  (pre_pos,   pose.pos[target_bone_idx],   alphaVal);
		if (doScale) pose.scale[target_bone_idx] = glm::mix  (pre_scale, pose.scale[target_bone_idx], alphaVal);
	}
}

float agGetPoseCtx::get_float_var(StringName name) const {
	auto var = object.get_float_variable(name);
	if (var.has_value())
		return var.value();
	auto curve = object.get_curve_value(name);
	return curve.value_or(0.f);
}

glm::vec3 agGetPoseCtx::get_vec3_var(StringName name) const {
	auto var = object.get_vec3_variable(name);
	if (var.has_value())
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_vec3_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

glm::quat agGetPoseCtx::get_quat_var(StringName name) const {
	auto var = object.get_quat_variable(name);
	if (var.has_value())
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_quat_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

bool agGetPoseCtx::get_bool_var(StringName name) const {
	auto var = object.get_bool_variable(name);
	if (var.has_value())
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_bool_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

int agGetPoseCtx::get_int_var(StringName name) const {
	auto var = object.get_int_variable(name);
	if (var.has_value())
		return var.value();
	sys_print(Error, "agGetPoseCtx::get_int_var: no variable exists: %s\n", name.get_c_str());
	throw std::runtime_error("no variable exists");
}

void agBlendMasked::reset() {
	input0->reset();
	input1->reset();
}

void agBlendMasked::get_pose(agGetPoseCtx& ctx) {

	float alpha_val = alpha.get_float(ctx);
	ctx.debug_enter("agBlendMasked: " + std::to_string(alpha_val));
	if (alpha_val <= 0.00001f) {
		input0->get_pose(ctx);
	} else if (alpha_val >= 0.99999f) {
		input1->get_pose(ctx);
	} else {
		agGetPoseCtx basePose(ctx);
		// Base always contributes at full parent weight; override layer scales with alpha.
		ctx.weight = ctx.weight * alpha_val;
		input0->get_pose(basePose);
		input1->get_pose(ctx);
		ctx.weight = basePose.weight;
		if (meshspace_blend) {
			util_global_blend(&ctx.get_skeleton(), &(*basePose.pose), &(*ctx.pose), alpha_val, maskWeights);
		} else {
			util_blend_with_mask(ctx.get_num_bones(), (*basePose.pose), (*ctx.pose), alpha_val, maskWeights);
		}
	}
	ctx.debug_exit();
}

void agBlendMasked::init_mask_for_model(const Model* model, float default_weight) {
	assert(model);
	maskWeights.resize(model->get_skel()->get_num_bones(), default_weight);
}

void agBlendMasked::set_all_children_weights(const Model* model, string bone, float weight) {
	assert(model);
	const int myIndex = model->bone_for_name(StringName(bone.c_str()));
	if (myIndex == -1)
		throw std::runtime_error("set_all_children_weights: invalid bone");
	int num_bones = model->get_skel()->get_num_bones();
	maskWeights.at(myIndex) = weight;
	for (int i = myIndex + 1; i < num_bones; i++) {
		const int parent = model->get_skel()->get_bone_parent(i);
		if (parent < myIndex)
			break;
		maskWeights.at(i) = weight;
	}
}

void agBlendMasked::set_one_bone_weight(const Model* model, string bone, float weight) {
	assert(model);
	const int myIndex = model->bone_for_name(StringName(bone.c_str()));
	if (myIndex == -1)
		throw std::runtime_error("set_all_children_weights: invalid bone");
	maskWeights.at(myIndex) = weight;
}

void agStatemachineBase::reset() {
	currentTree = nullptr;
	curTime = 0.0;
	if (blendingOut) {
		g_pose_pool.free(blendingOut);
		blendingOut = nullptr;
	}
}

void agStatemachineBase::get_pose(agGetPoseCtx& ctx) {
	if (!currentTree) {
		update(&ctx, true);
		if (currentTree) {
			currentTree->reset();
		} else {
			sys_print(Error, "agStatemachineBase::reset: no tree after update?\n");
			return;
		}
	}

	const float parent_weight = ctx.weight;
	if (blendingOut) {
		float time_left = get_transition_time_left();
		ctx.debug_enter("agStatemachineBase: transitoning " + std::to_string(time_left));
		// Scale incoming state events by blend alpha; outgoing is a static capture so its events are suppressed.
		float alpha = curTransitionDuration >= 0.00001f ? glm::clamp(curTransitionTime / curTransitionDuration, 0.f, 1.f) : 1.f;
		alpha = evaluate_easing(curTransition, alpha);
		ctx.weight *= alpha;
	} else {
		ctx.debug_enter("agStatemachineBase");
	}
	currentTree->get_pose(ctx);
	// ctx is a reference to the caller's context (not a copy); weight must only
	// propagate downward into children, so restore it before returning -- otherwise
	// a sibling node evaluated after us on the same ctx (or the parent itself) would
	// see this node's scaled-down weight instead of the original parent weight.
	ctx.weight = parent_weight;
	ctx.debug_exit();

	// update transitions
	if (blendingOut) {
		float alpha = curTransitionDuration >= 0.00001 ? curTransitionTime / curTransitionDuration : 1.0f;
		glm::clamp(alpha, 0.f, 1.f);
		alpha = evaluate_easing(curTransition, alpha);
		agGetPoseCtx other(ctx);
		util_blend(ctx.get_num_bones(), *blendingOut, *ctx.pose,
				   1.0 - alpha); // blend the last transition pose to the cur tree

		if (curTransitionTime >= curTransitionDuration) {
			g_pose_pool.free(blendingOut);
			blendingOut = nullptr;
			sys_print(Debug, "agStatemachineBase: transition end\n");
		} else {
			curTransitionTime += ctx.dt;
		}
	}
	auto preTree = currentTree;
	update(&ctx, false);
	curTime += ctx.dt;
	if (preTree != currentTree) {
		curTime = 0.0;
		curTransitionTime = 0.0;
		currentTree->reset();
		sys_print(Debug, "agStatemachineBase: transition\n");
		if (blendingOut) {
			sys_print(Debug, "agStatemachineBase: transition interrupted\n");
		} else {
			blendingOut = g_pose_pool.allocate();
		}
		auto& curPose = *ctx.pose;
		std::memcpy(blendingOut->pos, curPose.pos, sizeof(glm::vec3) * ctx.get_num_bones());
		std::memcpy(blendingOut->q, curPose.q, sizeof(glm::quat) * ctx.get_num_bones());
		std::memcpy(blendingOut->scale, curPose.scale, sizeof(float) * ctx.get_num_bones());
	}
}

void agStatemachineBase::set_pose(agBaseNode* pose) {
	currentTree = pose;
}

// use set_transition before a pose change to set how it transitions

void agStatemachineBase::set_transition_parameters(Easing easing, float blend_time) {
	this->curTransition = easing;
	this->curTransitionDuration = blend_time;
}

void agSlotPlayer::update(agGetPoseCtx* ctx, bool wantsReset) {
	auto slotPlayer = ctx->object.find_slot_with_name(slotName);
	if (slotPlayer) {
		clipPlayer.slot = slotPlayer;
	} else {
		sys_print(Warning, "agSlotPlayer::update: no slot %s\n", slotName.get_c_str());
		return;
	}
	set_transition_parameters(Easing::Linear, 0.2);
	if (slotPlayer->active && slotPlayer->time_remaining() > 0.2 /* start fading out */) {
		set_pose(&clipPlayer);
	} else {
		set_pose(input);
	}
}

void agBlendByInt::update(agGetPoseCtx* ctx, bool wantsReset) {
	if (inputs.empty()) {
		sys_print(Error, "agBlendByInt::update: no inuts?\n");
		throw std::runtime_error("agBlendByInt::update");
	}
	int index = integer.get_int(*ctx);
	if (index < 0 || index >= inputs.size()) {
		sys_print(Warning, "agBlendByInt::update: index out of range (%d, size=%d)\n", index, (int)inputs.size());
		index = 0;
	}
	set_transition_parameters(easing, blending_duration);
	set_pose(inputs.at(index));
}

void agSlotClipInternal::reset() {
	// nothing
}
#include "Animation.h"
void agSlotClipInternal::get_pose(agGetPoseCtx& ctx) {
	if (!slot || !slot->active || !slot->active->seq) { // cleared by model reload or never set
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}
	bool stopped_flag = false;
	float time_in = slot->time;
	get_clip_pose_shared(ctx, slot->active->seq, false, {}, {}, false, nullptr, 0.0, time_in, stopped_flag, nullptr);
}

// Eases a blend space axis value from `state.target` toward `newTarget` over
// params.smoothingTime seconds, shaped by params.smoothingType. Restarting from the
// currently-interpolated value (not from the old start) avoids a pop if the target moves
// again mid-transition.
static float step_axis_smoothing(const BlendSpaceAxisSmoothing& params, AxisSmoothState& state, float newTarget,
								 float dt) {
	if (!state.hasValue) {
		state.start = state.target = newTarget;
		state.elapsed = 0.f;
		state.hasValue = true;
		return newTarget;
	}
	if (!params.enabled || params.smoothingTime <= 0.00001f) {
		state.start = state.target = newTarget;
		state.elapsed = 0.f;
		return newTarget;
	}
	const float alpha_prev = glm::clamp(state.elapsed / params.smoothingTime, 0.f, 1.f);
	const float current = glm::mix(state.start, state.target, evaluate_easing(params.smoothingType, alpha_prev));
	if (glm::abs(newTarget - state.target) > 0.00001f) {
		state.start = current;
		state.target = newTarget;
		state.elapsed = 0.f;
	}
	state.elapsed += dt;
	const float alpha = glm::clamp(state.elapsed / params.smoothingTime, 0.f, 1.f);
	return glm::mix(state.start, state.target, evaluate_easing(params.smoothingType, alpha));
}

// Unreal's "Target Weight Interpolation": rather than a sample's blend weight snapping to its
// instantaneous target every frame, ramp each sample's weight toward its target at a max rate
// of `speed` per second (e.g. a sample that just dropped out of the active bracket/triangle
// fades out over 1/speed seconds instead of disappearing on the spot), then renormalize so
// the ramping weights still sum to 1.
static void apply_weight_speed(std::vector<float>& smoothed, const std::vector<float>& targets, bool enabled,
							   float speed, float dt) {
	smoothed.resize(targets.size(), 0.f);
	if (!enabled || speed <= 0.00001f) {
		smoothed = targets;
		return;
	}
	const float maxStep = speed * dt;
	for (size_t i = 0; i < targets.size(); i++) {
		const float diff = targets[i] - smoothed[i];
		if (diff > maxStep)
			smoothed[i] += maxStep;
		else if (diff < -maxStep)
			smoothed[i] -= maxStep;
		else
			smoothed[i] = targets[i];
	}
	float sum = 0.f;
	for (float w : smoothed) sum += w;
	if (sum > 0.00001f)
		for (float& w : smoothed) w /= sum;
}

static ResolvedBlendSample resolve_blend_sample(const BlendSpaceSample& s, MSkeleton& targetSkel) {
	ResolvedBlendSample out;
	const AnimationSeqAsset* asset = s.Clip.get();
	if (!asset || !asset->seq || !asset->srcModel || !asset->srcModel->get_skel()) {
		sys_print(Error, "BlendSpace: sample has no valid clip\n");
		return out;
	}
	out.seq = asset->seq;
	out.remap = targetSkel.get_remap(asset->srcModel->get_skel());
	return out;
}

void agBlendSpace1D::reset() {
	animTime = 0.f;
	xSmoothState = {};
	smoothedWeights.clear();
}
void agBlendSpace1D::refresh_after_model_reload(Model* reloaded) {
	dirty = true; // see agClipNode::refresh_after_model_reload: remap cache was wiped
}
void agBlendSpace1D::add_sample(const AnimationSeqAsset* asset, float position) {
	BlendSpaceSample s;
	s.Clip = AssetPtr<AnimationSeqAsset>(const_cast<AnimationSeqAsset*>(asset)); // AssetPtr is read-only here
	s.PosX = position;
	samples.push_back(s);
	dirty = true;
}
void agBlendSpace1D::build_if_dirty(MSkeleton& skel) {
	if (!dirty)
		return;
	resolved.resize(samples.size());
	for (size_t i = 0; i < samples.size(); i++) {
		resolved[i] = resolve_blend_sample(samples[i], skel);
		if (resolved[i].seq && resolved[i].seq->is_additive_clip != isAdditive)
			sys_print(Warning, "agBlendSpace1D: sample %zu additive-clip flag doesn't match blend space's isAdditive\n", i);
	}
	sortedOrder.resize(samples.size());
	for (size_t i = 0; i < samples.size(); i++)
		sortedOrder[i] = (int)i;
	std::sort(sortedOrder.begin(), sortedOrder.end(),
			 [&](int a, int b) { return samples[a].PosX < samples[b].PosX; });
	dirty = false;
}
void agBlendSpace1D::get_pose(agGetPoseCtx& ctx) {
	build_if_dirty(ctx.get_skeleton());
	if (sortedOrder.empty()) {
		sys_print(Error, "agBlendSpace1D::get_pose: no samples\n");
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}
	ctx.debug_enter("agBlendSpace1D");

	const float target = xInput.get_float(ctx);
	const float smoothedX = step_axis_smoothing(xSmoothing, xSmoothState, target, ctx.dt);

	// Find the pair of samples bracketing smoothedX (clamped to the sample range).
	int lo = 0;
	while (lo + 1 < (int)sortedOrder.size() && samples[sortedOrder[lo + 1]].PosX < smoothedX)
		lo++;
	const int iA = sortedOrder[lo];
	const int iB = sortedOrder[glm::min(lo + 1, (int)sortedOrder.size() - 1)];
	const float posA = samples[iA].PosX, posB = samples[iB].PosX;
	const float alpha = (iA == iB || posB - posA < 0.00001f)
							? 0.f
							: glm::clamp((smoothedX - posA) / (posB - posA), 0.f, 1.f);

	std::vector<float> targetWeights(samples.size(), 0.f);
	targetWeights[iA] += 1.f - alpha;
	targetWeights[iB] += alpha; // += so iA==iB (single active sample) still ends at 1.0

	apply_weight_speed(smoothedWeights, targetWeights, enableWeightSpeed, weightSpeed, ctx.dt);

	std::vector<ActiveBlendSample> actives;
	for (size_t i = 0; i < smoothedWeights.size(); i++)
		if (smoothedWeights[i] > 0.0001f)
			actives.push_back({resolved[i].seq, resolved[i].remap, smoothedWeights[i]});

	blend_space_evaluate(ctx, actives, syncGroup, syncType, looping, speed.get_float(ctx), animTime);
	ctx.debug_exit();
}

void agBlendSpace2D::reset() {
	animTime = 0.f;
	xSmoothState = {};
	ySmoothState = {};
	smoothedWeights.clear();
}
void agBlendSpace2D::refresh_after_model_reload(Model* reloaded) {
	dirty = true;
}
void agBlendSpace2D::add_sample(const AnimationSeqAsset* asset, float x, float y) {
	BlendSpaceSample s;
	s.Clip = AssetPtr<AnimationSeqAsset>(const_cast<AnimationSeqAsset*>(asset));
	s.PosX = x;
	s.PosY = y;
	samples.push_back(s);
	dirty = true;
}
void agBlendSpace2D::build_if_dirty(MSkeleton& skel) {
	if (!dirty)
		return;
	resolved.resize(samples.size());
	std::vector<glm::vec2> points(samples.size());
	for (size_t i = 0; i < samples.size(); i++) {
		resolved[i] = resolve_blend_sample(samples[i], skel);
		if (resolved[i].seq && resolved[i].seq->is_additive_clip != isAdditive)
			sys_print(Warning, "agBlendSpace2D: sample %zu additive-clip flag doesn't match blend space's isAdditive\n", i);
		points[i] = samples[i].grid_pos();
	}
	triangles = delaunay_triangulate_2d(points);
	dirty = false;
}
void agBlendSpace2D::get_pose(agGetPoseCtx& ctx) {
	build_if_dirty(ctx.get_skeleton());
	if (samples.empty()) {
		sys_print(Error, "agBlendSpace2D::get_pose: no samples\n");
		util_set_to_bind_pose(*ctx.pose, &ctx.get_skeleton());
		return;
	}
	ctx.debug_enter("agBlendSpace2D");

	const glm::vec2 target(xInput.get_float(ctx), yInput.get_float(ctx));
	const glm::vec2 smoothedPos(step_axis_smoothing(xSmoothing, xSmoothState, target.x, ctx.dt),
								step_axis_smoothing(ySmoothing, ySmoothState, target.y, ctx.dt));

	std::vector<float> targetWeights(samples.size(), 0.f);
	if (samples.size() == 1) {
		targetWeights[0] = 1.f;
	} else if (triangles.empty()) {
		// Fewer than 3 non-degenerate samples to triangulate: fall back to a 1D-style blend
		// between the first two samples, projecting the input onto the segment between them.
		const glm::vec2 posA = samples[0].grid_pos(), posB = samples[1].grid_pos();
		const glm::vec2 dir = posB - posA;
		const float lenSq = glm::dot(dir, dir);
		const float alpha = lenSq > 0.00001f ? glm::clamp(glm::dot(smoothedPos - posA, dir) / lenSq, 0.f, 1.f) : 0.f;
		targetWeights[0] += 1.f - alpha;
		targetWeights[1] += alpha;
	} else {
		// Find the triangle containing smoothedPos; if it's outside every triangle (outside
		// the sample hull), fall back to whichever triangle clamps closest (least negative
		// barycentric overshoot), matching Unreal's clamp-to-hull behavior for out-of-range input.
		int best = -1;
		float bestOverflow = std::numeric_limits<float>::max();
		float bu = 0.f, bv = 0.f, bw = 0.f;
		for (int ti = 0; ti < (int)triangles.size(); ti++) {
			const Triangle2D& t = triangles[ti];
			float u, v, w;
			const bool inside = barycentric_2d(samples[t.a].grid_pos(), samples[t.b].grid_pos(), samples[t.c].grid_pos(),
											   smoothedPos, u, v, w);
			if (inside) {
				best = ti;
				bu = u;
				bv = v;
				bw = w;
				break;
			}
			const float overflow = glm::max(0.f, -u) + glm::max(0.f, -v) + glm::max(0.f, -w);
			if (overflow < bestOverflow) {
				bestOverflow = overflow;
				best = ti;
				bu = u;
				bv = v;
				bw = w;
			}
		}
		const Triangle2D& t = triangles[best];
		bu = glm::max(bu, 0.f);
		bv = glm::max(bv, 0.f);
		bw = glm::max(bw, 0.f);
		const float sum = bu + bv + bw;
		if (sum > 0.00001f) {
			bu /= sum;
			bv /= sum;
			bw /= sum;
		} else {
			bu = 1.f;
		}
		targetWeights[t.a] += bu;
		targetWeights[t.b] += bv;
		targetWeights[t.c] += bw;
	}

	apply_weight_speed(smoothedWeights, targetWeights, enableWeightSpeed, weightSpeed, ctx.dt);

	std::vector<ActiveBlendSample> actives;
	for (size_t i = 0; i < smoothedWeights.size(); i++)
		if (smoothedWeights[i] > 0.0001f)
			actives.push_back({resolved[i].seq, resolved[i].remap, smoothedWeights[i]});

	blend_space_evaluate(ctx, actives, syncGroup, syncType, looping, speed.get_float(ctx), animTime);
	ctx.debug_exit();
}
