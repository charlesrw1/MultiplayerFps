#pragma once

#include "Animation.h"
#include "Model.h"
using glm::vec3;
using std::vector;
using glm::quat;


static float LawOfCosines(float a, float b, float c)
{
	return (a * a - b * b - c * c) / (-2 * b * c);
}

static float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}


static void util_subtract(int bonecount, const Pose& reference, Pose& source)
{
	for (int i = 0; i < bonecount; i++) {
		source.pos[i] = source.pos[i] - reference.pos[i];
		source.q[i] = source.q[i] - reference.q[i];
	}

}
// b = lerp(a,b,f)
static void util_blend(int bonecount, const Pose& a, Pose& b, float factor)
{
	for (int i = 0; i < bonecount; i++) {
		b.q[i] = glm::slerp(b.q[i], a.q[i], factor);
		b.q[i] = glm::normalize(b.q[i]);
		b.pos[i] = glm::mix(b.pos[i], a.pos[i], factor);
	}
}
// base = lerp(base,base+additive,f)
static void util_add(int bonecount, const Pose& additive, Pose& base, float fac)
{
	for (int i = 0; i < bonecount; i++) {
		base.pos[i] = glm::mix(base.pos[i], base.pos[i] + additive.pos[i], fac);
		base.q[i] = glm::slerp(base.q[i], base.q[i] + additive.q[i], fac);
		base.q[i] = glm::normalize(base.q[i]);
	}
}


static void util_twobone_ik(
	const vec3& a, const vec3& b, const vec3& c,
	const vec3& target, const vec3& pole_vector,
	const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
	glm::quat& a_local_rotation, glm::quat& b_local_rotation)
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
static void util_bilinear_blend(int bonecount, const Pose& x1, Pose& x2, const Pose& y1, Pose& y2, glm::vec2 fac)
{
	util_blend(bonecount, x1, x2, fac.x);
	util_blend(bonecount, y1, y2, fac.x);
	util_blend(bonecount, x2, y2, fac.y);
}

static float modulo_lerp(float start, float end, float mod, float alpha)
{
	float d1 = glm::abs(end - start);
	float d2 = mod - d1;


	if (d1 <= d2)
		return glm::mix(start, end, alpha);
	else {
		if (start >= end)
			return fmod(start + (alpha * d2), mod);
		else
			return fmod(end + ((1 - alpha) * d2), mod);
	}
}

static void util_calc_rotations(const Animation_Set* set,
	float curframe, int clip_index, const Model* model, Pose& pose)
{
	for (int i = 0; i < set->num_channels; i++) {
		int pos_idx = set->FirstPositionKeyframe(curframe, i, clip_index);
		int rot_idx = set->FirstRotationKeyframe(curframe, i, clip_index);

		vec3 interp_pos{};
		if (pos_idx == -1)
			interp_pos = model->bones.at(i).posematrix[3];
		else if (pos_idx == set->GetChannel(clip_index, i).num_positions - 1)
			interp_pos = set->GetPos(i, pos_idx, clip_index).val;
		else {
			int index0 = pos_idx;
			int index1 = pos_idx + 1;
			float t0 = set->GetPos(i, index0, clip_index).time;
			float t1 = set->GetPos(i, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_pos = glm::mix(set->GetPos(i, index0, clip_index).val, set->GetPos(i, index1, clip_index).val, scale);
		}

		glm::quat interp_rot{};
		if (rot_idx == -1) {
			interp_rot = model->bones.at(i).rot;
		}
		else if (rot_idx == set->GetChannel(clip_index, i).num_rotations - 1)
			interp_rot = set->GetRot(i, rot_idx, clip_index).val;
		else {
			int index0 = rot_idx;
			int index1 = rot_idx + 1;
			float t0 = set->GetRot(i, index0, clip_index).time;
			float t1 = set->GetRot(i, index1, clip_index).time;
			if (index0 == 0)t0 = 0.f;
			//float scale = MidLerp(clip.GetPos(i, index0).time, clip.GetPos(i, index1).time, curframe);
			//interp_pos = glm::mix(clip.GetPos(i, index0).val, clip.GetPos(i, index1).val, scale);
			float scale = MidLerp(t0, t1, curframe);
			assert(scale >= 0 && scale <= 1.f);
			interp_rot = glm::slerp(set->GetRot(i, index0, clip_index).val, set->GetRot(i, index1, clip_index).val, scale);
		}
		interp_rot = glm::normalize(interp_rot);

		pose.q[i] = interp_rot;
		pose.pos[i] = interp_pos;
	}
}
static void util_set_to_bind_pose(Pose& pose, const Model* model)
{
	for (int i = 0; i < model->bones.size(); i++) {
		pose.pos[i] = model->bones.at(i).localtransform[3];
		pose.q[i] = model->bones.at(i).rot;
	}
}

class Pose_Pool
{
public:
	Pose_Pool(int n) : poses(n) {}
	static Pose_Pool& get() {
		static Pose_Pool inst(64);
		return inst;
	}

	vector<Pose> poses;
	int head = 0;
	Pose* alloc(int count) {
		assert(count + head < 64);
		head += count;
		return &poses[head - count];
	}
	void free(int count) {
		head -= count;
		assert(head >= 0);
	}
};

class Matrix_Pool
{
public:
	Matrix_Pool(int n) : matricies(n) {}
	static Matrix_Pool& get() {
		static Matrix_Pool inst(256 * 2);
		return inst;
	}
	vector<glm::mat4> matricies;
	int head = 0;
	glm::mat4* alloc(int count) {
		assert(count + head < matricies.size());
		head += count;
		return &matricies[head - count];
	}
	void free(int count) {
		head -= count;
		assert(head >= 0);
	}
};




