#include "AnimationUtil.h"

#include "Debug.h"
#include "Animation/SkeletonData.h"

static glm::mat4 debug_animation_transform = glm::mat4(1);

void Animation_Debug::set_local_to_world(glm::mat4 transform) {
	debug_animation_transform = transform;
}
void Animation_Debug::push_line(const glm::mat4& transform_me, const glm::mat4& transform_parent, bool has_parent) {

	const auto& transform = debug_animation_transform;
	vec3 org = transform * transform_me[3];
	Color32 colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
	for (int i = 0; i < 3; i++) {
		vec3 dir = glm::mat3(transform) * transform_me[i];
		dir = normalize(dir);
		Debug::add_line(org, org + dir * 0.1f, colors[i], -1.f, false);
	}

	if (has_parent) {
		vec3 parent_org = transform * transform_parent[3];
		Debug::add_line(org, parent_org, COLOR_PINK, -1.f, false);
	}
}
void Animation_Debug::push_sphere(const glm::vec3& p, float radius) {
	glm::vec3 org = debug_animation_transform * glm::vec4(p, 1.0);
	Debug::add_sphere(org, radius, COLOR_GREEN, -1.f, false);
}

// source = reference pose - source
void util_subtract(int bonecount, const Pose& reference, Pose& source) {
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.scale[i] = source.scale[i] - reference.scale[i];

		source.q[i] = quat_delta(reference.q[i], source.q[i]);
	}
}

// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor) {
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
		b.scale[i] = glm::mix(b.scale[i], a.scale[i], factor);
	}
}
void util_blend_with_mask(int bonecount, const Pose& a, Pose& b, float factor, const std::vector<float>& mask) {
	ASSERT(mask.size() >= bonecount);
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(a.q[i], b.q[i], factor * mask[i]);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(a.pos[i], b.pos[i], factor * mask[i]);
		b.scale[i] = glm::mix(a.scale[i], b.scale[i], factor * mask[i]);
	}
}

static glm::quat quat_blend_additive(const glm::quat& a, const glm::quat& b, float t) {
	glm::quat target = b * a;
	return glm::slerp(a, target, t);
}

#include "Framework/Config.h"

static glm::quat forward_dir = glm::quat(0.0, 0, 0, 1);
static bool face_foward = true;
static bool apply_to_local = false;
static bool apply_to_all = false;
#include "imgui.h"
#include "GameEnginePublic.h"
void menu1235() {
	if (ImGui::Begin("abc")) {
		ImGui::SliderFloat4("dir", &forward_dir.x, -1, 1);
		ImGui::Checkbox("ff", &face_foward);
		ImGui::Checkbox("apply_to_local", &apply_to_local);
		ImGui::Checkbox("apply_to_all", &apply_to_all);

		forward_dir = glm::normalize(forward_dir);
	}
	ImGui::End();

	glm::mat4 transform = glm::mat4_cast(forward_dir);
	Debug::add_line(vec3(0.f), transform[0], COLOR_RED, -1.f, false);
	Debug::add_line(vec3(0.f), transform[1], COLOR_GREEN, -1.f, false);
	Debug::add_line(vec3(0.f), transform[2], COLOR_BLUE, -1.f, false);
}

static AddToDebugMenu aeasdf("meun", menu1235);

static void draw_skeleton_util_debug(const glm::mat4* bones, const MSkeleton* model, int count, float line_len,
									 const glm::mat4& transform) {

	for (int index = 0; index < count; index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = {COLOR_RED, COLOR_GREEN, COLOR_BLUE};
		for (int i = 0; i < 3; i++) {
			vec3 dir = glm::mat3(transform) * bones[index][i];
			dir = normalize(dir);
			Debug::add_line(org, org + dir * line_len, colors[i], -1.f, false);
		}

		if (model->get_bone_parent(index) != -1) {
			vec3 parent_org = transform * bones[model->get_bone_parent(index)][3];
			Debug::add_line(org, parent_org, COLOR_PINK, -1.f, false);
		}
	}
}

void util_meshspace_to_localspace(const glm::mat4* mesh, const MSkeleton* mod, Pose* out) {
	const int count = mod->get_num_bones();
	for (int i = 0; i < count; i++) {

		int parent = mod->get_bone_parent(i);

		glm::mat4 matrix = mesh[i];
		if (parent != -1) {
			matrix = glm::inverse(mesh[parent]) * matrix;
		}

		out->pos[i] = matrix[3];
		out->q[i] = glm::quat_cast(matrix);
		out->scale[i] = glm::length(matrix[0]);
	}
}

void util_localspace_to_meshspace_ptr_2(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* skel) {
	for (int i = 0; i < skel->get_num_bones(); i++) {
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (skel->get_bone_parent(i) == -1) {
			out_bone_matricies[i] = matrix;
		} else {
			assert(skel->get_bone_parent(i) < skel->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[skel->get_bone_parent(i)] * matrix;
		}
	}
	// for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}

void util_global_blend(const MSkeleton* skel, const Pose* a, Pose* b, float factor, const std::vector<float>& mask) {

	const int bonecount = skel->get_num_bones();
	ASSERT(bonecount <= Pose::MAX_BONES);

	glm::mat4 globalspace_base[Pose::MAX_BONES];
	glm::mat4 globalspace_layer[Pose::MAX_BONES];
	glm::quat globalspace_rotations[Pose::MAX_BONES];

	util_localspace_to_meshspace_ptr_2(*a, globalspace_base, skel);
	util_localspace_to_meshspace_ptr_2(*b, globalspace_layer, skel);

	for (int j = 0; j < bonecount; j++) {
		const float w = mask[j] * factor;
		const glm::quat base_g  = glm::quat_cast(globalspace_base[j]);
		const glm::quat layer_g = glm::quat_cast(globalspace_layer[j]);
		const glm::quat global  = glm::slerp(base_g, layer_g, w);
		globalspace_rotations[j] = global;

		const int parent = skel->get_bone_parent(j);
		b->q[j] = (parent == -1) ? global
		                         : glm::inverse(globalspace_rotations[parent]) * global;

		// Blend translation and scale in local space by the same weight.
		b->pos[j]   = glm::mix(a->pos[j],   b->pos[j],   w);
		b->scale[j] = glm::mix(a->scale[j], b->scale[j], w);
	}
}

// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac) {
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.scale[i] = glm::mix(base.scale[i], base.scale[i] + additive.scale[i], fac);

		glm::quat target = additive.q[i] * base.q[i];

		base.q[i] = glm::slerp(base.q[i], target, fac);
		// base.q[i] = glm::normalize(base.q[i]);
	}
}

// Shortest-arc rotation that takes unit vector `from` onto unit vector `to`.
// Handles the antiparallel (180 deg) case, which glm::rotation does not do robustly.
static glm::quat util_rotation_between(vec3 from, vec3 to) {
	from = normalize(from);
	to = normalize(to);
	float d = glm::clamp(dot(from, to), -1.f, 1.f);
	if (d > 0.999999f)
		return glm::quat(1, 0, 0, 0);
	if (d < -0.999999f) {
		// pick any axis perpendicular to `from`
		vec3 axis = cross(vec3(1, 0, 0), from);
		if (length(axis) < 1e-4f)
			axis = cross(vec3(0, 1, 0), from);
		return glm::angleAxis(glm::pi<float>(), normalize(axis));
	}
	return glm::angleAxis(acos(d), normalize(cross(from, to)));
}

// Analytic two-bone IK. Joints a (root, e.g. upper arm), b (mid, elbow), c (end, wrist)
// are GLOBAL positions; a_global_rotation/b_global_rotation are the GLOBAL rotations of
// bones a and b. On return a_local_rotation/b_local_rotation are post-multiplied by the
// LOCAL deltas that place c on `target`.
//
// Three passes — the order matters and the first was the source of a long-standing bug:
//   1. AIM:   rotate bone a so the a->c ray points straight at a->target. This pulls the
//             target INTO the bend plane. (Doing bend+aim together fails whenever the
//             target is out of the current a-b-c plane: reach comes out right but the
//             hand lands off to the side — see TestTwoBoneIk.)
//   2. TWIST: rotate around the aim axis so the elbow (b) swings into the plane that
//             contains the pole/joint target. The BEND pass below is only valid within
//             whatever plane b actually sits in, so the pole vector has to move b there
//             first rather than just picking an arbitrary bend-plane normal.
//   3. BEND:  with the chain now coplanar with the target AND the pole, bend a and b
//             within that plane (law of cosines) to set |a-c| = len_at, leaving c on the
//             target ray. This mirrors Unreal's Joint Target: the pole vector fully
//             determines which side the joint bends towards.
void util_twobone_ik(const vec3& a, const vec3& b, const vec3& c, const vec3& target, const vec3& pole_target,
					 const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
					 glm::quat& a_local_rotation, glm::quat& b_local_rotation) {
	const float eps = 0.01f;
	const float len_ab = length(b - a);
	const float len_cb = length(c - b);
	const float len_at = glm::clamp(length(target - a), eps, len_ab + len_cb - eps);

	// --- Pass 1: aim bone a so (c - a) points along (target - a) ---
	// Apply a world-space rotation Q to bone a via: a_local *= inverse(a_gr) * Q * a_gr.
	const glm::quat q_aim = util_rotation_between(c - a, target - a);
	a_local_rotation = a_local_rotation * (glm::inverse(a_global_rotation) * q_aim * a_global_rotation);
	glm::quat a_gr2 = q_aim * a_global_rotation;
	glm::quat b_gr2 = q_aim * b_global_rotation;
	vec3 b2 = a + q_aim * (b - a);
	const vec3 c2 = a + q_aim * (c - a); // now colinear with a->target

	// --- Pass 2: twist around the aim axis to swing b into the pole's plane ---
	const vec3 aim_dir = c2 - a; // parallel to target - a after pass 1
	const float aim_len = length(aim_dir);
	if (aim_len > 1e-6f) {
		const vec3 aim_axis = aim_dir / aim_len;
		const vec3 to_b = (b2 - a) - aim_axis * dot(b2 - a, aim_axis);
		const vec3 to_pole = (pole_target - a) - aim_axis * dot(pole_target - a, aim_axis);
		if (length(to_b) > 1e-6f && length(to_pole) > 1e-6f) {
			// signed angle about aim_axis, to guarantee the twist stays on-axis even
			// when to_b/to_pole are nearly anti-parallel (unlike a generic quat-between).
			const float twist_angle = atan2(dot(cross(to_b, to_pole), aim_axis), dot(to_b, to_pole));
			const glm::quat q_twist = glm::angleAxis(twist_angle, aim_axis);
			a_local_rotation = a_local_rotation * (glm::inverse(a_gr2) * q_twist * a_gr2);
			a_gr2 = q_twist * a_gr2;
			b_gr2 = q_twist * b_gr2;
			b2 = a + q_twist * (b2 - a);
		}
	}

	// --- Pass 3: bend within the now pole-aligned plane ---
	const float a_interior = acos(glm::clamp(dot(normalize(c2 - a), normalize(b2 - a)), -1.f, 1.f));
	const float b_interior = acos(glm::clamp(dot(normalize(a - b2), normalize(c2 - b2)), -1.f, 1.f));
	const float a_desired = acos(LawOfCosines(len_cb, len_ab, len_at));
	const float b_desired = acos(LawOfCosines(len_at, len_ab, len_cb));

	// Degenerate only when the arm is perfectly straight (b on the a->c line).
	vec3 axis0 = cross(c2 - a, b2 - a);
	if (length(axis0) < 1e-5f) {
		axis0 = cross(aim_dir, vec3(0.f, 1.f, 0.f));
		if (length(axis0) < 1e-5f)
			axis0 = cross(aim_dir, vec3(0.f, 0.f, 1.f));
	}
	axis0 = normalize(axis0);

	const glm::quat rot0 = glm::angleAxis(a_desired - a_interior, glm::inverse(a_gr2) * axis0);
	const glm::quat rot1 = glm::angleAxis(b_desired - b_interior, glm::inverse(b_gr2) * axis0);
	a_local_rotation = a_local_rotation * rot0;
	b_local_rotation = b_local_rotation * rot1;
}

// y2 = blend( blend(x1,x2,fac.x), blend(y1,y2,fac.x), fac.y)
#define IS_HIGH_BIT_SET(x) (x & (1u << 31u))
#define UNSET_HIGH_BIT(x) (x & ~(1u << 31u))

glm::vec3* AnimationSeq::get_pos_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);
	if (IS_HIGH_BIT_SET(offset.pos)) {
		if (keyframe > 0)
			return nullptr;
		return (glm::vec3*)(pose_data.data() + actual_ofs);
	} else {
		return ((glm::vec3*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}
float* AnimationSeq::get_scale_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
	if (IS_HIGH_BIT_SET(offset.scale)) {
		if (keyframe > 0)
			return nullptr;
		return (float*)(pose_data.data() + actual_ofs);
	} else {
		return ((float*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}

glm::quat* AnimationSeq::get_quat_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
	if (IS_HIGH_BIT_SET(offset.rot)) {
		if (keyframe > 0)
			return nullptr;
		return (glm::quat*)(pose_data.data() + actual_ofs);
	} else {
		return ((glm::quat*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}

ScalePositionRot AnimationSeq::get_keyframe(int bone, int keyframe, float lerpamt) const {
	// TODO: insert return statement here
	ChannelOffset offset = channel_offsets[bone];

	ScalePositionRot output;

	if (IS_HIGH_BIT_SET(offset.pos)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);
		output.pos = *(glm::vec3*)(pose_data.data() + actual_ofs);
	} else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);

		glm::vec3* ptr = (glm::vec3*)(pose_data.data() + actual_ofs);

		glm::vec3 p0 = ptr[keyframe];
		glm::vec3 p1 = ptr[keyframe + 1];

		output.pos = glm::mix(p0, p1, lerpamt);
	}

	if (IS_HIGH_BIT_SET(offset.rot)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
		output.rot = *(glm::quat*)(pose_data.data() + actual_ofs);
	} else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
		glm::quat* ptr = (glm::quat*)(pose_data.data() + actual_ofs);

		glm::quat r0 = ptr[keyframe];
		glm::quat r1 = ptr[keyframe + 1];

		output.rot = glm::slerp(r0, r1, lerpamt);
	}

	if (IS_HIGH_BIT_SET(offset.scale)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
		output.scale = *(float*)(pose_data.data() + actual_ofs);
	} else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
		float* ptr = (float*)(pose_data.data() + actual_ofs);

		float s0 = ptr[keyframe];
		float s1 = ptr[keyframe + 1];

		output.scale = glm::mix(s0, s1, lerpamt);
	}

	return output;
}

void AnimationSeq::get_active_events(float time, vector<const AnimEvent*>& out) const {
	for (auto& ev : anim_events) {
		if (ev.is_duration) {
			if (time >= ev.time_start && time <= ev.time_end)
				out.push_back(&ev);
		} else {
			if (time == ev.time_start)
				out.push_back(&ev);
		}
	}
}

#include "Animation/SkeletonData.h"
ConfigVar skeleton_calc_rotations_force_from_anim("skeleton_calc_rotations_force_from_anim", "0", CVAR_BOOL, "");

static std::unordered_map<std::string, int> bone_to_bool;
void bone_menu() {
	Model* m = Model::load("indiana.cmdl");
	for (auto& b : m->get_skel()->get_all_bones()) {
		if (bone_to_bool.find(b.strname) == bone_to_bool.end())
			bone_to_bool[b.strname] = (int)b.retarget_type;
		int bb = bone_to_bool[b.strname];
		ImGui::SliderInt(b.strname.c_str(), &bb, 0, 2);
		bone_to_bool[b.strname] = bb;
	}
}
ADD_TO_DEBUG_MENU(bone_menu);

void util_calc_rotations(const MSkeleton* skeleton, const AnimationSeq* clip, float time,
						 const BoneIndexRetargetMap* remap_indicies, Pose& outpose,
						 bool looping) {
	const int count = skeleton->get_num_bones();
	int keyframe = clip->get_frame_for_time(time);
	float lerp_amt = MidLerp(clip->get_time_of_keyframe(keyframe), clip->get_time_of_keyframe(keyframe + 1), time);

	for (int dest_idx = 0; dest_idx < count; dest_idx++) {
		int src_idx = (remap_indicies) ? (remap_indicies->my_skeleton_to_who)[dest_idx] : dest_idx;

		if (src_idx == -1) {
			outpose.pos[dest_idx] = skeleton->get_bone_local_transform(dest_idx)[3];
			outpose.q[dest_idx] = skeleton->get_bone_local_rotation(dest_idx);
			outpose.q[dest_idx] = glm::normalize(outpose.q[dest_idx]);
			outpose.scale[dest_idx] = 1.0;
			continue;
		}

		ScalePositionRot transform;
		if (looping && keyframe == clip->get_num_keyframes_exclusive() - 1) {
			auto last = clip->get_keyframe(src_idx, keyframe, 0.0f);
			auto first = clip->get_keyframe(src_idx, 0, 0.0f);
			transform.pos = glm::mix(last.pos, first.pos, lerp_amt);
			glm::quat first_rot = (glm::dot(last.rot, first.rot) < 0.f) ? -first.rot : first.rot;
			transform.rot = glm::slerp(last.rot, first_rot, lerp_amt);
			transform.scale = glm::mix(last.scale, first.scale, lerp_amt);
		} else {
			transform = clip->get_keyframe(src_idx, keyframe, lerp_amt);
		}
		outpose.pos[dest_idx] = transform.pos;
		outpose.q[dest_idx] = transform.rot;
		outpose.scale[dest_idx] = transform.scale;
	}
	if (remap_indicies) {
#if 1
		const bool force_to_from_anim = skeleton_calc_rotations_force_from_anim.get_bool();
		for (int dest_idx = 0; dest_idx < count; dest_idx++) {
			int src_idx = (remap_indicies) ? (remap_indicies->my_skeleton_to_who)[dest_idx] : dest_idx;
			if (src_idx == -1)
				continue;

			auto& bone = skeleton->get_all_bones()[dest_idx];
			auto get_op = [&]() {
				if (bone_to_bool.find(bone.strname) == bone_to_bool.end())
					bone_to_bool[bone.strname] = (int)bone.retarget_type;
			};
			get_op();
			const int opposite_day = bone_to_bool[bone.strname];

			auto type = bone.retarget_type;

			// if (type!=RetargetBoneType::FromTargetBindPose) {
			type = RetargetBoneType(opposite_day);
			//}

			if (type == RetargetBoneType::FromAnimation || force_to_from_anim) {
				// do nothing
				outpose.q[dest_idx] = remap_indicies->my_skelton_to_who_quat_delta.at(dest_idx) * outpose.q[dest_idx];

			} else if (type == RetargetBoneType::FromAnimationScaled) {
				float scale = glm::length(skeleton->get_bone_local_transform(dest_idx)[3]);
				scale /= glm::max(glm::length(remap_indicies->who->get_bone_local_transform(src_idx)[3]), 0.00001f);
				if (glm::abs(scale) < 0.00001)
					scale = 1.0;
				outpose.pos[dest_idx].y += (1 - scale);

				outpose.q[dest_idx] = remap_indicies->my_skelton_to_who_quat_delta.at(dest_idx) * outpose.q[dest_idx];

			} else if (type == RetargetBoneType::FromTargetBindPose) {
				outpose.pos[dest_idx] = skeleton->get_bone_local_transform(dest_idx)[3];
			}
		}
#endif
	}
}

void util_set_to_bind_pose(Pose& pose, const MSkeleton* skel) {
	for (int i = 0; i < skel->get_num_bones(); i++) {
		pose.pos[i] = skel->get_bone_local_transform(i)[3];
		pose.q[i] = skel->get_bone_local_rotation(i);
		pose.scale[i] = 1.f;
	}
}

static const int ROOT_BONE = -1;
// ConfigVar skip_scale_in_animation("skip_scale_in_animation", "0", CVAR_BOOL, "");
void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies,
								  const MSkeleton* model) {
	// const bool skip_scale = skip_scale_in_animation.get_bool();
	for (int i = 0; i < model->get_num_bones(); i++) {
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		// if(!skip_scale)
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		} else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
}

void util_localspace_to_meshspace_with_physics(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies,
											   const std::vector<bool>& phys_bitmask, const MSkeleton* model) {
	for (int i = 0; i < model->get_num_bones(); i++) {
		if (phys_bitmask[i])
			continue;

		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		} else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
}
void util_localspace_to_meshspace_ptr(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* model) {
	for (int i = 0; i < model->get_num_bones(); i++) {
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		} else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
	// for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}
