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
	bool has_initialized() const {
		return actor;
	}

	void init_physics_shape(
		const PhysicsFilterPresetBase* filter,	// the filter to apply
		const glm::mat4& initalTransform,			// starting transform
		bool isSimulating,						// if this is a simulating shape 
		bool sendOverlap,	// if overlap events will be sent to entity component
		bool sendHit,		// if hit events will be sent to EC
		bool isStatic,	// if this should create a static actor, not compatible with isSimulating
		bool isTrigger,	// if true, then this shape is not solid but a trigger
		bool startDisabled
	);

	void add_model_shape_to_actor(const Model* m);
	void add_sphere_shape_to_actor(const glm::vec3& pos, float radius);
	void add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius);
	void add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents);
	void update_mass();

	// wether actor was created with isStatic
	bool is_static() const;

	// set if actor should be simulating their shape, only for dynamic actors (else its kinematic for non-static physics objs)
	void set_simulate(bool simulating);
	void set_enabled(bool enabled);
	void set_set_my_transform(bool setMyTransform) {
		this->setMyTransformWhenSimulating = setMyTransform;
	}

	// valid for dynamic actors only
	glm::vec3 get_linear_velocity() const;
	void set_linear_velocity(const glm::vec3& v);
	void apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse);
	void set_objects_mass(float mass);
	void set_objects_density(float density);
	void set_transform(const glm::mat4& transform, bool teleport = false);

	glm::mat4 get_transform() const;


	// querying/setting who owns this shape
	EntityComponent* get_entity_owner() {
		return owner;
	}

	void set_filter_preset(const PhysicsFilterPresetBase* filter);
private:

	void set_shape_flags(physx::PxShape* shape);

	void free();
	void set_entity(EntityComponent* e) {
		owner = e;
	}
	physx::PxRigidActor* get_actor() const { return actor; }
	physx::PxRigidDynamic* get_dynamic_actor() const {
		assert(actor&&!is_static());
		return (physx::PxRigidDynamic*)actor;
	}
	const PhysicsFilterPresetBase* presetMask = nullptr;
	EntityComponent* owner = nullptr;
	physx::PxRigidActor* actor = nullptr;
	char ragDollBoneIndex = -1;	// if not -1, then this PxActor is part of a ragdoll and the index is the bone in the component
	bool isSimulating = false;
	bool disabled = false;
	bool sendHitEvents = false;
	bool sendOverlapEvents = false;
	bool setMyTransformWhenSimulating = true;
	bool isTrigger = false;
	bool isStatic = false;
	friend class PhysicsManLocal;
};

// Constraint wrapper for gameplay
class PhysicsConstraint
{
public:
	EntityComponent* owner = nullptr;
	physx::PxConstraint* constraint = nullptr;
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
	
	virtual PhysicsActor* allocate_physics_actor(EntityComponent* ecOwner) = 0;
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