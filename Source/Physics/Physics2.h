#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"
#include "Framework/InlineVec.h"
#include <vector>
#include "Framework/ClassBase.h"

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
	class PxMaterial;
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

// Rigid body definition
class PhysicsBodyDefinition
{
public:
	~PhysicsBodyDefinition();
	PhysicsBodyDefinition() = default;
	PhysicsBodyDefinition& operator=(const PhysicsBodyDefinition& other) = delete;
	PhysicsBodyDefinition(const PhysicsBodyDefinition& other) = delete;
	void uninstall_shapes();

	std::vector<physics_shape_def> shapes;
};


class PhysicsBody;
struct world_query_result
{
	float fraction = 1.0;
	glm::vec3 hit_pos;
	glm::vec3 hit_normal;
	glm::vec3 trace_dir;
	PhysicsBody* component = nullptr;
	uint16_t contents=0;
	uint32_t face_hit = 0;
	int16_t bone_hit = -1;
	bool had_initial_overlap = false;
	float distance = 0.0;
};
struct overlap_query_result {
	InlineVec<PhysicsBody*, 8> overlaps;
};

// never delete this!
// might change this later. create with standard new/malloc.... :(
// can set this to: MaterialInstances, Models, PhysicsBody components. will choose material based on that order. (PhysicsBody overrides Model, etc.)
// otherwise chooses the default material
class PhysicsMaterialWrapper : public ClassBase {
public:
	CLASS_BODY(PhysicsMaterialWrapper);
	PhysicsMaterialWrapper();
#pragma warning(push)
#pragma warning(disable : 4722) // "destructor never returns" warning. cause assert calls std::abort
	~PhysicsMaterialWrapper() {
		ASSERT(!"Dont delete physics materials!");
	}
#pragma warning(pop)

	REF void set_friction(float static_f,float dynamic_f);
	REF void set_restitution(float r);
	physx::PxMaterial* material = nullptr;
};


using TraceIgnoreVec = InlineVec<PhysicsBody*, 4>;

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
		overlap_query_result& out,
		const vertical_capsule_def_t& capsule,
		const glm::vec3& start,
		uint32_t channel_mask);
	bool sphere_is_overlapped(
		overlap_query_result& out,
		float radius,
		const glm::vec3& start,
		uint32_t channel_mask);
	
	// simulate scene and fetch the results, thus a blocking update
	void simulate_and_fetch(float dt);

	// used only by model loader
	bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def);

	// debug mesh
	void sync_render_data();

private:
	PhysicsManImpl* impl=nullptr;
};

extern PhysicsManager g_physics;