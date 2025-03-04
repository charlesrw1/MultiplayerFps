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
#include "Animation/Runtime/Animation.h"
#include "Scripting/FunctionReflection.h"

#include <physx/foundation/PxTransform.h>

#include "Framework/AddClassToFactory.h"
#include "Game/AssetPtrMacro.h"
#include "Game/EntityPtrMacro.h"

ConfigVar ed_physics_shapes_depth_tested("ed_physics_shapes_depth_tested", "1", CVAR_BOOL, "are physics shapes in the editor depth tested");

using namespace physx;

void PhysicsComponentBase::fetch_new_transform()
{
	ASSERT(get_is_simulating());
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();

	if (0&&interpolate_visuals) {
		last_position = next_position;
		last_rot = next_rot;
		next_position = physx_to_glm(pose.p);
		next_rot = physx_to_glm(pose.q);
		//if (!get_owner()->get_parent()) {
		//}
		//else {
		//	auto parent_t = get_owner()->get_parent()->get_ws_transform();
		//	glm::mat4 myworld = glm::translate(glm::mat4(1.f), physx_to_glm(pose.p)) * glm::mat4_cast(physx_to_glm(pose.q));
		//	auto mylocal = glm::inverse(parent_t) * myworld;
		//	next_rot = glm::normalize(glm::quat_cast(mylocal));
		//	next_position = mylocal[3];
		//}
		set_ticking(true);
	}
	else {
		get_owner()->set_ws_transform(physx_to_glm(pose.p), physx_to_glm(pose.q), get_owner()->get_ls_scale());
	}
}

glm::vec3 calc_angular_vel(const glm::quat& q1, const glm::quat& q2, float dt) {
    glm::quat dq = q2 * glm::inverse(q1);
    if (dq.w < 0.0f) {
        dq = glm::quat(-dq.w, -dq.x, -dq.y, -dq.z);
    }
    return 2.0f * glm::vec3(dq.x, dq.y, dq.z) / dt;
}

void PhysicsComponentBase::enable_with_initial_transforms(const glm::mat4& t0, const glm::mat4& t1, float dt)
{
	set_is_enable(true);
	auto rot0 = glm::quat_cast(t0);
	auto rot1 = glm::quat_cast(t1);
	glm::vec3 lin_vel = (glm::vec3(t1[3]) - glm::vec3(t0[3])) / dt;
	glm::vec3 ang_vel = calc_angular_vel(rot0, rot1, dt);
	set_linear_velocity(lin_vel);
	set_angular_velocity(ang_vel);
	last_position = t0[3];
	next_position = t1[3];
	last_rot = rot0;
	next_rot = rot1;
	get_owner()->set_is_top_level(true);
	get_owner()->set_ws_transform(t1);
	force_set_transform(t1);
	set_ticking(true);
}


void PhysicsComponentBase::update()
{
	set_ticking(false);
	return;
	if (!enabled || !get_is_simulating() || !interpolate_visuals) {
		return;
	}

	// interpolate
	float alpha = eng->get_frame_remainder()/eng->get_fixed_tick_interval();
	auto ip = glm::mix(last_position, next_position,alpha);
	auto iq = glm::slerp(last_rot, next_rot, alpha);
	auto mat = glm::translate(glm::mat4(1),ip);
	mat = mat *  glm::mat4_cast(iq);

	get_owner()->set_ws_transform(mat);
}
void PhysicsComponentBase::set_linear_velocity(const glm::vec3& v)
{
	if (auto d = get_dynamic_actor()) {
		d->setLinearVelocity(glm_to_physx(v));
	}
}
void PhysicsComponentBase::set_angular_velocity(const glm::vec3& v)
{
	if (auto d = get_dynamic_actor()) {
		d->setAngularVelocity(glm_to_physx(v));
	}
}

// Initialization done in pre_start now to let joint initialization work properly in start()

void PhysicsComponentBase::pre_start()
{
	if (eng->is_editor_level()) 
		return;

	ASSERT(editor_shape_id==0);

	auto& initial_transform = get_ws_transform();

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

	if (get_is_simulating()) {
		auto& ws = get_ws_transform();
		get_owner()->set_is_top_level(true);
		get_owner()->set_ws_transform(ws);
	}

}

void PhysicsComponentBase::update_bone_parent_animator()
{
	ASSERT(init_state != initialization_state::HAS_ID);

	auto get_the_parent_animator = [&]() -> AnimatorInstance* {
		if (get_owner()->has_parent_bone()) {
			if (get_owner()->get_parent()) {
				auto m = get_owner()->get_parent()->get_cached_mesh_component();
				if (m) {
					return m->get_animator_instance();
				}
			}
		}
		return nullptr;
	};

	auto a = get_the_parent_animator();
	if (!a) return;
	if (!simulate_physics ||!enabled)
		a->remove_simulating_physics_object(get_owner());
	else
		a->add_simulating_physics_object(get_owner());
}

void PhysicsComponentBase::start()
{
	if (eng->is_editor_level()) {
		auto shape = get_owner()->create_component<MeshBuilderComponent>();
		shape->dont_serialize_or_edit = true;
		editor_shape_id = shape->get_instance_id();
		auto mb = get_editor_meshbuilder();
		mb->use_background_color = true;
		mb->depth_tested = ed_physics_shapes_depth_tested.get_bool();
		mb->mb.Begin();
		add_editor_shapes();
		mb->mb.End();	
		mb->on_changed_transform();
		return;
	}

	update_bone_parent_animator();
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

	simulate_physics = false;
	update_bone_parent_animator();
}

void PhysicsComponentBase::update_mass()
{
	ASSERT(physxActor);
	if (!get_is_actor_static()) {
		auto dyn = (PxRigidDynamic*)physxActor;
		PxRigidBodyExt::updateMassAndInertia(*dyn,density);
	}
}
float PhysicsComponentBase::get_mass()const
{
	ASSERT(physxActor);
	if (!get_is_actor_static() && physxActor) {
		auto dyn = (PxRigidDynamic*)physxActor;
		return dyn->getMass();
	}
	return 0.f;
}


void CapsuleComponent::add_actor_shapes() {

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
	
	add_box_shape_to_actor(glm::mat4(1.f), get_owner()->get_ls_scale() * 0.5f);
}


void SphereComponent::add_actor_shapes() {
	auto scale = get_owner()->get_ls_scale();
	auto scale_sz = glm::max(scale.x, glm::max(scale.y, scale.z));

	add_sphere_shape_to_actor(glm::vec3(0.f), radius* scale_sz);
}
void MeshColliderComponent::add_actor_shapes() {
	auto mesh = get_owner()->get_component<MeshComponent>();
	if (!mesh || !mesh->get_model())
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
	if (!enabled)	// not enabled, skip
		return;	
	if (simulate_physics)
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
	if (!physxActor)
		return false;
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
void PhysicsComponentBase::apply_force(const glm::vec3& worldspace, const glm::vec3& force)
{
	if (has_initialized()) {
		physx::PxRigidBodyExt::addForceAtPos(
			*get_dynamic_actor(),
			glm_to_physx(force),
			glm_to_physx(worldspace),
			physx::PxForceMode::eFORCE);
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
		for (int i = 0; i < body->shapes.size(); i++) {
			auto& shape = body->shapes[i];
			if (shape.shape == ShapeType_e::ConvexShape) {
				PxMeshScale scale;
				scale.scale = glm_to_physx(get_owner()->get_ls_scale());
				PxShape* aConvexShape = PxRigidActorExt::createExclusiveShape(*physxActor,
					PxConvexMeshGeometry(shape.convex_mesh,scale), *physics_local_impl->default_material);
				set_shape_flags(aConvexShape);
			}
			else if (shape.shape == ShapeType_e::MeshShape) {
				PxMeshScale scale;
				scale.scale = glm_to_physx(get_owner()->get_ls_scale());

				PxShape* tri_shape = PxRigidActorExt::createExclusiveShape(*physxActor,
					PxTriangleMeshGeometry(shape.tri_mesh,scale), *physics_local_impl->default_material
				);
				set_shape_flags(tri_shape);
			}
		}
	}
	else {
		auto& aabb = model->get_bounds();

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

				update_bone_parent_animator();
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
			if (!enabled)
				get_owner()->set_is_top_level(false);

			update_bone_parent_animator();
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
void PhysicsComponentBase::force_set_transform(const glm::mat4& m)
{
	if (has_initialized()) {
		physxActor->setGlobalPose(glm_to_physx(m), true);
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

void PhysicsJointComponent::refresh_joint()
{
	free_joint();

	// init joint
	auto self_physics = get_owner_physics();
	if (!self_physics) {
		sys_print(Warning, "Joint component has no physics component\n");
		return;
	}
	PhysicsComponentBase* other = nullptr;
	if (get_target()) {
		other= get_target()->get_component<PhysicsComponentBase>();
		if (!other) {
			sys_print(Warning, "Joint component target has no physics component\n");
		}
	}
	init_joint(self_physics, other/* can be nullptr */);
	ASSERT(get_joint());
}

void PhysicsJointComponent::start()
{
	if (eng->is_editor_level()) {
		editor_meshbuilder = get_owner()->create_component<MeshBuilderComponent>();
		editor_meshbuilder->dont_serialize_or_edit = true;
		editor_meshbuilder->use_background_color = true;
		editor_meshbuilder->use_transform = false;
		editor_meshbuilder->depth_tested = ed_physics_shapes_depth_tested.get_bool();
	}

	if(!eng->is_editor_level()) {
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
	return get_owner()->get_component<PhysicsComponentBase>();
}
static glm::mat4 get_transform_joint(JointAnchor anchor, int axis)
{
	auto local_t = glm::translate(glm::mat4(1), anchor.p) * glm::mat4_cast(anchor.q);
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
	auto local_t = glm::translate(glm::mat4(1), anchor.p);
	world = world * local_t;
	editor_meshbuilder->mb.AddSphere(world[3], 0.05, 10, 10, COLOR_RED);

	if (!get_target())
		return;
	auto other_phys = get_target()->get_component<PhysicsComponentBase>();
	if (!other_phys)
		return;
	world = get_target()->get_ws_transform();
	
}

template<typename T>
static T* make_joint_shared(const glm::mat4& ws_transform, JointAnchor anchor, int local_joint_axis, T*(*create_func)(PxPhysics&, PxRigidActor*, const PxTransform&, PxRigidActor*, const PxTransform&), PhysicsComponentBase* a, PhysicsComponentBase*b)
{
	T* joint = nullptr;
	auto my_local = get_transform_joint(anchor, local_joint_axis);
	auto my_world = ws_transform * my_local;
	if (b) {
		auto& other_world = b->get_ws_transform();
		auto other_local = glm::inverse(other_world) * my_world;
		joint = create_func(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			b->get_physx_actor(), glm_to_physx(other_local));
	}
	else {
		joint = create_func(*physics_local_impl->physics_factory,
			a->get_physx_actor(), glm_to_physx(my_local),
			nullptr, glm_to_physx(my_world));
	}
	if(joint)
		joint->setConstraintFlag(PxConstraintFlag::eVISUALIZATION, true);

	return joint;
}

void HingeJointComponent::init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b)
{
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	joint = make_joint_shared(get_ws_transform(), anchor, local_joint_axis, PxRevoluteJointCreate, a, b);

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

	joint = make_joint_shared(get_ws_transform(), anchor, local_joint_axis, PxSphericalJointCreate, a, b);
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



PxJoint* AdvancedJointComponent::get_joint() const {
	return joint;
}
void AdvancedJointComponent::init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b) {
	ASSERT(!joint);
	ASSERT(get_owner() == a->get_owner());
	joint = make_joint_shared(get_ws_transform(), anchor, local_joint_axis, PxD6JointCreate, a, b);
	auto get_jm_enum = [&](JointMotion jm) {
		if (jm == JM::Free) return PxD6Motion::eFREE;
		else if (jm == JM::Limited) return PxD6Motion::eLIMITED;
		else return PxD6Motion::eLOCKED;
	};
	if (!joint)
		return;
	joint->setMotion(PxD6Axis::eX, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eY, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eZ, get_jm_enum(x_motion));
	joint->setMotion(PxD6Axis::eTWIST, get_jm_enum(ang_x_motion));
	joint->setMotion(PxD6Axis::eSWING1, get_jm_enum(ang_y_motion));
	joint->setMotion(PxD6Axis::eSWING2, get_jm_enum(ang_z_motion));
	//joint->setLinearLimit(PxJointLinearLimit(linear_distance_max, PxSpring(linear_stiff, linear_damp)));
	joint->setTwistLimit(PxJointAngularLimitPair(twist_limit_min,twist_limit_max,PxSpring(twist_stiff,twist_damp)));
	if (ang_y_limit <= 0) ang_y_limit = 0.00001;
	if (ang_z_limit <= 0) ang_z_limit = 0.00001;
	joint->setSwingLimit(PxJointLimitCone(ang_y_limit, ang_z_limit, PxSpring(cone_stiff, cone_damp)));
}
void AdvancedJointComponent::free_joint()
{
	if (joint) {
		joint->release();
		joint = nullptr;
	}
}
void AdvancedJointComponent::draw_meshbuilder()
{
	auto myworld = get_ws_transform();
	auto mylocal = get_transform_joint(anchor, local_joint_axis);
	myworld = myworld * mylocal;

	// draw x angle
	auto origin = myworld[3];
	float length = 0.4;
	if(ang_x_motion==JM::Limited)
	{
		glm::vec3 min = glm::vec3(0,sin(twist_limit_min), cos(twist_limit_min)) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min,1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_RED);
		glm::vec3 max = glm::vec3(0,sin(twist_limit_max), cos(twist_limit_max)) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_RED);
	}
	if(ang_y_motion==JM::Limited)
	{
		glm::vec3 min = glm::vec3(sin(-ang_y_limit),0, cos(-ang_y_limit)) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min,1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_GREEN);
		glm::vec3 max = glm::vec3(sin(ang_y_limit),0, cos(ang_y_limit)) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_GREEN);
	}
	if(ang_z_motion==JM::Limited)
	{
		glm::vec3 min = glm::vec3(cos(-ang_z_limit),sin(-ang_z_limit),0) * length;
		glm::vec3 min_vec = myworld * glm::vec4(min,1.0);
		editor_meshbuilder->mb.PushLine(origin, min_vec, COLOR_BLUE);
		glm::vec3 max = glm::vec3(cos(ang_z_limit),sin(ang_z_limit),0) * length;
		glm::vec3 max_vec = myworld * glm::vec4(max, 1.0);
		editor_meshbuilder->mb.PushLine(origin, max_vec, COLOR_BLUE);
	}
}

#ifdef EDITOR_BUILD
// FIXME!
#include "LevelEditor/EditorDocLocal.h"
class AnchorJointEditor : public IPropertyEditor
{
public:
	~AnchorJointEditor() {
		if (ed_doc.manipulate->is_using_key_for_custom(this))
			ed_doc.manipulate->stop_using_custom();
	}
	// Inherited via IPropertyEditor
	virtual bool internal_update() override
	{
		JointAnchor* j = (JointAnchor*)prop->get_ptr(instance);

		Entity* me = ed_doc.selection_state->get_only_one_selected().get();
		if (!me) {
			ImGui::Text("no Entity* found\n");
			return false;
		}

		if (ed_doc.manipulate->is_using_key_for_custom(this)) {
			auto last_matrix = ed_doc.manipulate->get_custom_transform();
			auto local = glm::inverse(me->get_ws_transform()) * last_matrix;
			j->q = (glm::quat_cast(local));
			j->p = local[3];

		};

		bool ret = false;
		if (ImGui::DragFloat3("##vec", (float*)&j->p, 0.05))
			ret = true;
		glm::vec3 eul = glm::eulerAngles(j->q);
		eul *= 180.f / PI;
		if (ImGui::DragFloat3("##eul", &eul.x, 1.0)) {
			eul *= PI / 180.f;
			j->q = glm::normalize(glm::quat(eul));

			ret = true;
		}

		glm::mat4 matrix = glm::translate(glm::mat4(1.f), j->p) * glm::mat4_cast(j->q);
		ed_doc.manipulate->set_start_using_custom(this, me->get_ws_transform() *  matrix);

		return true;
	}
};

ADDTOFACTORYMACRO_NAME(AnchorJointEditor, IPropertyEditor, "JointAnchor");
#endif
class AnchorJointSerializer : public IPropertySerializer
{
	// Inherited via IPropertySerializer
	virtual std::string serialize(DictWriter& out, const PropertyInfo& info, const void* inst, ClassBase* user) override
	{
		const JointAnchor* j = (const JointAnchor*)info.get_ptr(inst);
		return string_format("%f %f %f %f %f %f %f", j->p.x, j->p.y, j->p.z, j->q.w, j->q.x, j->q.y, j->q.z);
	}
	virtual void unserialize(DictParser& in, const PropertyInfo& info, void* inst, StringView token, ClassBase* user) override
	{
		JointAnchor* j = (JointAnchor*)info.get_ptr(inst);
		std::string to_str(token.str_start, token.str_len);
		int args = sscanf(to_str.c_str(), "%f %f %f %f %f %f %f", &j->p.x, &j->p.y, &j->p.z, &j->q.w, &j->q.x, &j->q.y, &j->q.z);
		if (args != 7) sys_print(Warning, "Anchor joint unserializer fail\n");
	}
};
ADDTOFACTORYMACRO_NAME(AnchorJointSerializer, IPropertySerializer, "JointAnchor");