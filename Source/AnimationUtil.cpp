#include "AnimationUtil.h"

void util_subtract(int bonecount, const Pose& reference, Pose& source)
{
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.q[i] = source.q[i] - reference.q[i];
	}

}

// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor)
{
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
	}
}

// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac)
{
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.q[i] = glm::slerp(base.q[i], base.q[i] + additive.q[i], fac);
		base.q[i] = glm::normalize(base.q[i]);
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

void util_calc_rotations(const Animation_Set* set, float curframe, int clip_index, const Model* model, const std::vector<int>* remap_indicies, Pose& pose)
{
	for (int dest_idx = 0; dest_idx < set->num_channels; dest_idx++) {
		int src_idx = (remap_indicies) ? (*remap_indicies)[dest_idx] : dest_idx;

		if (src_idx == -1) {
			pose.pos[dest_idx] = model->bones.at(dest_idx).localtransform[3];
			pose.q[dest_idx] = model->bones.at(dest_idx).rot;
			pose.q[dest_idx] = glm::normalize(pose.q[dest_idx]);
			continue;
		}


		int pos_idx = set->FirstPositionKeyframe(curframe, src_idx, clip_index);
		int rot_idx = set->FirstRotationKeyframe(curframe, src_idx, clip_index);

		vec3 interp_pos{};
		if (pos_idx == -1)
			interp_pos = model->bones.at(src_idx).localtransform[3];
		else if (pos_idx == set->GetChannel(clip_index, src_idx).num_positions - 1)
			interp_pos = set->GetPos(src_idx, pos_idx, clip_index).val;
		else {
			int index0 = pos_idx;
			int index1 = pos_idx + 1;
			float t0 = set->GetPos(src_idx, index0, clip_index).time;
			float t1 = set->GetPos(src_idx, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_pos = glm::mix(set->GetPos(src_idx, index0, clip_index).val, set->GetPos(src_idx, index1, clip_index).val, scale);
		}

		glm::quat interp_rot{};
		if (rot_idx == -1) {
			interp_rot = model->bones.at(src_idx).rot;
		}
		else if (rot_idx == set->GetChannel(clip_index, src_idx).num_rotations - 1)
			interp_rot = set->GetRot(src_idx, rot_idx, clip_index).val;
		else {
			int index0 = rot_idx;
			int index1 = rot_idx + 1;
			float t0 = set->GetRot(src_idx, index0, clip_index).time;
			float t1 = set->GetRot(src_idx, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_rot = glm::slerp(set->GetRot(src_idx, index0, clip_index).val, set->GetRot(src_idx, index1, clip_index).val, scale);
		}
		interp_rot = glm::normalize(interp_rot);

		pose.q[dest_idx] = interp_rot;
		pose.pos[dest_idx] = interp_pos;
	}
}

void util_set_to_bind_pose(Pose& pose, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++) {
		pose.pos[i] = model->bones.at(i).localtransform[3];
		pose.q[i] = model->bones.at(i).rot;
	}
}