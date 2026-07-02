#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <vector>
#include "Framework/StringName.h"

class Pose;
class MSkeleton;

// Tuning knobs for one spring bone, modeled as a damped harmonic oscillator (F = -k*x - c*v)
// on each of three orthogonal axes relative to the bone's animated rest direction:
//   along  - the bone's own rest direction (parent -> animated tip)
//   yaw/pitch - the two axes perpendicular to it
// Small damping relative to stiffness gives an underdamped, jiggly response; damping close to
// (or above) critical (c ~= 2*sqrt(k)) gives a subtle, quickly-settling response.
struct SpringBoneParams
{
	float yaw_stiffness = 100.f;
	float yaw_damping = 8.f;
	float pitch_stiffness = 100.f;
	float pitch_damping = 8.f;
	// Only used if allow_length_flex is true; otherwise the bone stays a rigid distance
	// from its parent (position-constrained back to rest length every update).
	float along_stiffness = 100.f;
	float along_damping = 8.f;
	bool allow_length_flex = false;

	float gravity = 0.f; // acceleration magnitude along gravityDir, world space
	glm::vec3 gravityDir = glm::vec3(0.f, -1.f, 0.f);
};

// One spring-simulated bone. Chains are just bones whose boneParent is also
// a spring bone; SpringBones sorts by bone index so parents always update
// before children (skeleton bone arrays are stored parent-before-child).
struct SpringBoneDef
{
	StringName boneName;
	int boneIndex = -1;
	int parentIndex = -1;
	SpringBoneParams params;
};

// Per-instance simulation state for a SpringBoneDef, parallel array. Kept in WORLD space (not
// mesh space) so that the owning entity's translation and rotation between frames shows up as
// an implicit force on the bone -- the rest position/direction the spring chases moves with the
// entity, so a sudden stop/turn snaps the bone same as real inertia would.
struct SpringBoneState
{
	glm::vec3 currentPos = glm::vec3(0.f); // actual simulated tip position
	glm::vec3 velocity = glm::vec3(0.f);	// actual simulated tip velocity
	bool initialized = false;
};

// Owned directly by AnimatorObject. Runs after the anim graph has produced a
// local-space pose and cached_bonemats has been filled with the fully
// animated mesh-space chain; overwrites cached_bonemats entries for bones
// that were registered as spring bones.
class SpringBones
{
public:
	// boneName must exist on skel and not be the skeleton root; logs and no-ops otherwise
	// (bone names are often data-driven/editor-authored and can transiently be invalid).
	// Call any time after the AnimatorObject owning this has a valid skeleton.
	void add_spring_bone(const MSkeleton* skel, StringName boneName, const SpringBoneParams& params);

	// ownerWorldTransform: the owning entity's current world transform (identity if none),
	// used to simulate in world space -- see SpringBoneState comment.
	void update(float dt, const Pose& pose, std::vector<glm::mat4>& cached_bonemats, const MSkeleton* skel,
				const glm::mat4& ownerWorldTransform);

private:
	std::vector<SpringBoneDef> defs;
	std::vector<SpringBoneState> states;
	bool needsSort = false;
};
