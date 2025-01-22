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

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Physics2.h"
#include "Framework/MeshBuilder.h"
#include "Render/RenderObj.h"

#include <memory>

struct CollisionResponse
{
	uint32_t blockMask = 0;	// mask of objects to block
	uint32_t overlapMask = 0; // mask of objects to overlap with 
	uint8_t type = 0;	// 0-32 value to set what "type" the object is
	uint8_t preset = 0;
	bool generateHitEvent : 1;
	bool generateOverlapEvent : 1;
};
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
	return physx::PxQuat(v.x, v.y, v.z,v.w);
}

inline physx::PxTransform PhysTransform::get_physx() const
{
	return physx::PxTransform(glm_to_physx(position), glm_to_physx(rotation));
}
#include <glm/gtc/type_ptr.hpp>
inline PxTransform glm_to_physx(const glm::mat4& mI)
{
	return PxTransform(PxMat44(
		glm_to_physx(mI[0]),
		glm_to_physx(mI[1]),
		glm_to_physx(mI[2]),
		glm_to_physx(mI[3])
	));
}

class MyPhysicsCallback;
class PhysicsManImpl
{
public:
	PhysicsManImpl();
	~PhysicsManImpl();


	bool sweep_shared(world_query_result& out,
		physx::PxGeometry& geom,
		const glm::vec3& start,
		const glm::vec3& dir,
		float length,
		const TraceIgnoreVec* ignored_components = nullptr,
		uint32_t collision_channel_mask = UINT32_MAX);

	bool overlap_shared(
		physx::PxGeometry& geom,
		const glm::vec3& start,
		uint32_t mask)
	{
		physx::PxOverlapBuffer overlap;
		physx::PxTransform local(glm_to_physx(start));
		bool status = scene->overlap(geom, local, overlap);
		return status;
	}

	physx::PxScene* get_physx_scene() { return scene; }

	void init();
	void simulate_and_fetch(float dt);

	// used only by model loader
	bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def);


	physx::PxMaterial* default_material = nullptr;
	physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
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