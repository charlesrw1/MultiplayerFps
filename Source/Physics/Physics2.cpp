#include "Physics2.h"
#include <physx/PxRigidActor.h>
#include <physx/PxRigidDynamic.h>
#include <physx/extensions/PxRigidBodyExt.h>
#include <physx/geometry/PxBoxGeometry.h>
#include <physx/geometry/PxSphereGeometry.h>
#include <physx/geometry/PxCapsuleGeometry.h>
#include <physx/PxScene.h>

#include <physx/foundation/PxFoundation.h>
#include <physx/PxPhysicsAPI.h>
#include <physx/PxActor.h>
#include <physx/PxScene.h>
#include <physx/PxPhysics.h>
#include <physx/characterkinematic/PxController.h>
#include <physx/foundation/PxPhysicsVersion.h>
#include <physx/common/PxTolerancesScale.h>

#define COOKING

#include "physx/cooking/PxCooking.h"
#include "Framework/Util.h"

// for debug drawing
#include "Framework/MeshBuilder.h"

#define WARN_ONCE(a,...) { \
	static bool has_warned = false; \
	if (!has_warned) { \
		sys_print(a, __VA_ARGS__);\
		has_warned = true;\
	} }

// Int is a bitflags:
//	1 = draw dynamic shapes (rigid body physics driven stuff)
//	2 = draw trigger volumes
//	4 = draw static shapes	 (world)
//	8 = draw kinematic shapes (players, doors, etc.)
//  16 = draw contraints	(bones, etc.)
// set to 31 for everything

struct DebugPhysxSceneFlags
{
	enum Enum
	{
		Dynamic = 1,
		Trigger = 2,
		Static = 4,
		Kinematic = 8,
		Contraints = 16,
	};
};

ConfigVar g_draw_physx_scene("g_draw_physx_scene", "0", CVAR_DEV | CVAR_INTEGER, 0, 31);
ConfigVar g_draw_every_physx_ray_hit("g_draw_every_physx_ray_hit", "0", CVAR_DEV | CVAR_BOOL);
ConfigVar g_draw_every_physx_contact("g_draw_every_physx_contact", "0", CVAR_DEV | CVAR_BOOL);


inline glm::vec3 physx_to_glm(const physx::PxVec3& v) {
	return glm::vec3(v.x, v.y, v.z);
}
inline glm::quat physx_to_glm(const physx::PxQuat& v) {
	return glm::quat(v.w, v.x, v.y, v.z);
}
inline physx::PxVec3 glm_to_physx(const glm::vec3& v) {
	return physx::PxVec3(v.x, v.y, v.z);
}

class PhysicsManLocal : public PhysicsManPublic
{
public:
	void trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& end, PhysContents::Enum mask) override {
		float length = glm::length(end - start);
		glm::vec3 dir = (end - start) / length;
		trace_ray(out, start, dir, length, mask);
	}
	void trace_ray(world_query_result& out, const glm::vec3& start, const glm::vec3& dir, float length, PhysContents::Enum mask) override {
		physx::PxRaycastBuffer hit;
		bool status = scene->raycast(
			glm_to_physx(start), glm_to_physx(dir), length, hit);
		sys_print("ray: %d\n", (int)status);
		out.fraction = 1.0;
	}
	
	PhysicsConstraint* allocate_constraint() override {
		return new PhysicsConstraint;
	}
	void free_constraint(PhysicsConstraint*& constraint) override {
		delete constraint;
		constraint = nullptr;
	}

	PhysicsActor* allocate_physics_actor() override {
		return new PhysicsActor; 
	}
	void free_physics_actor(PhysicsActor*& actor) override {
		delete actor;
		actor = nullptr;
	}

	physx::PxScene* get_physx_scene() { return scene; }

	void init() override {
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
	}
	void clear_scene() override {}
	void simulate_and_fetch(float dt) override {
		scene->simulate(dt);
		scene->fetchResults(true/* block */);
	}
	void on_level_start() override {}
	void on_level_end() override {}

	void debug_draw_shapes() override;

	physx::PxMaterial* default_material = nullptr;
	physx::PxDefaultCpuDispatcher* dispatcher = nullptr;
	physx::PxDefaultErrorCallback err;
	physx::PxDefaultAllocator alloc;
	physx::PxScene* scene = nullptr;
	physx::PxPhysics* physics_factory = nullptr;
	physx::PxFoundation* foundation = nullptr;

	MeshBuilder debug_mesh;
};

static PhysicsManLocal physics_local;
PhysicsManPublic* g_physics = &physics_local;

PhysTransform::PhysTransform(const physx::PxTransform& t) :
	position(physx_to_glm(t.p)), rotation(physx_to_glm(t.q)) {}

void PhysicsActor::apply_impulse(glm::vec3 worldspace, glm::vec3 impulse)
{
	physx::PxRigidBodyExt::addForceAtPos(
		*get_dynamic_actor(),
		glm_to_physx(impulse),
		glm_to_physx(worldspace),
		physx::PxForceMode::eIMPULSE);
}
PhysTransform PhysicsActor::get_transform() const
{
	return actor->getGlobalPose();
}

bool PhysicsActor::is_dynamic() const {
	assert(actor);
	return actor->getType() == physx::PxActorType::eRIGID_DYNAMIC;
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
	if (is_allocated()) {
		//physics_local.scene->removeActor(*(physx::PxActor*)actor);
		actor->release();
		actor = nullptr;
	}
}

void PhysicsActor::create_static_sphere_actor(sphere_def_t sphere, const glm::vec3& pos)
{
	if (is_allocated()) {
		sys_print("??? physics actor wasn't freed before call to create()\n");
		free();
	}
	physx::PxTransform t(glm_to_physx(pos));

	physx::PxRigidStatic* static_actor = physics_local.physics_factory->createRigidStatic(t);
	physx::PxShape* sphere_shape = physx::PxRigidActorExt::createExclusiveShape(*static_actor,
		physx::PxSphereGeometry(sphere.radius),*physics_local.default_material);
	physics_local.scene->addActor(*static_actor);
	actor = static_actor;
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

void PhysicsManLocal::debug_draw_shapes()
{
	ASSERT(scene);

	if (g_draw_physx_scene.get_integer() == 0) {
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 0.0);
		return;
	}
	using namespace physx;
	static bool init = false;
	if (!init) {
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eSCALE, 10.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCONTACT_NORMAL, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eBODY_AXES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_AABBS, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_SHAPES, 1.0);
		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_EDGES, 1.0);

		scene->setVisualizationParameter(physx::PxVisualizationParameter::eCOLLISION_STATIC, 1.0);
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
}