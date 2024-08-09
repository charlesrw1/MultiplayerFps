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

// Wrapper around PxActor
class Model;
class Entity;
class EntityComponent;
class PhysicsFilterPresetBase;
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

	// releases any actor associated with this
	void free();

	bool is_allocated() const {
		return actor;
	}

	// creation functions, if an actor already exists, will warn and delete it, you should explicitly call free()
	// creates physics from the models collision bodies, falls back to an AABB
	void create_static_actor_from_model(const Model* model, PhysTransform transform, PhysicsShapeType type = PhysicsShapeType::SimulateAndQuery);
	void create_dynamic_actor_from_model(const Model* model);
	// create from primitives
	void create_static_actor_from_shape(const physics_shape_def& shape, PhysicsShapeType type = PhysicsShapeType::SimulateAndQuery);
	void create_dynamic_actor_from_shape(const physics_shape_def& shape, PhysicsShapeType type = PhysicsShapeType::SimulateAndQuery);

	// set how the actor's shape should simulate, all shapes are the same type
	// ex: trigger only, query only, query and simulate...
	void set_all_shapes_as_type(PhysicsShapeType type);

	// wether actor was created with dynamic/static
	bool is_dynamic() const;
	bool is_static() const;

	// valid for dynamic actors only
	glm::vec3 get_linear_velocity() const;
	void apply_impulse(glm::vec3 worldspace, glm::vec3 impulse);
	void set_objects_mass(float mass);
	void set_objects_density(float density);
	// kinematic actors are controlled by setting position/rotation manually
	// use for doors, platforms
	bool set_kinematic_flag(bool is_kinematic);
	// set if actor should be simulating their shape, only for dynamic actors
	bool set_simulate(bool simulating);

	// valid for both dynamic and static
	PhysTransform get_transform() const;

	// disables the object from everyting (queries/simulate/triggers)
	bool disable_physics();

	// querying/setting who owns this shape
	void set_entity(EntityComponent* e) {
		owner = e;
	}
	EntityComponent* get_entity_owner() {
		return owner;
	}
private:
	physx::PxRigidActor* get_actor() const { return actor; }
	physx::PxRigidDynamic* get_dynamic_actor() const {
		assert(actor&&is_dynamic());
		return (physx::PxRigidDynamic*)actor;
	}
	const PhysicsFilterPresetBase* presetMask = nullptr;
	EntityComponent* owner = nullptr;
	physx::PxRigidActor* actor = nullptr;
	int ragDollBoneIndex = -1;	// if not -1, then this PxActor is part of a ragdoll and the index is the bone in the component
};

// Constraint wrapper for gameplay
class PhysicsConstraint
{
public:
	EntityComponent* owner = nullptr;
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

	std::vector<PhysicsActor*> bones;
	std::vector<PhysicsConstraint*> constraints;

	Model* skeleton = nullptr;
	Entity* owner = nullptr;
};

struct world_query_result
{
	float fraction = 1.0;
	glm::vec3 hit_pos;
	glm::vec3 hit_normal;
	glm::vec3 trace_dir;
	PhysicsActor* actor = nullptr;
	uint16_t contents=0;
	uint32_t face_hit = 0;
	int16_t bone_hit = -1;
	bool had_initial_overlap = false;
	float distance = 0.0;
};
class BinaryReader;
class PhysicsManPublic
{
public:
	virtual void init() = 0;

	virtual bool trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& end, uint32_t channel_mask) = 0;
	virtual bool trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& dir, float length, uint32_t channel_mask) = 0;
	
	virtual bool sweep_capsule(
		world_query_result& out,
		const vertical_capsule_def_t& capsule, 
		const glm::vec3& start, 
		const glm::vec3& dir, 
		float length, 
		uint32_t channel_mask) = 0;
	virtual bool sweep_sphere(
		world_query_result& out,
		float radius,
		const glm::vec3& start,
		const glm::vec3& dir,
		float length,
		uint32_t channel_mask) = 0;
	virtual bool capsule_is_overlapped(
		const vertical_capsule_def_t& capsule,
		const glm::vec3& start,
		uint32_t channel_mask) = 0;
	virtual bool sphere_is_overlapped(
		world_query_result& out,
		float radius,
		const glm::vec3& start,
		uint32_t channel_mask) = 0;
	
	virtual PhysicsActor* allocate_physics_actor() = 0;
	virtual void free_physics_actor(PhysicsActor*& actor) = 0;

	virtual PhysicsConstraint* allocate_constraint() = 0;
	virtual void free_constraint(PhysicsConstraint*& constraint) = 0;
	
	virtual void clear_scene() = 0;
	
	// simulate scene and fetch the results, thus a blocking update
	virtual void simulate_and_fetch(float dt) = 0;

	// called by renderer only, matrix/shader/etc. already set
	virtual void debug_draw_shapes() = 0;

	// used only by model loader
	virtual bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def) = 0;
};

extern PhysicsManPublic* g_physics;