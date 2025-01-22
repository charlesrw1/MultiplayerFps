#pragma once

#include "Framework/Handle.h"
#include "Framework/FreeList.h"
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/InlineVec.h"
#include "Framework/StringName.h"
#include "Framework/FreeList.h"
// physics system
#include "physx/foundation/PxTransform.h"
#include "Framework/Util.h"
#include "Framework/Config.h"


namespace physx
{

	class PxRigidActor;
	class PxRigidDynamic;
	class PxConstraint;
	class PxTriangleMesh;
	class PxScene;
	class PxConvexMesh;
	class PxActor;
	class PxShape;
}

enum class ShapeType_e : uint8_t
{
	None,
	Sphere,
	Box,
	Capsule,
	ConvexShape,
	MeshShape,
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
		physx::PxTriangleMesh* tri_mesh;
	};
	glm::vec3 local_p = glm::vec3(0.0);
	glm::quat local_q = glm::quat(1, 0, 0, 0);

	static physics_shape_def create_sphere(glm::vec3 center, float radius) {
		physics_shape_def shape;
		shape.shape = ShapeType_e::Sphere;
		shape.sph.radius = radius;
		shape.local_p = center;
		return shape;
	}
	static physics_shape_def create_box(glm::vec3 halfsize, glm::vec3 pos, glm::quat rot = glm::quat(1, 0, 0, 0)) {
		physics_shape_def shape;
		shape.shape = ShapeType_e::Box;
		shape.box.halfsize = halfsize;
		shape.local_p = pos;
		shape.local_q = rot;
		return shape;
	}

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
	StringName group_name;					// StringName associated with this, for example hitbox name
	uint32_t dont_collide_with_mask = 0;	// mask of other subbodies to not collide with (for skeleton meshes)
	uint16_t shape_start = 0;
	uint16_t shape_count = 0;
	int16_t skeletal_mesh_bone = -1;		// associated bone in model's skeleton
	ShapeUseage_e usage = ShapeUseage_e::Collision;
};

// Rigid body definition
class PhysicsBody
{
public:
	bool can_use_for_rigid_body_dynamic() const {
		return can_be_dynamic;
	}
	bool has_skeleton_physics() const {
		return is_skeleton;
	}

	bool can_be_dynamic = false;	// set to true if triangle mesh shape not used
	std::vector<physics_shape_def> shapes;
	uint32_t num_shapes_of_main = 0;	// these are the main collider shapes

	// subbodies that are used by skeletons for ragdolls/hitboxes
	bool is_skeleton = false;
	std::vector<PSubBodyDef> subbodies;
	std::vector<PhysicsBodyConstraintDef> constraints;
};

struct PhysTransform
{
	PhysTransform(const physx::PxTransform& t);
	PhysTransform() = default;
	physx::PxTransform get_physx() const;
	glm::vec3 position=glm::vec3(0.0);
	glm::quat rotation=glm::quat(0,0,0,0);
};

enum class PhysicsShapeType
{
	SimulateAndQuery,	// most physical things
	TriggerAndQuery,	// rare usage
	SimulateOnly,		// simulating things that dont get involved with raycasts/character movement
	QueryOnly,			// for things like hitboxes
	TriggerOnly,		// most common for invisible triggers
};


class PhysicsComponentBase;
struct world_query_result
{
	float fraction = 1.0;
	glm::vec3 hit_pos;
	glm::vec3 hit_normal;
	glm::vec3 trace_dir;
	PhysicsComponentBase* component = nullptr;
	uint16_t contents=0;
	uint32_t face_hit = 0;
	int16_t bone_hit = -1;
	bool had_initial_overlap = false;
	float distance = 0.0;
};

class PhysicsComponentBase;
using TraceIgnoreVec = InlineVec<PhysicsComponentBase*, 4>;

class BinaryReader;
class PhysicsManImpl;
class PhysicsManager
{
public:
	void init();

	bool trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& end, const TraceIgnoreVec* ignore, uint32_t channel_mask);
	bool trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& dir, float length, const TraceIgnoreVec* ignore, uint32_t channel_mask);
	
	bool sweep_capsule(
		world_query_result& out,
		const vertical_capsule_def_t& capsule, 
		const glm::vec3& start, 
		const glm::vec3& dir, 
		float length, 
		uint32_t channel_mask = UINT32_MAX,
		const TraceIgnoreVec* ignore = nullptr
	);
	bool sweep_sphere(
		world_query_result& out,
		float radius,
		const glm::vec3& start,
		const glm::vec3& dir,
		float length,
		uint32_t channel_mask = UINT32_MAX,
		const TraceIgnoreVec* ignore = nullptr
	);
	bool capsule_is_overlapped(
		const vertical_capsule_def_t& capsule,
		const glm::vec3& start,
		uint32_t channel_mask);
	bool sphere_is_overlapped(
		world_query_result& out,
		float radius,
		const glm::vec3& start,
		uint32_t channel_mask);
	
	// simulate scene and fetch the results, thus a blocking update
	void simulate_and_fetch(float dt);

	// used only by model loader
	bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def);

private:
	PhysicsManImpl* impl=nullptr;
};

extern PhysicsManager g_physics;