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

void PhysicsComponentBase::fetch_new_transform()
{
	ASSERT(get_is_simulating());
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();
	last_position = next_position;
	last_rot = next_rot;
	next_position = physx_to_glm(pose.p);
	next_rot = physx_to_glm(pose.q);
	set_ticking(true);
}
void PhysicsComponentBase::update()
{
	// interpolate
	float alpha = eng->get_frame_remainder()/eng->get_fixed_tick_interval();
	auto ip = glm::mix(last_position, next_position,alpha);
	auto iq = glm::slerp(last_rot, next_rot, alpha);
	auto mat = glm::translate(glm::mat4(1),ip);
	mat = mat *  glm::mat4_cast(iq);
	get_owner()->set_ws_transform(mat);
}

// Initialization done in pre_start now to let joint initialization work properly in start()

void PhysicsComponentBase::pre_start()
{
	if (eng->is_editor_level()) 
		return;

	ASSERT(editor_shape_id==0);

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

	set_ticking(false);
	// latch physics interpolation
	next_position = get_owner()->get_ls_position();
	next_rot = get_owner()->get_ls_rotation();

	update_mass();

}
void PhysicsComponentBase::start()
{
	if (eng->is_editor_level()) {
		auto shape = get_owner()->create_and_attach_component_type<MeshBuilderComponent>();
		shape->dont_serialize_or_edit = true;
		editor_shape_id = shape->get_instance_id();
		auto mb = get_editor_meshbuilder();
		mb->use_background_color = true;
		mb->mb.Begin();
		add_editor_shapes();
		mb->mb.End();	
		mb->on_changed_transform();
		return;
	}
}

void PhysicsComponentBase::end()
{
	if (editor_shape_id != 0) {
		auto shapeptr = eng->get_level()->get_entity(editor_shape_id);
		if (shapeptr) {
			((EntityComponent*)shapeptr)->destroy();
		}
		editor_shape_id = 0;
	}
		

	if (has_initialized()) {
		physics_local_impl->scene->removeActor(*(physx::PxActor*)physxActor);
		physxActor->userData = nullptr;	// cursed moment, get a stale pointer in onTrigger after actor has been removed (?), set it null here to avoid that
		physxActor->release();
		physxActor = nullptr;
	}
}

void PhysicsComponentBase::update_mass()
{
	ASSERT(physxActor);
	if (!get_is_actor_static()) {
		auto dyn = (PxRigidDynamic*)physxActor;
		PxRigidBodyExt::updateMassAndInertia(*dyn, 3.f);
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

	add_vertical_capsule_to_actor(glm::vec3(0, height_offset, 0), height, radius);
}

static Color32 mb_color = { 86, 150, 252 };

void CapsuleComponent::add_editor_shapes() {
	auto mb = get_editor_meshbuilder();
	mb->mb.AddLineCapsule(glm::vec3(0, height_offset, 0), radius, height * 0.5, mb_color);
}
void BoxComponent::add_editor_shapes() {
	auto mb = get_editor_meshbuilder();
	mb->mb.PushLineBox(glm::vec3(-0.5f),glm::vec3(0.5), mb_color);
}
void SphereComponent::add_editor_shapes() {
	auto mb = get_editor_meshbuilder();
	mb->mb.AddLineSphere(glm::vec3(0.f), radius, mb_color);
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

	add_box_shape_to_actor(get_ws_transform(), get_owner()->get_ls_scale()*0.5f);
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
	if (editor_shape_id != 0) {
		auto mb = get_editor_meshbuilder();
		mb->mb.Begin();
		add_editor_shapes();
		mb->mb.End();
	}

	if (!has_initialized())
		return;
	set_transform(get_ws_transform());
}
PhysicsComponentBase::~PhysicsComponentBase()
{
	ASSERT(!physxActor);
}
PhysicsComponentBase::PhysicsComponentBase() {
	set_call_init_in_editor(true);
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
		shape->setFlags(PxShapeFlag::eTRIGGER_SHAPE | PxShapeFlag::eVISUALIZATION);
	else
		shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eVISUALIZATION);
	PxFilterData filter;
	filter.word0 = (1ul << uint32_t(physics_layer));
	filter.word1 = get_collision_mask_for_physics_layer(physics_layer);
	shape->setQueryFilterData(filter);
	shape->setSimulationFilterData(filter);
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
	auto capGeom = PxCapsuleGeometry(radius, height * 0.5 - radius);
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor,
		capGeom, *physics_local_impl->default_material);


	auto localpose =  glm::translate(glm::mat4(1), base) * glm::mat4_cast(glm::angleAxis(HALFPI, glm::vec3(0,0,1)));

	shape->setLocalPose(glm_to_physx(localpose));
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

				next_position = get_owner()->get_ls_position();
				next_rot = get_owner()->get_ls_rotation();
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
void PhysicsComponentBase::set_physics_layer(PhysicsLayer l)
{
	if (l == physics_layer) return;
	physics_layer = l;
	if (has_initialized())
		refresh_shapes();
}
void PhysicsComponentBase::refresh_shapes()
{
	ASSERT(has_initialized());

	PxShape* buffer[64];
	int num = physxActor->getNbShapes();
	int start = 0;
	while (num > 0) {
		int c = physxActor->getShapes(buffer, 64, start);
		ASSERT(c <= 64);
		for (int i = 0; i < c; i++)
			set_shape_flags(buffer[i]);
		
		num -= 64;
		start += 64;
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
	if (is_trig == is_trigger) return;
	is_trigger = is_trig;
	if (has_initialized())
		refresh_shapes();
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

MeshBuilderComponent* PhysicsComponentBase::get_editor_meshbuilder() const {
	return (MeshBuilderComponent*)eng->get_level()->get_entity(editor_shape_id);
}

PhysicsJointComponent::PhysicsJointComponent()
{
	set_call_init_in_editor(true);
	set_ticking(false);
}
PhysicsJointComponent::~PhysicsJointComponent()
{

}
const PropertyInfoList* PhysicsJointComponent::get_props() {
	START_PROPS(PhysicsJointComponent)
		REG_ENTITY_PTR(target,PROP_DEFAULT),
		REG_VEC3(local_joint_from_offset,PROP_DEFAULT),
		REG_VEC3(local_joint_to_offset,PROP_DEFAULT),
		REG_INT(local_joint_axis,PROP_DEFAULT,"0")
	END_PROPS(PhysicsJointComponent)
}
void PhysicsJointComponent::refresh_joint()
{
	free_joint();
	has_joint = false;

	// init joint
	auto self_physics = get_owner_physics();
	if (!self_physics) {
		sys_print(Warning, "Joint component has no physics component\n");
		return;
	}
	PhysicsComponentBase* other = nullptr;
	if (get_target()) {
		other= get_target()->get_first_component<PhysicsComponentBase>();
		if (!other) {
			sys_print(Warning, "Joint component target has no physics component\n");
		}
	}
	init_joint(self_physics, other/* can be nullptr */);
	ASSERT(get_joint());
	has_joint = true;
}

void PhysicsJointComponent::start()
{
	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_and_attach_component_type<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;
	}
	else {
		refresh_joint();
	}
}

void PhysicsJointComponent::set_target(Entity* e)
{
	if (e != target.get()) {
		target = e->get_self_ptr();
		refresh_joint();
	}
}

void PhysicsJointComponent::end()
{
	if (editor_meshbuilder)
		editor_meshbuilder->destroy();
}

PhysicsComponentBase* PhysicsJointComponent::get_owner_physics() {
	return get_owner()->get_first_component<PhysicsComponentBase>();
}
static glm::mat4 get_transform_joint(glm::vec3 local, PhysicsComponentBase* b, int axis)
{
	auto local_t = glm::translate(glm::mat4(1), local);
	glm::mat4 m = glm::mat4(1);
	if (axis == 1) {
		m[0] = glm::vec4(0, 1, 0,0);
		m[1] = glm::vec4(-1, 0, 0,0);
	}
	else if (axis == 2) {
		m[0] = glm::vec4(0, 0, 1,0);
		m[2] = glm::vec4(-1, 0, 0,0);
	}

	return local_t * m;
}
void PhysicsJointComponent::draw_meshbuilder()
{
	glm::mat4 world = get_ws_transform();
	auto local_t = glm::translate(glm::mat4(1), local_joint_from_offset);
	world = world * local_t;
	editor_meshbuilder->mb.AddSphere(world[3], 0.25, 10, 10, COLOR_RED);

	if (!get_target())
		return;
	auto other_phys = get_target()->get_first_component<PhysicsComponentBase>();
	if (!other_phys)
		return;
	world = get_target()->get_ws_transform();
	local_t = glm::translate(glm::mat4(1), local_joint_to_offset);
	world = world * local_t;
	editor_meshbuilder->mb.AddSphere(world[3], 0.25, 10, 10, COLOR_PINK);
}


void HingeJointComponent::init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b)
{
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	auto my_local = get_transform_joint(local_joint_from_offset, a, local_joint_axis);
	auto my_world = get_ws_transform() * my_local;
	if (b) {
		auto other_world = b->get_ws_transform();
		auto other_local = glm::inverse(other_world) * my_world;
		joint = PxRevoluteJointCreate(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			b->get_physx_actor(), glm_to_physx(other_local));
	}
	else {
		joint = PxRevoluteJointCreate(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			nullptr, glm_to_physx(my_world));
	}
	joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);

}
physx::PxJoint* HingeJointComponent::get_joint() const  {
	return joint;
}
void HingeJointComponent::free_joint()
{
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}

void BallSocketJointComponent::init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b)
{
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	auto my_local = get_transform_joint(local_joint_from_offset, a, local_joint_axis);
	auto my_world = get_ws_transform() * my_local;
	if (b) {
		auto other_world = b->get_ws_transform();
		auto other_local = glm::inverse(other_world) * my_world;
		joint = PxSphericalJointCreate(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			b->get_physx_actor(), glm_to_physx(other_local));
	}
	else {
		joint = PxSphericalJointCreate(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			nullptr, glm_to_physx(my_world));
	}
	joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);
}
physx::PxJoint* BallSocketJointComponent::get_joint() const  {
	return joint;
}
void BallSocketJointComponent::free_joint()
{
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}

void PhysicsJointComponent::on_changed_transform() {
	if (editor_meshbuilder) {
		editor_meshbuilder->mb.Begin();
		draw_meshbuilder();
		editor_meshbuilder->mb.End();
	}
}
void PhysicsJointComponent::editor_on_change_property() {
	if (editor_meshbuilder) {
		editor_meshbuilder->mb.Begin();
		draw_meshbuilder();
		editor_meshbuilder->mb.End();
	}
}

CLASS_IMPL(PhysicsJointComponent);
CLASS_IMPL(HingeJointComponent);
CLASS_IMPL(BallSocketJointComponent);