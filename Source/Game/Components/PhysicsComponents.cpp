#include "Render/Model.h"
#include "PhysicsComponents.h"
#include "GameEnginePublic.h"
#include "Physics/Physics2.h"
#include "BillboardComponent.h"
#include "Assets/AssetDatabase.h"
#include "Game/Entity.h"
#include "Render/Texture.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "MeshbuilderComponent.h"
#include "Physics/Physics2Local.h"
#include "Level.h"
#include "MeshComponent.h"

CLASS_IMPL(PhysicsComponentBase);
CLASS_IMPL(BoxComponent);
CLASS_IMPL(CapsuleComponent);
CLASS_IMPL(SphereComponent);
CLASS_IMPL(MeshColliderComponent);


using namespace physx;

void PhysicsComponentBase::on_init()
{
	if (eng->is_editor_level())
		return;

	auto initial_transform = get_ws_transform();

	ASSERT(!has_initialized());

	auto factory = physics_local_impl->physics_factory;
	if (is_static) {
		PxTransform t = glm_to_physx(initial_transform);
		t.q.normalize();

		physxActor = factory->createRigidStatic(t);
	}
	else {
		auto t = glm_to_physx(initial_transform);
		t.q.normalize();

		physxActor = factory->createRigidDynamic(t);
		auto dyn = (PxRigidDynamic*)physxActor;
		dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !this->simulate_physics);
	}

	physics_local_impl->scene->addActor(*physxActor);

	physxActor->userData = this;

	physxActor->setActorFlag(physx::PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
	physxActor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, !enabled);

	// call down to derived
	add_actor_shapes();

	update_mass();
}

void PhysicsComponentBase::on_deinit()
{
	if (has_initialized()) {
		physics_local_impl->scene->removeActor(*(physx::PxActor*)physxActor);
		physxActor->release();
		physxActor = nullptr;
	}
}

void PhysicsComponentBase::update_mass()
{
	ASSERT(physxActor);
	if (!get_is_actor_static()) {
		auto dyn = (PxRigidDynamic*)physxActor;
		PxRigidBodyExt::updateMassAndInertia(*dyn, 1.f);
	}
}


void CapsuleComponent::add_actor_shapes() {
	//if (disable_physics)
	//	return;
	//actor = g_physics.allocate_physics_actor(this);
	//
	//auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	//actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	//actor->add_vertical_capsule_to_actor(glm::vec3(0.f), height, radius);
	//actor->update_mass();
	//
	//if (eng->is_editor_level())
	//{
	//	auto b = get_owner()->create_and_attach_component_type<BillboardComponent>();
	//	b->set_texture(default_asset_load<Texture>("icon/_nearest/capsule_collider.png"));
	//	b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	//}

	add_vertical_capsule_to_actor(glm::vec3(0.f), height, radius);
}

void BoxComponent::add_actor_shapes() {
	//actor = g_physics.allocate_physics_actor(this);
	//auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	//actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	//actor->add_box_shape_to_actor(get_ws_transform(), side_len);
	//actor->update_mass();
	//
	//if (eng->is_editor_level())
	//{
	//	auto b = get_owner()->create_and_attach_component_type<BillboardComponent>();
	//	b->set_texture(default_asset_load<Texture>("icon/_nearest/box_collider.png"));
	//	b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	//
	//	editor_view = get_owner()->create_and_attach_component_type<MeshBuilderComponent>();
	//	auto& mb = editor_view->editor_mb;
	//	mb.Begin();
	//	mb.PushLineBox(glm::vec3(-0.5),glm::vec3(0.5), COLOR_RED);
	//	mb.End();
	//}

	add_box_shape_to_actor(get_ws_transform(), get_owner()->get_ls_scale());
}


void SphereComponent::add_actor_shapes() {
	//actor = g_physics.allocate_physics_actor(this);
	//auto preset = (physics_preset.ptr) ? (PhysicsFilterPresetBase*)physics_preset.ptr->default_class_object : nullptr;
	//actor->init_physics_shape(preset, get_ws_transform(), simulate_physics, send_overlap, send_hit, is_static, is_trigger, disable_physics);
	//actor->add_sphere_shape_to_actor(get_ws_transform()[3], radius);
	//actor->update_mass();
	//
	//if (eng->is_editor_level())
	//{
	//	auto b = get_owner()->create_and_attach_component_type<BillboardComponent>();
	//	b->set_texture(default_asset_load<Texture>("icon/_nearest/sphere_collider.png"));
	//	b->dont_serialize_or_edit = true;	// editor only item, dont serialize
	//
	//	editor_view = get_owner()->create_and_attach_component_type<MeshBuilderComponent>();
	//	auto& mb = editor_view->editor_mb;
	//	mb.Begin();
	//	mb.AddSphere({}, radius, 8, 8, COLOR_RED);
	//	mb.End();
	//}
	add_sphere_shape_to_actor(get_ws_transform()[3], radius);
}
void MeshColliderComponent::add_actor_shapes() {
	auto mesh = get_owner()->get_first_component<MeshComponent>();
	if (!mesh)
		sys_print(Error, "MeshColliderComponent couldnt find MeshComponent");
	else
		add_model_shape_to_actor(mesh->get_model());
}

void PhysicsComponentBase::on_changed_transform() {
	if (!has_initialized())
		return;
	set_transform(get_ws_transform());
}
PhysicsComponentBase::~PhysicsComponentBase()
{
}
PhysicsComponentBase::PhysicsComponentBase() {
	physics_preset.ptr = &PP_DefaultBlockAll::StaticType;
}

bool PhysicsComponentBase::get_is_actor_static() const {
	ASSERT(physxActor);
	return physxActor->getType() == physx::PxActorType::eRIGID_STATIC;
}

void PhysicsComponentBase::apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse)
{
	if (has_initialized()) {
		physx::PxRigidBodyExt::addForceAtPos(
			*get_dynamic_actor(),
			glm_to_physx(impulse),
			glm_to_physx(worldspace),
			physx::PxForceMode::eIMPULSE);
	}
}
glm::mat4 PhysicsComponentBase::get_transform() const
{
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();
	auto mat = glm::translate(glm::mat4(1), physx_to_glm(pose.p));
	mat = mat *  glm::mat4_cast(physx_to_glm(pose.q));
	return mat;
}

physx::PxRigidDynamic* PhysicsComponentBase::get_dynamic_actor() const {
	ASSERT(physxActor && !get_is_actor_static());
	return (physx::PxRigidDynamic*)physxActor;
}
glm::vec3 PhysicsComponentBase::get_linear_velocity() const {
	return physx_to_glm(get_dynamic_actor()->getLinearVelocity());
}
void PhysicsComponentBase::set_shape_flags(PxShape* shape)
{
	if (is_trigger)
		shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eVISUALIZATION);
	else
		shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eVISUALIZATION);
}
void PhysicsComponentBase::add_model_shape_to_actor(const Model* model)
{
	if (model->get_physics_body()) {
		auto body = model->get_physics_body();
		for (int i = 0; i < body->num_shapes_of_main; i++) {
			auto& shape = body->shapes[i];
			if (shape.shape != ShapeType_e::ConvexShape)
				continue;

			PxShape* aConvexShape = PxRigidActorExt::createExclusiveShape(*physxActor,
				PxConvexMeshGeometry(shape.convex_mesh), *physics_local_impl->default_material);
			set_shape_flags(aConvexShape);
		}
	}
	else {
		auto aabb = model->get_bounds();
		auto boxGeom = PxBoxGeometry(glm_to_physx((aabb.bmax - aabb.bmin) * 0.5f));

		auto shape = PxRigidActorExt::createExclusiveShape(*physxActor,
			boxGeom, *physics_local_impl->default_material);

		auto middle = (aabb.bmax + aabb.bmin) * 0.5f;

		shape->setLocalPose(PxTransform(glm_to_physx(middle)));

		set_shape_flags(shape);
	}
}
void PhysicsComponentBase::add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius)
{
	auto capGeom = PxCapsuleGeometry(radius, height * 0.5);
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor,
		capGeom, *physics_local_impl->default_material);

	glm::vec3 targetCenter = base + glm::vec3(0.f, height * 0.5f, 0.f);

	shape->setLocalPose(PxTransform(glm_to_physx(targetCenter)));
	set_shape_flags(shape);
}
void PhysicsComponentBase::add_sphere_shape_to_actor(const glm::vec3& pos, float radius)
{
	auto boxGeom = PxSphereGeometry(radius);
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor,
		boxGeom, *physics_local_impl->default_material);
	shape->setLocalPose(PxTransform(glm_to_physx(pos)));
	set_shape_flags(shape);
}
void PhysicsComponentBase::add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents)
{
	auto boxGeom = PxBoxGeometry(glm_to_physx(halfExtents));
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor,
		boxGeom, *physics_local_impl->default_material);
	shape->setLocalPose(glm_to_physx(localTransform));
	set_shape_flags(shape);
}

void PhysicsComponentBase::set_is_simulating(bool simulate_physics)
{
	if (simulate_physics != this->simulate_physics) {
		this->simulate_physics = simulate_physics;
		if (has_initialized()) {
			if (get_is_actor_static()) {
				sys_print(Warning, "set_simulating set on a static PhysicsActor\n");
			}
			else {
				auto dyn = get_dynamic_actor();
				dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !this->simulate_physics);
			}
		}
	}
}
void PhysicsComponentBase::set_is_enable(bool enabled)
{
	if (enabled != this->enabled) {
		this->enabled = enabled;
		if (has_initialized()) {
			physxActor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
		}
	}
}

void PhysicsComponentBase::set_transform(const glm::mat4& transform, bool teleport)
{
	if (has_initialized()) {
		if (simulate_physics) {
			// empty
		}
		else if (get_is_actor_static()) {
			auto t = glm_to_physx(transform);
			t.q.normalize();
			physxActor->setGlobalPose(t);
			if (simulate_physics)
				set_linear_velocity({});
		}
		else {
			auto dyn = get_dynamic_actor();
			dyn->setKinematicTarget(glm_to_physx(transform));
		}
	}
}

void PhysicsComponentBase::set_linear_velocity(const glm::vec3& v)
{
	if (has_initialized()) {
		if (get_is_actor_static()) {
			sys_print(Warning, "set_linear_velocity on a static PhysicsActor\n");
		}
		else {
			auto dyn = get_dynamic_actor();
			dyn->setLinearVelocity(glm_to_physx(v));
		}
	}
}

void PhysicsComponentBase::set_is_trigger(bool is_trig) {
	is_trigger = is_trig;
}
void PhysicsComponentBase::set_send_overlap(bool send_overlap) {
	this->send_overlap = send_overlap;
}
void PhysicsComponentBase::set_send_hit(bool send_hit) {
	this->send_hit = send_hit;
}
void PhysicsComponentBase::set_is_static(bool is_static)
{
	this->is_static = is_static;
}