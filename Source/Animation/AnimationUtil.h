#pragma once

#include "Animation/Runtime/Animation.h"
#include "Model.h"
using glm::vec3;
using std::vector;
using glm::quat;


inline float LawOfCosines(float a, float b, float c)
{
	return (a * a - b * b - c * c) / (-2 * b * c);
}

inline float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}


void util_subtract(int bonecount, const Pose& reference, Pose& source);
// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor);
// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac);


void util_twobone_ik(
	const vec3& a, const vec3& b, const vec3& c,
	const vec3& target, const vec3& pole_vector,
	const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
	glm::quat& a_local_rotation, glm::quat& b_local_rotation);


// y2 = blend( blend(x1,x2,fac.x), blend(y1,y2,fac.x), fac.y)
void util_bilinear_blend(int bonecount, const Pose& x1, Pose& x2, const Pose& y1, Pose& y2, glm::vec2 fac);

void util_calc_rotations(const Animation_Set* set,
	float curframe, int clip_index, const Model* model, const std::vector<int>* remap_indicies, Pose& pose);
void util_set_to_bind_pose(Pose& pose, const Model* model);

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




