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
	Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
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
	Debug::add_sphere(org, radius, COLOR_GREEN,-1.f,false);
}


// source = reference pose - source
void util_subtract(int bonecount, const Pose& reference, Pose& source)
{
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.scale[i] = source.scale[i] - reference.scale[i];

		source.q[i] = quat_delta(reference.q[i],source.q[i]);

	}

}

// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor)
{
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
		b.scale[i] = glm::mix(b.scale[i], a.scale[i], factor);
	}
}
void util_blend_with_mask(int bonecount, const Pose& a, Pose& b, float factor, const std::vector<float>& mask)
{
	ASSERT(mask.size() >= bonecount);
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(a.q[i], b.q[i], factor*mask[i]);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(a.pos[i], b.pos[i], factor*mask[i]);
		b.scale[i] = glm::mix(a.scale[i], b.scale[i], factor * mask[i]);

	}
}


static glm::quat quat_blend_additive(const glm::quat& a, const glm::quat& b, float t)
{
	glm::quat target = b * a;
	return glm::slerp(a, target, t);
}


#include "Framework/Config.h"

static glm::quat forward_dir = glm::quat(0.0,0,0,1);
static bool face_foward = true;
static bool apply_to_local = false;
static bool apply_to_all = false;
#include "imgui.h"
#include "GameEnginePublic.h"
void menu1235()
{
	if(ImGui::Begin("abc")) {
		ImGui::SliderFloat4("dir", &forward_dir.x,-1,1);
		ImGui::Checkbox("ff", &face_foward);
		ImGui::Checkbox("apply_to_local", &apply_to_local);
		ImGui::Checkbox("apply_to_all", &apply_to_all);

		forward_dir = glm::normalize(forward_dir);
	} ImGui::End();

	glm::mat4 transform = glm::mat4_cast(forward_dir);
	Debug::add_line(vec3(0.f), transform[0], COLOR_RED, -1.f, false);
	Debug::add_line(vec3(0.f), transform[1], COLOR_GREEN, -1.f, false);
	Debug::add_line(vec3(0.f), transform[2], COLOR_BLUE, -1.f, false);


}

static AddToDebugMenu aeasdf("meun", menu1235);


static void draw_skeleton_util_debug(const glm::mat4* bones,const MSkeleton* model, int count, float line_len, const glm::mat4& transform)
{

	for (int index = 0; index < count; index++) {
		vec3 org = transform * bones[index][3];
		Color32 colors[] = { COLOR_RED,COLOR_GREEN,COLOR_BLUE };
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

void util_meshspace_to_localspace(const glm::mat4* mesh, const MSkeleton* mod, Pose* out)
{
	const int count = mod->get_num_bones();
	for (int i = 0; i < count; i++) {

		int parent = mod->get_bone_parent(i);

		glm::mat4 matrix = mesh[i];
		if (parent != -1) {
			matrix =  glm::inverse(mesh[parent])*matrix;
		}

		out->pos[i] = matrix[3];
		out->q[i] = glm::quat_cast(matrix);
		out->scale[i] = glm::length(matrix[0]);
	}
}

void util_localspace_to_meshspace_ptr_2(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* skel)
{
	for (int i = 0; i < skel->get_num_bones(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (skel->get_bone_parent(i) == -1) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(skel->get_bone_parent(i) < skel->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[skel->get_bone_parent(i)] * matrix;
		}
	}
	//for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}




void util_global_blend(const MSkeleton* skel, const Pose* a,  Pose* b, float factor, const std::vector<float>& mask)
{

	const int bonecount = skel->get_num_bones();

	std::vector<glm::mat4> globalspace_base(bonecount);

	//for (int i = 0; i < bonecount; i++)
	//{
	//	glm::mat4x4 matrix = glm::mat4_cast(glm::slerp(a->q[i],b->q[i],mask[i]));
	//	matrix[3] = glm::vec4(glm::mix(a->pos[i],b->pos[i],mask[i]),1.f);
	//
	//	if (bone_vec[i].parent == -1) {
	//		globalspace_base[i] = matrix;
	//	}
	//	else {
	//		//assert(model->bones[i].parent < model->bones.size());
	//		globalspace_base[i] = globalspace_base[bone_vec[i].parent] * matrix;
	//	}
	//}

	util_localspace_to_meshspace_ptr_2(*a, globalspace_base.data(), skel);


	std::vector<glm::mat4> globalspace_layer(bonecount);
	util_localspace_to_meshspace_ptr_2(*b, globalspace_layer.data(), skel);

	std::vector<glm::quat> globalspace_rotations(bonecount);

	for (int j = 0; j < bonecount; j++) {
		glm::quat base = glm::quat_cast(globalspace_base[j]);
		glm::quat layer = glm::quat_cast(globalspace_layer[j]);
		glm::quat global = glm::slerp(base, layer, mask[j] * factor);	// meshspace 
		globalspace_rotations[j] = global;
		//glm::vec4 t = globalspace_layer[j][3];
		//globalspace_base[j] = glm::mat4_cast(layer);
		//globalspace_base[j][3] = t;
#if 1
		int parent = skel->get_bone_parent(j);
		if (parent == -1) {
			b->q[j]=global;
		}
		else {
			//glm::mat3 matrix = glm::mat3_cast(global);
			//glm::mat3 parent_mat = glm::mat3_cast(globalspace_rotations[parent]);
			//matrix = glm::inverse(parent_mat) * matrix;
			//b->q[j] = glm::quat_cast(matrix);
			b->q[j] = glm::inverse(globalspace_rotations[parent]) * global;
		}

		b->pos[j] = a->pos[j];
		b->scale[j] = a->scale[j];
#endif
	}
}

// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac)
{
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.scale[i] = glm::mix(base.scale[i], base.scale[i] + additive.scale[i], fac);


		glm::quat target = additive.q[i] * base.q[i];

		base.q[i] = glm::slerp(base.q[i], target, fac);
		//base.q[i] = glm::normalize(base.q[i]);
	}
}

void util_twobone_ik(const vec3& a, const vec3& b, const vec3& c, const vec3& target, const vec3& pole_vector, const glm::quat& a_global_rotation, const glm::quat& b_global_rotation, glm::quat& a_local_rotation, glm::quat& b_local_rotation)
{
	float eps = 0.01;
	float len_ab = length(b - a);
	float len_cb = length(b - c);
	float len_at = glm::clamp(length(target - a), eps, len_ab + len_cb - eps);

	// Interior angles of a and b
	float a_interior_angle = acos(dot(normalize(c - a), normalize(b - a)));
	float b_interior_angle = acos(dot(normalize(a - b), normalize(c - b)));
	vec3 c_a = c - a;
	vec3 target_a = normalize(target - a);
	float dot_c_a_t_a = dot(normalize(c_a), target_a);
	float c_interior_angle = acos(glm::clamp(dot_c_a_t_a, -0.9999999f, 0.9999999f));

	// Law of cosines to get the desired angles of the triangle
	float a_desired_angle = acos(LawOfCosines(len_cb, len_ab, len_at));
	float b_desired_angle = acos(LawOfCosines(len_at, len_ab, len_cb));

	// Axis to rotate around
	vec3 d = b_global_rotation * pole_vector;
	//vec3 axis0 =   normalize(cross(c - a, d));
	vec3 axis0 = normalize(cross(c - a, b - a));
	vec3 t_a = target - a;
	vec3 cross_c_a_ta = cross(c_a, t_a);
	vec3 axis1 = normalize(cross_c_a_ta);
	glm::quat rot0 = glm::angleAxis(a_desired_angle - a_interior_angle, glm::inverse(a_global_rotation) * axis0);
	glm::quat rot1 = glm::angleAxis(b_desired_angle - b_interior_angle, glm::inverse(b_global_rotation) * axis0);
	glm::quat rot2 = glm::angleAxis(c_interior_angle, glm::inverse(a_global_rotation) * axis1);

	a_local_rotation = a_local_rotation * (rot0 * rot2);
	b_local_rotation = b_local_rotation * rot1;
}

// y2 = blend( blend(x1,x2,fac.x), blend(y1,y2,fac.x), fac.y)
void util_bilinear_blend(int bonecount, const Pose& x1, Pose& x2, const Pose& y1, Pose& y2, glm::vec2 fac)
{
	util_blend(bonecount, x1, x2, fac.x);
	util_blend(bonecount, y1, y2, fac.x);
	util_blend(bonecount, x2, y2, fac.y);
}


#define IS_HIGH_BIT_SET(x) (x&(1u<<31u))
#define UNSET_HIGH_BIT(x) (x & ~(1u<<31u))

glm::vec3* AnimationSeq::get_pos_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);
	if (IS_HIGH_BIT_SET(offset.pos)) {
		if (keyframe > 0) return nullptr;
		return (glm::vec3*)(pose_data.data() + actual_ofs);
	}
	else {
		return ((glm::vec3*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}
float* AnimationSeq::get_scale_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
	if (IS_HIGH_BIT_SET(offset.scale)) {
		if (keyframe > 0) return nullptr;
		return (float*)(pose_data.data() + actual_ofs);
	}
	else {
		return ((float*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}

glm::quat* AnimationSeq::get_quat_write_ptr(int channel, int keyframe) {

	ChannelOffset offset = channel_offsets[channel];
	uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
	if (IS_HIGH_BIT_SET(offset.rot)) {
		if (keyframe > 0) return nullptr;
		return (glm::quat*)(pose_data.data() + actual_ofs);
	}
	else {
		return ((glm::quat*)(pose_data.data() + actual_ofs)) + keyframe;
	}
}


ScalePositionRot AnimationSeq::get_keyframe(int bone, int keyframe, float lerpamt) const
{
	// TODO: insert return statement here
	ChannelOffset offset = channel_offsets[bone];
	
	ScalePositionRot output;

	if (IS_HIGH_BIT_SET(offset.pos)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);
		output.pos = *(glm::vec3*)(pose_data.data() + actual_ofs);
	}
	else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.pos);

		glm::vec3* ptr = (glm::vec3*)(pose_data.data() + actual_ofs);

		glm::vec3 p0 = ptr[keyframe];
		glm::vec3 p1 = ptr[keyframe+1];

		output.pos = glm::mix(p0, p1, lerpamt);
	}

	if (IS_HIGH_BIT_SET(offset.rot)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
		output.rot = *(glm::quat*)(pose_data.data() + actual_ofs);
	}
	else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.rot);
		glm::quat* ptr = (glm::quat*)(pose_data.data() + actual_ofs);

		glm::quat r0 = ptr[keyframe];
		glm::quat r1 = ptr[keyframe + 1];

		output.rot = glm::slerp(r0, r1, lerpamt);
	}

	if (IS_HIGH_BIT_SET(offset.scale)) {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
		output.scale = *(float*)(pose_data.data() + actual_ofs);
	}
	else {
		uint32_t actual_ofs = UNSET_HIGH_BIT(offset.scale);
		float* ptr = (float*)(pose_data.data() + actual_ofs);

		float s0 = ptr[keyframe];
		float s1 = ptr[keyframe + 1];

		output.scale = glm::mix(s0, s1, lerpamt);
	}

	return output;
}

const AnimationEvent* AnimationSeq::get_events_for_keyframe(int keyframe, int* count) const
{
	return nullptr;
}

#include "Animation/SkeletonData.h"
void util_calc_rotations(const MSkeleton* skeleton, const AnimationSeq* clip, float time, const BoneIndexRetargetMap* remap_indicies, Pose& outpose)
{
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

		ScalePositionRot transform = clip->get_keyframe(src_idx, keyframe, lerp_amt);
		outpose.pos[dest_idx] = transform.pos;
		outpose.q[dest_idx] = transform.rot;
		outpose.scale[dest_idx] = transform.scale;

	}
	if (remap_indicies) {
		for (int dest_idx = 0; dest_idx < count; dest_idx++) {
			int src_idx = (remap_indicies) ? (remap_indicies->my_skeleton_to_who)[dest_idx] : dest_idx;
			if (src_idx == -1)
				continue;

			auto& bone = skeleton->get_all_bones()[dest_idx];
			if (bone.retarget_type == RetargetBoneType::FromAnimation) {
				// do nothing
			}
			else if (bone.retarget_type == RetargetBoneType::FromAnimationScaled) {
				float scale = glm::length(skeleton->get_bone_local_transform(dest_idx)[3]);
				scale /= glm::length(remap_indicies->who->get_bone_local_transform(src_idx)[3]);
				outpose.pos[dest_idx] *= scale;
			}
			else if (bone.retarget_type == RetargetBoneType::FromTargetBindPose) {
				outpose.pos[dest_idx] = skeleton->get_bone_local_transform(dest_idx)[3];

				//outpose.q[dest_idx] = remap_indicies->my_skelton_to_who_quat_delta.at(dest_idx) * outpose.q[dest_idx];
			}
		}
	}
}



void util_set_to_bind_pose(Pose& pose, const MSkeleton* skel)
{
	for (int i = 0; i < skel->get_num_bones(); i++) {
		pose.pos[i] = skel->get_bone_local_transform(i)[3];
		pose.q[i] = skel->get_bone_local_rotation(i);
		pose.scale[i] = 1.f;
	}
}

static const int ROOT_BONE = -1;

void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
}

void util_localspace_to_meshspace_with_physics(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies, const std::vector<bool>& phys_bitmask, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		if (phys_bitmask[i])
			continue;

		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}

}
void util_localspace_to_meshspace_ptr(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* model)
{
	for (int i = 0; i < model->get_num_bones(); i++)
	{
		glm::mat4x4 matrix = glm::mat4_cast(local.q[i]);
		matrix[3] = glm::vec4(local.pos[i], 1.0);
		matrix = glm::scale(matrix, glm::vec3(local.scale[i]));

		if (model->get_bone_parent(i) == ROOT_BONE) {
			out_bone_matricies[i] = matrix;
		}
		else {
			assert(model->get_bone_parent(i) < model->get_num_bones());
			out_bone_matricies[i] = out_bone_matricies[model->get_bone_parent(i)] * matrix;
		}
	}
	//for (int i = 0; i < model->bones.size(); i++)
	//	out_bone_matricies[i] =  out_bone_matricies[i];
}
