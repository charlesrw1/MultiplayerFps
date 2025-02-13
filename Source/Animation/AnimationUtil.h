#pragma once

#include "Animation/Runtime/Animation.h"
#include "Render/Model.h"
using glm::vec3;
using std::vector;
using glm::quat;

class Animation_Debug
{
public:
	static void set_local_to_world(glm::mat4 transform);
	static void push_line(const glm::mat4& transform_me, const glm::mat4& transform_parent, bool has_parent);
	static void push_sphere(const glm::vec3& p, float radius);
};


inline float LawOfCosines(float a, float b, float c)
{
	return (a * a - b * b - c * c) / (-2 * b * c);
}

inline float MidLerp(float min, float max, float mid_val)
{
	return (mid_val - min) / (max - min);
}
inline glm::quat quat_delta(const glm::quat& from, const glm::quat& to)
{
	return to * glm::inverse(from);
}


struct BoneIndexRetargetMap;
class AnimationSeq;
class MSkeleton;
void util_calc_rotations(const MSkeleton* skeleton, const AnimationSeq* clip, float time, const BoneIndexRetargetMap* remap_indicies, Pose& outpose);
void util_meshspace_to_localspace(const glm::mat4* mesh, const MSkeleton* mod, Pose* out);
void util_localspace_to_meshspace_ptr_2(const Pose& local, glm::mat4* out_bone_matricies, const MSkeleton* skel);
void util_localspace_to_meshspace_ptr(const Pose& local, glm::mat4* out_bone_matricies, const Model* model);
void util_subtract(int bonecount, const Pose& reference, Pose& source);
// b = lerp(a,b,f)
void util_blend(int bonecount, const Pose& a, Pose& b, float factor);
// base = lerp(base,base+additive,f)
void util_add(int bonecount, const Pose& additive, Pose& base, float fac);

// b = lerp(a,b,f)
void util_blend_with_mask(int bonecount, const Pose& a, Pose& b, float factor, const std::vector<float>& mask);

// a = base pose
// b = layered pose
// returns output in b
void util_global_blend(const MSkeleton* skel,const Pose* a,  Pose* b, float factor,const std::vector<float>& mask);

void util_twobone_ik(
	const vec3& a, const vec3& b, const vec3& c,
	const vec3& target, const vec3& pole_vector,
	const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
	glm::quat& a_local_rotation, glm::quat& b_local_rotation);


// y2 = blend( blend(x1,x2,fac.x), blend(y1,y2,fac.x), fac.y)
void util_bilinear_blend(int bonecount, const Pose& x1, Pose& x2, const Pose& y1, Pose& y2, glm::vec2 fac);

void util_set_to_bind_pose(Pose& pose, const MSkeleton* skel);


