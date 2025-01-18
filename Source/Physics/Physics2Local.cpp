#include "Physics2.h"
#include "Physics2Local.h"
#include "Render/RenderObj.h"
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

#include "physx/extensions/PxDefaultStreams.h"

#define COOKING

#include "physx/cooking/PxCooking.h"
#include "Framework/Util.h"

#include <physx/PxFiltering.h>
#include <physx/PxScene.h>
#include "Framework/Hashset.h"

// for debug drawing
#include "Framework/MeshBuilder.h"

#include "Render/Model.h"


#include "Render/DrawPublic.h"

#include "Game/Components/PhysicsComponents.h"
#include "Game/Entity.h"

#define WARN_ONCE(a,...) { \
	static bool has_warned = false; \
	if (!has_warned) { \
		sys_print(a, __VA_ARGS__);\
		has_warned = true;\
	} }


ConfigVar g_draw_physx_scene("g_draw_physx_scene", "0", CVAR_DEV | CVAR_BOOL, "draw the physx debug scene");


PhysicsManImpl* physics_local_impl = nullptr;
PhysicsManager g_physics;

void PhysicsManager::init()
{
	impl = new PhysicsManImpl();
	physics_local_impl = impl;
	impl->init();
}

bool PhysicsManager::sweep_capsule(
	world_query_result& out,
	const vertical_capsule_def_t& capsule,
	const glm::vec3& start,
	const glm::vec3& dir,
	float length,
	uint32_t mask)
{
	auto geom = physx::PxCapsuleGeometry(capsule.radius, capsule.half_height);
	return impl->sweep_shared(out, geom, start, dir, length, mask);
}
bool PhysicsManager::sweep_sphere(
	world_query_result& out,
	float radius,
	const glm::vec3& start,
	const glm::vec3& dir,
	float length,
	uint32_t mask) {
	auto geom = physx::PxSphereGeometry(radius);
	return impl->sweep_shared(out, geom, start, dir, length, mask);
}
bool PhysicsManager::capsule_is_overlapped(
	const vertical_capsule_def_t& capsule,
	const glm::vec3& start,
	uint32_t mask) {
	auto geom = physx::PxCapsuleGeometry(capsule.radius, capsule.half_height);
	return impl->overlap_shared(geom, start, mask);

}
bool PhysicsManager::sphere_is_overlapped(
	world_query_result& out,
	float radius,
	const glm::vec3& start,
	uint32_t mask) {
	auto geom = physx::PxSphereGeometry(radius);
	return impl->overlap_shared(geom, start, mask);
}
void PhysicsManager::simulate_and_fetch(float dt)
{
	impl->simulate_and_fetch(dt);
}

bool PhysicsManager::load_physics_into_shape(BinaryReader& reader, physics_shape_def& def) {
	return impl->load_physics_into_shape(reader, def);
}

bool PhysicsManager::trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& end, uint32_t mask) {
	float length = glm::length(end - start);
	glm::vec3 dir = (end - start) / length;
	return trace_ray(out, start, dir, length, mask);
}
bool PhysicsManager::trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& dir, float length, uint32_t mask) {
	physx::PxRaycastBuffer hit;
	bool status = impl->scene->raycast(
		glm_to_physx(start), glm_to_physx(dir), length, hit);
	sys_print(Debug,"ray: %d\n", (int)status);
	if (!status) {
		out.fraction = 1.0;
		return status;
	}
	out.fraction = hit.block.distance / length;
	out.component = (PhysicsComponentBase*)hit.block.actor->userData;
	out.hit_pos = physx_to_glm(hit.block.position);
	out.hit_normal = physx_to_glm(hit.block.normal);
	out.trace_dir = dir;
	out.distance = hit.block.distance;
	return status;
}


PhysTransform::PhysTransform(const physx::PxTransform& t) :
	position(physx_to_glm(t.p)), rotation(physx_to_glm(t.q)) {}



using namespace physx;





static vec3 randColor(uint32_t number) {
	return fract(sin(vec3(number + 1) * vec3(12.8787, 1.97, 20.73739)));
}
static Color32 randcolor32(uint32_t number)
{
	glm::vec3 v = randColor(number);
	Color32 c32;
	c32.r = (v.x * 255.0);
	c32.g = (v.y * 255.0);
	c32.b = (v.z * 255.0);
	c32.a = 255;
	return c32;
}
#include "Render/RenderObj.h"
#include "Physics2Local.h"

 bool PhysicsManImpl::sweep_shared(world_query_result& out, physx::PxGeometry& geom, const glm::vec3& start, const glm::vec3& dir, float length, uint32_t mask)
{
	physx::PxSweepBuffer sweep;
	PxTransform relativePose(glm_to_physx(start), PxQuat(PxHalfPi, PxVec3(0, 0, 1)));
	physx::PxTransform local(relativePose);
	bool status = scene->sweep(geom, local, glm_to_physx(dir), length, sweep, PxHitFlag::eDEFAULT | PxHitFlag::eMTD /* for initial overlaps */);
	if (!status) {
		out.fraction = 1.0;
		return status;
	}
	out.had_initial_overlap = sweep.block.hadInitialOverlap();
	out.distance = sweep.block.distance;
	out.fraction = sweep.block.distance / length;
	out.component = (PhysicsComponentBase*)sweep.block.actor->userData;
	out.hit_pos = physx_to_glm(sweep.block.position);

	out.hit_normal = physx_to_glm(sweep.block.normal);
	out.trace_dir = dir;
	return status;
}

  void PhysicsManImpl::init() {
	 sys_print(Info, "Initializing Physics\n");

	 foundation = PxCreateFoundation(PX_PHYSICS_VERSION, alloc, err);

	 physics_factory = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), true);

	 physx::PxSceneDesc sceneDesc(physics_factory->getTolerancesScale());
	 sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	 sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
	 dispatcher = physx::PxDefaultCpuDispatcherCreate(2);

	 if (!dispatcher)
		 Fatalf("PxDefaultCpuDispatcherCreate failed!");

	 sceneDesc.cpuDispatcher = dispatcher;
	 scene = physics_factory->createScene(sceneDesc);

	 default_material = physics_factory->createMaterial(0.5f, 0.5f, 0.1f);

	 scene->setFlag(PxSceneFlag::eENABLE_ACTIVE_ACTORS, true);
	 scene->setFlag(PxSceneFlag::eEXCLUDE_KINEMATICS_FROM_ACTIVE_ACTORS, true);
 }

  void PhysicsManImpl::simulate_and_fetch(float dt) {
	  CPUFUNCTIONSTART;

	  scene->simulate(dt);
	  scene->fetchResults(true/* block */);

	  // retrieve array of actors that moved
	  PxU32 nbActiveTransforms;
	  auto activeTransforms = scene->getActiveActors(nbActiveTransforms);

	  // update each render object with the new transform
	  for (PxU32 i = 0; i < nbActiveTransforms; ++i)
	  {
		  auto phys_comp = (PhysicsComponentBase*)activeTransforms[i]->userData;
		  if (phys_comp) {
			  phys_comp->get_owner()->set_ws_transform(phys_comp->get_transform());
		  }
	  }

	  update_debug_physics_shapes();
  }

  // used only by model loader

   bool PhysicsManImpl::load_physics_into_shape(BinaryReader& reader, physics_shape_def& def) {
	  if (def.shape == ShapeType_e::ConvexShape) {
		  uint32_t count = reader.read_int32();
		  uint8_t* data = new uint8_t[count];
		  reader.read_bytes_ptr(data, count);
		  physx::PxDefaultMemoryInputData inp(data, count);

		  def.convex_mesh = physics_factory->createConvexMesh(inp);
	  }

	  return true;
  }

void PhysicsManImpl::update_debug_physics_shapes()
{
	ASSERT(scene);

	if (g_draw_physx_scene.get_integer() == 0) {
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 0.0);
		if (debug_mesh_handle.is_valid())
			idraw->get_scene()->remove_meshbuilder(debug_mesh_handle);
		return;
	}
	MeshBuilder_Object o;
	using namespace physx;
	static bool init = false;
	if (!init) {
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 2.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_EDGES, 1.0);
		init = true;
	}
	debug_mesh.Begin();

	auto& rb = scene->getRenderBuffer();
	for (PxU32 i = 0; i < rb.getNbLines(); i++)
	{
		const PxDebugLine& line = rb.getLines()[i];
		// render the line
		debug_mesh.PushLine(physx_to_glm(line.pos0), physx_to_glm(line.pos1), *((Color32*)&line.color0));
	}
	debug_mesh.End();
	debug_mesh.Draw(MeshBuilder::LINES);

	o.visible = true;
	o.transform = glm::mat4(1.f);
	o.meshbuilder = &debug_mesh;

	if (!debug_mesh_handle.is_valid())
		debug_mesh_handle = idraw->get_scene()->register_meshbuilder(o);
	else
		idraw->get_scene()->update_meshbuilder(debug_mesh_handle, o);
}