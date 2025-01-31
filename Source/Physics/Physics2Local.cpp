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

#include "Framework/Config.h"
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
	uint32_t channel_mask,
	const TraceIgnoreVec* ignore)
{
	auto geom = physx::PxCapsuleGeometry(capsule.radius, capsule.half_height);
	return impl->sweep_shared(out, geom, start, dir, length, ignore, channel_mask);
}
bool PhysicsManager::sweep_sphere(
	world_query_result& out,
	float radius,
	const glm::vec3& start,
	const glm::vec3& dir,
	float length,
	uint32_t channel_mask,
	const TraceIgnoreVec* ignore) {
	auto geom = physx::PxSphereGeometry(radius);
	return impl->sweep_shared(out, geom, start, dir, length,ignore, channel_mask);
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

#include "Render/RenderObj.h"
#include "Physics2Local.h"
#include <unordered_set>
class MyPhysicsQueryFilter : public physx::PxQueryFilterCallback
{
public:
	MyPhysicsQueryFilter(const TraceIgnoreVec* vec) : ignored(vec) {}

	// Inherited via PxQueryFilterCallback
	virtual PxQueryHitType::Enum preFilter(const PxFilterData& filterData, const PxShape* shape, const PxRigidActor* actor, PxHitFlags& queryFlags) override
	{
		PhysicsComponentBase* ptr =(PhysicsComponentBase*)actor->userData;
		for (int i = 0; i < ignored->size(); i++)
			if (ptr == (*ignored)[i])
				return PxQueryHitType::eNONE;
		return PxQueryHitType::eBLOCK;
	}
	virtual PxQueryHitType::Enum postFilter(const PxFilterData& filterData, const PxQueryHit& hit, const PxShape* shape, const PxRigidActor* actor) override
	{
		return PxQueryHitType::eBLOCK;
	}

	const TraceIgnoreVec* ignored = nullptr;
};

bool PhysicsManager::trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& end, const TraceIgnoreVec* ignore, uint32_t mask) {
	float length = glm::length(end - start);
	glm::vec3 dir = (end - start) / length;
	return trace_ray(out, start, dir, length, ignore, mask);
}
bool PhysicsManager::trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& dir, float length,const TraceIgnoreVec* ignore, uint32_t channel_mask) {
	physx::PxRaycastBuffer hit;

	MyPhysicsQueryFilter query_filter(ignore);
	PxQueryFilterData filter;
	filter.data.word0 = channel_mask;
	if(ignore)
		filter.flags |= PxQueryFlag::ePREFILTER;

	bool status = impl->scene->raycast(
		glm_to_physx(start), glm_to_physx(dir), length, hit, PxHitFlag::eDEFAULT,
		filter,
		(ignore) ? &query_filter : nullptr);


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


 bool PhysicsManImpl::sweep_shared(world_query_result& out, physx::PxGeometry& geom, const glm::vec3& start, const glm::vec3& dir, float length, const TraceIgnoreVec* ignore, uint32_t channel_mask)
{
	physx::PxSweepBuffer sweep;
	PxTransform relativePose(glm_to_physx(start), PxQuat(PxHalfPi, PxVec3(0, 0, 1)));
	physx::PxTransform local(relativePose);

	MyPhysicsQueryFilter query_filter(ignore);
	PxQueryFilterData filter;
	filter.data.word0 = channel_mask;
	if(ignore)
		filter.flags |= PxQueryFlag::ePREFILTER;

	bool status = scene->sweep(geom, local, glm_to_physx(dir), length, sweep, PxHitFlag::eDEFAULT | PxHitFlag::eMTD /* for initial overlaps */,
		filter,
		(ignore) ? &query_filter : nullptr);

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

 class MyPhysicsCallback : public physx::PxSimulationEventCallback
 {
 public:
	 // Inherited via PxSimulationEventCallback
	 virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override
	 {
	 }
	 virtual void onWake(PxActor** actors, PxU32 count) override
	 {
	 }
	 virtual void onSleep(PxActor** actors, PxU32 count) override
	 {
	 }
	 virtual void onContact(const PxContactPairHeader& pairHeader, const PxContactPair* pairs, PxU32 nbPairs) override
	 {
		 sys_print(Debug, "contact\n");
	 }
	 virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) override
	 {
		 for (int i = 0; i < (int)count; i++) {
			 auto& pair = pairs[i];
			 PhysicsComponentBase* trigger_obj = (PhysicsComponentBase*)pair.triggerActor->userData;
			 PhysicsComponentBase* other_obj = (PhysicsComponentBase*)pair.otherActor->userData;
			 if (!trigger_obj || !other_obj)
				 continue;

			 triggered_pairs.push_back({ trigger_obj,other_obj, pair.status == physx::PxPairFlag::eNOTIFY_TOUCH_FOUND });
		 }
	 }
	 virtual void onAdvance(const PxRigidBody* const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override
	 {
	 }

	 void call_all_triggered() {
		 for (auto& p : triggered_pairs) {
			if (p.is_start) {
				 //sys_print(Debug, "trigger found\n");
				 p.trigger->on_trigger_start.invoke(p.other);
			 }
			 else {
				 //sys_print(Debug, "trigger lost\n");
				  p.trigger->on_trigger_end.invoke(p.other);
			 }
		 }
		 triggered_pairs.clear();
	 }

	 struct trigger_pair {
		 PhysicsComponentBase* trigger = nullptr;
		 PhysicsComponentBase* other = nullptr;
		 bool is_start = false;
	 };
	 std::vector<trigger_pair> triggered_pairs;
 };
 PhysicsManImpl::~PhysicsManImpl()
 {

 }
 PhysicsManImpl::PhysicsManImpl()
 {

 }

 static PxFilterFlags my_filter_shader(
    PxFilterObjectAttributes attributes0, PxFilterData filterData0,
    PxFilterObjectAttributes attributes1, PxFilterData filterData1,
    PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize)
{
    // let triggers through
    if(PxFilterObjectIsTrigger(attributes0) || PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = PxPairFlag::eTRIGGER_DEFAULT;
        return PxFilterFlag::eDEFAULT;
    }
    // generate contacts for all that were not filtered above
	if ((filterData0.word0 & filterData1.word1) &&  (filterData1.word0 & filterData0.word1)) {
		pairFlags = PxPairFlag::eCONTACT_DEFAULT;
		return PxFilterFlag::eDEFAULT;
	}
	else
		return PxFilterFlag::eKILL;

}

  void PhysicsManImpl::init() {
	 sys_print(Info, "Initializing Physics\n");

	 foundation = PxCreateFoundation(PX_PHYSICS_VERSION, alloc, err);

	 physics_factory = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation, physx::PxTolerancesScale(), true);

	 physx::PxSceneDesc sceneDesc(physics_factory->getTolerancesScale());
	 sceneDesc.gravity = physx::PxVec3(0.0f, -9.81f, 0.0f);
	 sceneDesc.filterShader = my_filter_shader;// physx::PxDefaultSimulationFilterShader;
	 dispatcher = physx::PxDefaultCpuDispatcherCreate(2);


	 if (!dispatcher)
		 Fatalf("PxDefaultCpuDispatcherCreate failed!");

	 sceneDesc.cpuDispatcher = dispatcher;
	 scene = physics_factory->createScene(sceneDesc);

	 default_material = physics_factory->createMaterial(0.5f, 0.5f, 0.1f);

	scene->setFlag(PxSceneFlag::eENABLE_ACTIVE_ACTORS, true);
	 scene->setFlag(PxSceneFlag::eEXCLUDE_KINEMATICS_FROM_ACTIVE_ACTORS, true);

	 mycallback.reset(new MyPhysicsCallback);
	 scene->setSimulationEventCallback(mycallback.get());
 }

  void PhysicsManImpl::simulate_and_fetch(float dt) {
	  CPUFUNCTIONSTART;

	  scene->simulate(dt);
	  scene->fetchResults(true/* block */);

	  mycallback->call_all_triggered();

	  // retrieve array of actors that moved
	  PxU32 nbActiveTransforms;
	  auto activeTransforms = scene->getActiveActors(nbActiveTransforms);

	  // update each render object with the new transform
	  for (PxU32 i = 0; i < nbActiveTransforms; ++i)
	  {
		  auto phys_comp = (PhysicsComponentBase*)activeTransforms[i]->userData;
		  if (phys_comp) {
			  phys_comp->fetch_new_transform();
		  }
	  }

	  update_debug_physics_shapes();
  }


PhysicsBody::~PhysicsBody()
{
	for (auto& s : shapes) {
		if (s.shape == ShapeType_e::ConvexShape) { 
			if (s.convex_mesh)			  
				s.convex_mesh->release(); 
		}
		else if (s.shape == ShapeType_e::MeshShape) { 
			if (s.tri_mesh)			  
				s.tri_mesh->release(); 
		}
	}
}
bool PhysicsManImpl::load_physics_into_shape(BinaryReader& reader, physics_shape_def& def) {
	if (def.shape != ShapeType_e::ConvexShape && def.shape != ShapeType_e::MeshShape)	return true;
	uint32_t count = reader.read_int32();
	uint8_t* data = new uint8_t[count];
	reader.read_bytes_ptr(data, count);
	physx::PxDefaultMemoryInputData inp(data, count);

	if (def.shape == ShapeType_e::ConvexShape) {
		def.convex_mesh = physics_factory->createConvexMesh(inp);
	}
	else if (def.shape == ShapeType_e::MeshShape) {
		def.tri_mesh = physics_factory->createTriangleMesh(inp);
	}
	delete[] data;

	return true;
}

void PhysicsManImpl::update_debug_physics_shapes()
{
	ASSERT(scene);
	return;
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
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 1.0);
		//scene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL, 1.0);
		//scene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0);
		//scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_EDGES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LOCAL_FRAMES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eJOINT_LIMITS, 0.4);



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