#pragma once

#include "Framework/Handle.h"
#include "Framework/FreeList.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include <physx/PxActor.h>
#include <physx/PxPhysics.h>
#include <physx/geometry/PxBoxGeometry.h>
#include <physx/geometry/PxSphereGeometry.h>
#include <physx/geometry/PxCapsuleGeometry.h>
#include <physx/PxScene.h>

#include "Framework/InlineVec.h"

#include "Framework/StringName.h"
#include <physx/PxRigidActor.h>
#include <physx/PxRigidDynamic.h>
#include <physx/extensions/PxRigidBodyExt.h>
// physics system

enum class ShapeType_e : uint8_t
{
	None,
	Sphere,
	Box,
	Capsule,
	ConvexShape,
};

enum class ShapeUseage_e : uint8_t
{
	Collision,	// Simulate always
	Ragdoll,	// Simualte when ragdolled
	Hitbox,		// Scene queryable
};

enum class PhysicsConstraintType_e : uint8_t
{
	Distance,
	Hinge,
	BallSocket,
	D6Joint,
};


struct sphere_def_t {
	float radius;
};
struct box_def_t {
	glm::vec3 halfsize;
};
struct vertical_capsule_def_t {
	float radius;
	float half_height;
};

struct physics_shape_def
{
	ShapeType_e shape  = ShapeType_e::None;
	union {
		sphere_def_t sph;
		box_def_t box;
		vertical_capsule_def_t cap;
		// cooked physx data
		physx::PxConvexMesh* convex_mesh;
	};
	glm::mat4 transform = glm::mat4(1.0);
};

class PhysicsBodyConstraintDef
{
public:
	int parent_body = -1;
	int child_body = -1;
	PhysicsConstraintType_e type = PhysicsConstraintType_e::Hinge;
	glm::vec3 limit_min = glm::vec3(0.0);
	glm::vec3 limit_max = glm::vec3(1.0);
	float damping = 0.0;
};
struct PSubBodyDef
{
	StringName group_name;
	uint32_t dont_collide_with_mask = 0;
	uint16_t shape_start = 0;
	uint16_t shape_count = 0;
	int16_t skeletal_mesh_bone = -1;
	ShapeUseage_e usage = ShapeUseage_e::Collision;
};
// Rigid body definition
class PhysicsBody
{
public:
	physx::PxTriangleMesh* trimesh_shape = nullptr;
	bool is_skeleton = false;
	std::vector<physics_shape_def> shapes;
	std::vector<PSubBodyDef> subbodies;
	std::vector<PhysicsBodyConstraintDef> constraints;
};

// A physics actor in the scene, wraps physx PxActor


inline glm::vec3 physx_to_glm(const physx::PxVec3& v) {
	return glm::vec3(v.x, v.y, v.z);
}
inline glm::quat physx_to_glm(const physx::PxQuat& v) {
	return glm::quat(v.w, v.x, v.y, v.z);
}
inline physx::PxVec3 glm_to_physx(const glm::vec3& v) {
	return physx::PxVec3(v.x, v.y, v.z);
}
struct PhysTransform
{
	PhysTransform(const physx::PxTransform& t) :
		position(physx_to_glm(t.p)), rotation(physx_to_glm(t.q)) {}
	glm::vec3 position;
	glm::quat rotation;
};


class Entity;
class PhysicsActor
{
public:
	PhysicsActor() = default;
	~PhysicsActor() {
		free();
	}
	PhysicsActor(const PhysicsActor& other) = delete;
	PhysicsActor& operator=(PhysicsActor& other) = delete;
	PhysicsActor(PhysicsActor&& other);

	void create_actor_from_def(const PhysicsBody& def, const PSubBodyDef& subbody);
	void create_trimesh_actor(physx::PxTriangleMesh* mesh);
	void create_actor_from_shape(const physics_shape_def& shape);

	void free();

	bool is_active() const {
		return actor != nullptr;
	}

	bool is_dynamic() const {
		assert(actor);
		return actor->getType() == physx::PxActorType::eRIGID_DYNAMIC;
	}
	bool is_static() const {
		assert(actor);
		return actor->getType() == physx::PxActorType::eRIGID_STATIC;
	}
	physx::PxRigidActor* get_actor() const { return actor; }


	glm::vec3 get_linear_velocity() const {
		return physx_to_glm(get_dynamic_actor()->getLinearVelocity());
	}
	void apply_impulse(glm::vec3 worldspace, glm::vec3 impulse) {
		physx::PxRigidBodyExt::addForceAtPos(
			*get_dynamic_actor(),
			glm_to_physx(impulse),
			glm_to_physx(worldspace),
			physx::PxForceMode::eIMPULSE);
	}
	PhysTransform get_transform() const {
		return actor->getGlobalPose();
	}
	void set_transform() {
		
	}

private:
	physx::PxRigidDynamic* get_dynamic_actor() const {
		assert(actor&&is_dynamic());
		return (physx::PxRigidDynamic*)actor;
	}
	uint16_t flags = 0;
	Entity* owner = nullptr;
	int bone_index = 0;
	physx::PxRigidActor* actor = nullptr;
};

// Constraint wrapper for gameplay
class PhysicsConstraintComponent
{
public:
	physx::PxConstraint* constraint = nullptr;
};

class Model;
class SkeletonPhysicsActor
{
public:
	void set_group_as_simulating(StringName group, bool simulate);
	void set_ragdoll_simulating(bool simulate);
	bool set_jiggle_simulating(bool simulate);

	void update();

	std::vector<PhysicsActor> bones;
	std::vector<PhysicsConstraintComponent> constraints;

	Model* skeleton = nullptr;
	Entity* owner = nullptr;
};

class PhysicsManLocal
{
public:
	// Shape traces
	void trace_ray();
	void box_cast();
	void sphere_cast();
	void capsule_cast();
	void simulate();

	physx::PxScene* get_global_scene() {
		return scene;
	}
private:
	physx::PxScene* scene = nullptr;
};