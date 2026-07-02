#pragma once

#include "Animation/Runtime/Animation.h"
#include "Render/Model.h"
using glm::quat;
using glm::vec3;
using std::vector;

class Animation_Debug
{
public:
	static void set_local_to_world(glm::mat4 transform);
	static void push_line(const glm::mat4& transform_me, const glm::mat4& transform_parent, bool has_parent);
	static void push_sphere(const glm::vec3& p, float radius);
};

inline float LawOfCosines(float a, float b, float c) {
	return (a * a - b * b - c * c) / (-2 * b * c);
}

inline float MidLerp(float min, float max, float mid_val) {
	return (mid_val - min) / (max - min);
}
inline glm::quat quat_delta(const glm::quat& from, const glm::quat& to) {
	return to * glm::inverse(from);
}

struct BoneIndexRetargetMap;
class AnimationSeq;
class MSkeleton;
void util_calc_rotations(const MSkeleton* skeleton, const AnimationSeq* clip, float time,
						 const BoneIndexRetargetMap* remap_indicies, Pose& outpose,
						 bool looping = false);
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
void util_global_blend(const MSkeleton* skel, const Pose* a, Pose* b, float factor, const std::vector<float>& mask);

// pole_target: WORLD-space position the joint (elbow/knee) should bend towards, i.e.
// Unreal's "Joint Target" location. It always drives the bend plane (see .cpp).
void util_twobone_ik(const vec3& a, const vec3& b, const vec3& c, const vec3& target, const vec3& pole_target,
					 const glm::quat& a_global_rotation, const glm::quat& b_global_rotation,
					 glm::quat& a_local_rotation, glm::quat& b_local_rotation);

// Per-bone length multiplier (>=1) so a two-bone chain of rest length (len_ab+len_cb) can
// reach dist_to_target, capped at max_stretch. Returns 1 (no stretch) below start_stretch_ratio
// of the natural reach; ramps linearly from 1 to max_stretch as dist goes from
// (start_stretch_ratio * reach) to (max_stretch * reach) -- mirrors Unreal's Two Bone IK
// "Start Stretch Ratio". Starting the ramp before dist == reach (start_stretch_ratio < 1) means
// the chain is already partway stretched by the time it would otherwise hit the straight-arm
// singularity in util_twobone_ik. Apply the returned scale to both bones' local translations
// before calling util_twobone_ik so the a/b/c positions it receives already reflect the
// lengthened chain.
float util_twobone_stretch_scale(float len_ab, float len_cb, float dist_to_target, float max_stretch, float start_stretch_ratio = 1.f);

void util_set_to_bind_pose(Pose& pose, const MSkeleton* skel);

void util_localspace_to_meshspace(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies,
								  const MSkeleton* model);
void util_localspace_to_meshspace_with_physics(const Pose& local, std::vector<glm::mat4x4>& out_bone_matricies,
											   const std::vector<bool>& phys_bitmask, const MSkeleton* model);
