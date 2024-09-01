#include "Physics2.h"
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

#include "Game/EntityComponent.h"

#define WARN_ONCE(a,...) { \
	static bool has_warned = false; \
	if (!has_warned) { \
		sys_print(a, __VA_ARGS__);\
		has_warned = true;\
	} }


ConfigVar g_draw_physx_scene("g_draw_physx_scene", "0", CVAR_DEV | CVAR_BOOL, "draw the physx debug scene");



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

physx::PxTransform PhysTransform::get_physx() const
{
	return physx::PxTransform(glm_to_physx(position), glm_to_physx(rotation));
}


class PhysicsManImpl
{
public:
	PhysicsManImpl() : awake_dynamic_actors(3), all_physics_actors(16) {}


	
	bool sweep_shared(world_query_result& out,
		physx::PxGeometry& geom,
		const glm::vec3& start,
		const glm::vec3& dir,
		float length,
		uint32_t mask)
	{
		physx::PxSweepBuffer sweep;
		PxTransform relativePose(glm_to_physx(start),PxQuat(PxHalfPi, PxVec3(0, 0, 1)));
		physx::PxTransform local(relativePose);
		bool status = scene->sweep(geom, local, glm_to_physx(dir), length, sweep, PxHitFlag::eDEFAULT | PxHitFlag::eMTD /* for initial overlaps */);
		if (!status) {
			out.fraction = 1.0;
			return status;
		}
		out.had_initial_overlap = sweep.block.hadInitialOverlap();
		out.distance = sweep.block.distance;
		out.fraction = sweep.block.distance / length;
		out.actor = (PhysicsActor*)sweep.block.actor->userData;
		out.hit_pos = physx_to_glm(sweep.block.position);

		out.hit_normal = physx_to_glm(sweep.block.normal);
		out.trace_dir = dir;
		return status;
	}
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


	
	PhysicsConstraint* allocate_constraint() {
		return new PhysicsConstraint;
	}
	void free_constraint(PhysicsConstraint*& constraint) {
		delete constraint;
		constraint = nullptr;
	}

	PhysicsActor* allocate_physics_actor(EntityComponent* ecOwner) {
		auto a = new PhysicsActor();
		a->set_entity(ecOwner);
		all_physics_actors.insert(a);
		return a;
	}
	void free_physics_actor(PhysicsActor*& actor) {
		all_physics_actors.remove(actor);
		awake_dynamic_actors.remove(actor);

		delete actor;
		actor = nullptr;
	}

	physx::PxScene* get_physx_scene() { return scene; }

	void init() {
		sys_print("Initializing Physics\n");

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
	void simulate_and_fetch(float dt) {
		CPUFUNCTIONSTART;

		scene->simulate(dt);
		scene->fetchResults(true/* block */);

		// retrieve array of actors that moved
		PxU32 nbActiveTransforms;
		auto activeTransforms = scene->getActiveActors(nbActiveTransforms);

		// update each render object with the new transform
		for (PxU32 i = 0; i < nbActiveTransforms; ++i)
		{
			auto physActor = (PhysicsActor*)activeTransforms[i]->userData;
			if (physActor) {
				auto ec = physActor->get_entity_owner();
				if (ec)
					ec->set_ws_transform(physActor->get_transform());
			}
		}
	}

	// used only by model loader
	bool load_physics_into_shape(BinaryReader& reader, physics_shape_def& def) {
		if (def.shape == ShapeType_e::ConvexShape) {
			uint32_t count = reader.read_int32();
			uint8_t* data = new uint8_t[count];
			reader.read_bytes_ptr(data, count);
			physx::PxDefaultMemoryInputData inp(data, count);

			def.convex_mesh = physics_factory->createConvexMesh(inp);
		}

		return true;
	}

	physx::PxMaterial* default_material = nullptr;
	physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
	physx::PxDefaultErrorCallback err;
	physx::PxDefaultAllocator alloc;
	physx::PxScene* scene = nullptr;
	physx::PxPhysics* physics_factory = nullptr;
	physx::PxFoundation* foundation = nullptr;

	MeshBuilder debug_mesh;


	hash_set<PhysicsActor> all_physics_actors;
	hash_set<PhysicsActor> awake_dynamic_actors;	// issues callbacks for these
};

PhysicsManager g_physics;
static PhysicsManImpl* local_impl = nullptr;

void PhysicsManager::init()
{
	impl = new PhysicsManImpl();
	local_impl = impl;
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
PhysicsActor* PhysicsManager::allocate_physics_actor(EntityComponent* ecOwner)
{
	return impl->allocate_physics_actor(ecOwner);
}
void PhysicsManager::free_physics_actor(PhysicsActor*& actor)
{
	impl->free_physics_actor(actor);
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
	sys_print("ray: %d\n", (int)status);
	if (!status) {
		out.fraction = 1.0;
		return status;
	}
	out.fraction = hit.block.distance / length;
	out.actor = (PhysicsActor*)hit.block.actor->userData;
	out.hit_pos = physx_to_glm(hit.block.position);
	out.hit_normal = physx_to_glm(hit.block.normal);
	out.trace_dir = dir;
	out.distance = hit.block.distance;
	return status;
}


PhysTransform::PhysTransform(const physx::PxTransform& t) :
	position(physx_to_glm(t.p)), rotation(physx_to_glm(t.q)) {}

void PhysicsActor::apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse)
{
	physx::PxRigidBodyExt::addForceAtPos(
		*get_dynamic_actor(),
		glm_to_physx(impulse),
		glm_to_physx(worldspace),
		physx::PxForceMode::eIMPULSE);
}
glm::mat4 PhysicsActor::get_transform() const
{
	auto pose = actor->getGlobalPose();
	auto mat = glm::translate(glm::mat4(1), physx_to_glm(pose.p));
	mat = mat *  glm::mat4_cast(physx_to_glm(pose.q));
	return mat;
}


bool PhysicsActor::is_static() const {
	assert(actor);
	return actor->getType() == physx::PxActorType::eRIGID_STATIC;
}

glm::vec3 PhysicsActor::get_linear_velocity() const {
	return physx_to_glm(get_dynamic_actor()->getLinearVelocity());
}

void PhysicsActor::free()
{
	if (has_initialized()) {
		local_impl->scene->removeActor(*(physx::PxActor*)actor);
		actor->release();
		actor = nullptr;
	}
}

using namespace physx;
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
void PhysicsActor::set_shape_flags(PxShape* shape)
{
	if (isTrigger)
		shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eVISUALIZATION);
	else
		shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eVISUALIZATION);
}
void PhysicsActor::add_model_shape_to_actor(const Model* model)
{
	if (disabled)
		return;
	if (model->get_physics_body()) {
		auto body = model->get_physics_body();
		for (int i = 0; i < body->num_shapes_of_main; i++) {
			auto& shape = body->shapes[i];
			if (shape.shape != ShapeType_e::ConvexShape)
				continue;

			PxShape* aConvexShape = PxRigidActorExt::createExclusiveShape(*actor,
				PxConvexMeshGeometry(shape.convex_mesh), *local_impl->default_material);
			set_shape_flags(aConvexShape);
		}
	}
	else {
		auto aabb = model->get_bounds();
		auto boxGeom = PxBoxGeometry(glm_to_physx((aabb.bmax - aabb.bmin) * 0.5f));

		auto shape = PxRigidActorExt::createExclusiveShape(*actor,
			boxGeom, *local_impl->default_material);

		auto middle = (aabb.bmax + aabb.bmin) * 0.5f;

		shape->setLocalPose(PxTransform(glm_to_physx(middle)));

		set_shape_flags(shape);
	}
}
void PhysicsActor::add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius)
{
	if (disabled)
		return;
	auto capGeom = PxCapsuleGeometry(radius, height * 0.5);
	auto shape = PxRigidActorExt::createExclusiveShape(*actor,
		capGeom, *local_impl->default_material);

	glm::vec3 targetCenter = base + glm::vec3(0.f, height * 0.5f, 0.f);

	shape->setLocalPose(PxTransform(glm_to_physx(targetCenter)));
	set_shape_flags(shape);
}
void PhysicsActor::add_sphere_shape_to_actor(const glm::vec3& pos, float radius)
{
	if (disabled)
		return;
	auto boxGeom = PxSphereGeometry(radius);
	auto shape = PxRigidActorExt::createExclusiveShape(*actor,
		boxGeom, *local_impl->default_material);
	shape->setLocalPose(PxTransform(glm_to_physx(pos)));
	set_shape_flags(shape);
}
void PhysicsActor::add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents)
{
	if (disabled)
		return;

	auto boxGeom = PxBoxGeometry(glm_to_physx(halfExtents));
	auto shape = PxRigidActorExt::createExclusiveShape(*actor,
		boxGeom, *local_impl->default_material);
	shape->setLocalPose(glm_to_physx(localTransform));
	set_shape_flags(shape);
}

void PhysicsActor::init_physics_shape(
	const PhysicsFilterPresetBase* filter,	// the filter to apply
	const glm::mat4& initalTransform,			// starting transform
	bool isSimulating,						// if this is a simulating shape 
	bool sendOverlap,	// if overlap events will be sent to entity component
	bool sendHit,		// if hit events will be sent to EC
	bool isStatic,	// if this should create a static actor, not compatible with isSimulating
	bool isTrigger,
	bool startDisabled
)
{
	if (has_initialized()) {
		sys_print("??? physics actor wasn't freed before call to init_physics_shape()\n");
		free();
	}
	this->disabled = startDisabled;
	this->presetMask = filter;
	this->isSimulating = isSimulating;
	this->sendHitEvents = sendHit;
	this->sendOverlapEvents = sendOverlap;
	this->isTrigger = isTrigger;
	this->isStatic = isStatic;


	auto factory = local_impl->physics_factory;
	if (isStatic) {
		auto t = glm_to_physx(initalTransform);
		t.q.normalize();

		actor = factory->createRigidStatic(t);
	}
	else {
		auto t = glm_to_physx(initalTransform);
		t.q.normalize();

		actor = factory->createRigidDynamic(t);
		auto dyn = (PxRigidDynamic*)actor;
		dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !this->isSimulating);
	}

	local_impl->scene->addActor(*actor);

	actor->userData = this;

	actor->setActorFlag(physx::PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
	actor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, disabled);

}

void PhysicsActor::set_simulate(bool isSimulating)
{
	assert(actor);
	if (this->isSimulating != isSimulating) {
		this->isSimulating = isSimulating;
		if (!is_static()) {
			auto dyn = (PxRigidDynamic*)actor;
			dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !this->isSimulating);
		}
		else
			sys_print("??? set_simulating set on a static PhysicsActor\n");
	}
}
void PhysicsActor::set_enabled(bool enabled)
{
	assert(actor);
	this->disabled = enabled;
	actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, disabled);
}

void PhysicsActor::update_mass()
{
	if (!isStatic) {
		auto dyn = (PxRigidDynamic*)actor;
		PxRigidBodyExt::updateMassAndInertia(*dyn, 1.f);
	}
}
void PhysicsActor::set_transform(const glm::mat4& transform, bool teleport)
{
	if (is_static() || !teleport) {
		auto t = glm_to_physx(transform);
		t.q.normalize();
		actor->setGlobalPose(t);
		if(isSimulating)
			set_linear_velocity({});
	}
	else{
		if (isSimulating) {
			sys_print("??? set_transform on a simulating PhysicsActor\n");
		}
		auto dyn = get_dynamic_actor();
		dyn->setKinematicTarget(glm_to_physx(transform));
	}
}
void PhysicsActor::set_linear_velocity(const glm::vec3& v)
{
	if (is_static()) {
		sys_print("??? set_linear_velocity on a static PhysicsActor\n");
	}
	else {
		auto dyn = get_dynamic_actor();
		dyn->setLinearVelocity(glm_to_physx(v));
	}
}




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
#if 0
void PhysicsManLocal::debug_draw_shapes()
{
	ASSERT(scene);

	if (g_draw_physx_scene.get_integer() == 0) {
		return;
	}
	using namespace physx;


	const bool draw_dynamic = g_draw_physx_scene.get_integer() & DebugPhysxSceneFlags::Dynamic;
	const bool draw_static = g_draw_physx_scene.get_integer() & DebugPhysxSceneFlags::Static;
	const bool draw_triggers = g_draw_physx_scene.get_integer() & DebugPhysxSceneFlags::Trigger;
	const bool draw_constraints = g_draw_physx_scene.get_integer() & DebugPhysxSceneFlags::Contraints;
	const bool draw_kinematic = g_draw_physx_scene.get_integer() & DebugPhysxSceneFlags::Kinematic;

	debug_mesh.Begin();
	{
		PxActor* actor_buffer[256];

		uint32_t actor_flags = 0;
		if (draw_dynamic)
			actor_flags |= PxActorTypeFlag::eRIGID_DYNAMIC;
		if (draw_static)
			actor_flags |= PxActorTypeFlag::eRIGID_STATIC;

		uint32_t count = scene->getActors((PxActorTypeFlags)actor_flags, actor_buffer, 256, 0);

		for (uint32_t i = 0; i < count; i++) {
			PxActor* actor = actor_buffer[i];
			if (actor->getType() != PxActorType::eRIGID_DYNAMIC && actor->getType() != PxActorType::eRIGID_STATIC)
				continue;
			PxRigidActor* rigid = (PxRigidActor*)actor;
			PxTransform transform = rigid->getGlobalPose();
			PhysTransform myt(transform);
			const glm::mat4 worldmatrix = glm::mat4_cast(myt.rotation) * glm::translate(glm::mat4(1), myt.position);

			PxShape* shapebuffer[64];
			uint32_t numshapes = rigid->getShapes(shapebuffer, 64, 0);

			const Color32 random_obj_color = randcolor32(i);
			for (uint32_t shape_idx = 0; shape_idx < numshapes; shape_idx++) {
				PxShape* shape = shapebuffer[shape_idx];
				PhysTransform myt_shape(shape->getLocalPose());
				const PxGeometry* geombase = &shape->getGeometry();

				const glm::mat4 shapematrix = worldmatrix * glm::mat4_cast(myt_shape.rotation) * glm::translate(glm::mat4(1), myt_shape.position);
				switch (geombase->getType())
				{
				case PxGeometryType::eBOX:
				{
					PxBoxGeometry* box = (PxBoxGeometry*)geombase;
					debug_mesh.PushOrientedLineBox(glm::vec3(0.0), physx_to_glm(box->halfExtents)*2.f, shapematrix, random_obj_color);
				}break;

				case PxGeometryType::eSPHERE:
					
				{
					PxSphereGeometry* sphere = (PxSphereGeometry*)geombase;
					glm::vec3 center = shapematrix[3];
					debug_mesh.AddSphere(center, sphere->radius, 12, 12, random_obj_color);
				} break;

				case PxGeometryType::eCONVEXMESH:

					break;

				case PxGeometryType::eCAPSULE:

					break;

				case PxGeometryType::eTRIANGLEMESH:

					break;
				}
			}
		}
	}
	debug_mesh.End();
	debug_mesh.Draw(MeshBuilder::LINES);
}
#endif

void PhysicsManager::debug_draw_shapes()
{
	ASSERT(impl->scene);

	if (g_draw_physx_scene.get_integer() == 0) {
		impl->scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 0.0);
		return;
	}
	using namespace physx;
	auto scene = impl->scene;
	static bool init = false;
	if (!init) {
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 2.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_EDGES, 1.0);
		init = true;
	}
	impl->debug_mesh.Begin();

	auto& rb = scene->getRenderBuffer();
	for (PxU32 i = 0; i < rb.getNbLines(); i++)
	{
		const PxDebugLine& line = rb.getLines()[i];
		// render the line
		impl->debug_mesh.PushLine(physx_to_glm(line.pos0), physx_to_glm(line.pos1), *((Color32*)&line.color0));
	}
	impl->debug_mesh.End();
	//impl->debug_mesh.Draw(MeshBuilder::LINES);
}