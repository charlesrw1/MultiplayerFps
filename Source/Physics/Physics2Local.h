#pragma once
#include <physx/PxRigidActor.h>
#include <physx/PxRigidDynamic.h>
#include <physx/extensions/PxRigidBodyExt.h>
#include <physx/geometry/PxBoxGeometry.h>
#include <physx/geometry/PxSphereGeometry.h>
#include <physx/geometry/PxCapsuleGeometry.h>
#include <physx/PxScene.h>

#include "Framework/BinaryReadWrite.h"

#include <physx/foundation/PxFoundation.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxActor.h>
#include <physx/PxScene.h>
#include <physx/PxPhysics.h>
#include <physx/characterkinematic/PxController.h>
#include <physx/foundation/PxPhysicsVersion.h>
#include <physx/common/PxTolerancesScale.h>
#include <physx/foundation/PxTransform.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Physics2.h"
#include "Framework/MeshBuilder.h"
#include "Render/RenderObj.h"

#include <memory>
#include <array>

using namespace physx;

inline glm::vec3 physx_to_glm(const physx::PxVec3& v) {
	return glm::vec3(v.x, v.y, v.z);
}
inline glm::quat physx_to_glm(const physx::PxQuat& v) {
	return glm::quat(v.w, v.x, v.y, v.z);
}
inline physx::PxVec3 glm_to_physx(const glm::vec3& v) {
	return physx::PxVec3(v.x, v.y, v.z);
}
inline physx::PxQuat glm_to_physx(const glm::quat& v) {
	return physx::PxQuat(v.x, v.y, v.z, v.w);
}

#include <glm/gtc/type_ptr.hpp>
inline PxTransform glm_to_physx(const glm::mat4& mI) {
	return PxTransform(PxMat44(glm_to_physx(mI[0]), glm_to_physx(mI[1]), glm_to_physx(mI[2]), glm_to_physx(mI[3])));
}

class MyPhysicsCallback;
class PhysicsManImpl
{
public:
	PhysicsManImpl();
	~PhysicsManImpl();
	void set_physics_layer_collisions(std::span<const bool> triangular_matrix);

	// Max number of physics layers, bounded by the 32 bits of a PxFilterData word.
	static const int MAX_PHYSICS_LAYERS = 32;
	// Per-layer collision masks. layer_collision_masks[i] has bit j set if layer i
	// collides with layer j. Built by set_physics_layer_collisions.
	std::array<uint32_t, MAX_PHYSICS_LAYERS> layer_collision_masks;

	bool sweep_shared(world_query_result& out, physx::PxGeometry& geom, const glm::vec3& start, const glm::vec3& dir,
					  float length, const TraceIgnoreVec* ignored_components = nullptr,
					  uint32_t collision_channel_mask = UINT32_MAX);

	bool overlap_shared(overlap_query_result& out, physx::PxGeometry& geom, const glm::vec3& start, uint32_t mask);

	physx::PxScene* get_physx_scene() { return scene; }

	void init();
	void simulate_and_fetch(float dt);

	// used only by model loader
	bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def);

	physx::PxMaterial* default_material = nullptr;
	physx::PxCpuDispatcher* dispatcher = nullptr;
	physx::PxDefaultErrorCallback err;
	physx::PxDefaultAllocator alloc;
	physx::PxScene* scene = nullptr;
	physx::PxPhysics* physics_factory = nullptr;
	physx::PxFoundation* foundation = nullptr;

	std::unique_ptr<MyPhysicsCallback> mycallback;

	MeshBuilder debug_mesh;
	handle<MeshBuilder_Object> debug_mesh_handle;
	void update_debug_physics_shapes();
};

extern PhysicsManImpl* physics_local_impl;