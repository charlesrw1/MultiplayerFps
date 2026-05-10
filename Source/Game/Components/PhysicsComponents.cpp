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

#include <physx/foundation/PxTransform.h>

#include "Framework/AddClassToFactory.h"

ConfigVar ed_physics_shapes_depth_tested("ed_physics_shapes_depth_tested", "1", CVAR_BOOL,
										 "are physics shapes in the editor depth tested");

using namespace physx;

void PhysicsBody::fetch_new_transform() {
	ASSERT(get_is_simulating());
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();
	in_transform_fetch = true;
	if (0 && interpolate_visuals) {
		last_position = next_position;
		last_rot = next_rot;
		next_position = physx_to_glm(pose.p);
		next_rot = physx_to_glm(pose.q);
		set_ticking(true);
	} else {
		get_owner()->set_ws_transform(physx_to_glm(pose.p), physx_to_glm(pose.q), get_owner()->get_ls_scale());
	}
	in_transform_fetch = false;
}

glm::vec3 calc_angular_vel(const glm::quat& q1, const glm::quat& q2, float dt) {
	ASSERT(dt > 0.f);
	glm::quat dq = q2 * glm::inverse(q1);
	if (dq.w < 0.0f) {
		dq = glm::quat(-dq.w, -dq.x, -dq.y, -dq.z);
	}
	return 2.0f * glm::vec3(dq.x, dq.y, dq.z) / dt;
}

void PhysicsBody::enable_with_initial_transforms(const glm::mat4& t0, const glm::mat4& t1, float dt) {
	ASSERT(dt > 0.f);
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

void PhysicsBody::add_triggered_callback(IPhysicsEventCallback* callback) {
	ASSERT(callback);
	std::shared_ptr<IPhysicsEventCallback> ptr(callback);
	IPhysicsEventCallback* key = ptr.get();
	std::function<void(PhysicsBodyEventArg)> func = [moved_ptr = std::move(ptr)](PhysicsBodyEventArg arg) {
		moved_ptr->on_event(arg);
	};
	on_trigger.add(key, func);
}

void PhysicsBody::update() {
	set_ticking(false);
	return;
	if (!enabled || !get_is_simulating() || !interpolate_visuals) {
		return;
	}

	// interpolate
	float alpha = eng->get_frame_remainder() / eng->get_fixed_tick_interval();
	auto ip = glm::mix(last_position, next_position, alpha);
	auto iq = glm::slerp(last_rot, next_rot, alpha);
	auto mat = glm::translate(glm::mat4(1), ip);
	mat = mat * glm::mat4_cast(iq);

	get_owner()->set_ws_transform(mat);
}

void PhysicsBody::set_linear_velocity(const glm::vec3& v) {
	if (auto d = get_dynamic_actor()) {
		d->setLinearVelocity(glm_to_physx(v));
	}
}
void PhysicsBody::set_angular_velocity(const glm::vec3& v) {
	if (auto d = get_dynamic_actor()) {
		d->setAngularVelocity(glm_to_physx(v));
	}
}

// Initialization done in pre_start now to let joint initialization work properly in start()
void PhysicsBody::on_shape_changes() {
	if (!physxActor)
		return;
	// clear shapes
	const PxU32 shapeCount = physxActor->getNbShapes();
	if (shapeCount != 0) {
		std::vector<PxShape*> shapes(shapeCount);
		physxActor->getShapes(shapes.data(), shapeCount);
		for (int i = 0; i < shapes.size(); i++) {
			auto shape = shapes[i];
			physxActor->detachShape(*shape);
		}
	}

	// call down to derived
	add_actor_shapes();
	update_mass();
}

void PhysicsBody::on_actor_type_change() {
	if (physxActor) {
		physics_local_impl->scene->removeActor(*(physx::PxActor*)physxActor);
		physxActor->userData = nullptr; // cursed moment, get a stale pointer in onTrigger after actor has been removed
										// (?), set it null here to avoid that
		physxActor->release();
		physxActor = nullptr;
	}

	auto& initial_transform = get_ws_transform();
	auto factory = physics_local_impl->physics_factory;
	if (is_static) {
		glm::vec3 p, s;
		glm::quat q;
		decompose_transform(initial_transform, p, q, s);
		PxTransform t = {};
		t.p = glm_to_physx(p);
		t.q = glm_to_physx(q);
		t.q.normalize();

		physxActor = factory->createRigidStatic(t);
	} else {
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

	assert(has_initialized());

	on_shape_changes();
}

void PhysicsBody::update_bone_parent_animator() {
	// ASSERT(init_state != initialization_state::HAS_ID);
}

void PhysicsBody::start() {
	// was in pre_start
	{
		ASSERT(editor_shape_id == 0);
		ASSERT(!has_initialized());
		on_actor_type_change();
		ASSERT(has_initialized());

		set_ticking(false);
		// latch physics interpolation
		next_position = get_owner()->get_ls_position();
		next_rot = get_owner()->get_ls_rotation();

		if (get_is_simulating()) {
			auto& ws = get_ws_transform();
			get_owner()->set_is_top_level(true);
			get_owner()->set_ws_transform(ws);
		}
	}

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

void PhysicsBody::stop() {
	ASSERT(get_owner());
	if (editor_shape_id != 0) {
		auto shapeptr = eng->get_level()->get_entity(editor_shape_id);
		if (shapeptr) {
			((Component*)shapeptr)->destroy();
		}
		editor_shape_id = 0;
	}

	if (has_initialized()) {
		physics_local_impl->scene->removeActor(*(physx::PxActor*)physxActor);
		physxActor->userData = nullptr; // cursed moment, get a stale pointer in onTrigger after actor has been removed
										// (?), set it null here to avoid that
		physxActor->release();
		physxActor = nullptr;
	}
	assert(!physxActor);
	simulate_physics = false;
	update_bone_parent_animator();
}

void PhysicsBody::update_mass() {
	ASSERT(physxActor);
	if (!get_is_actor_static()) {
		auto dyn = (PxRigidDynamic*)physxActor;
		PxRigidBodyExt::updateMassAndInertia(*dyn, density);
	}
}

float PhysicsBody::get_mass() const {
	ASSERT(physxActor);
	if (!get_is_actor_static() && physxActor) {
		auto dyn = (PxRigidDynamic*)physxActor;
		return dyn->getMass();
	}
	return 0.f;
}

void PhysicsBody::on_changed_transform() {
	ASSERT(get_owner());
	if (editor_shape_id != 0) {
		auto mb = get_editor_meshbuilder();
		mb->mb.Begin();
		add_editor_shapes();
		mb->mb.End();
	}

	if (!has_initialized())
		return;
	if (!enabled) // not enabled, skip
		return;
	if (simulate_physics && in_transform_fetch) // skip if in transform fetching
		return;
	set_transform(get_ws_transform());
}

PhysicsBody::~PhysicsBody() {
	ASSERT(!physxActor);
}
PhysicsBody::PhysicsBody() {
	set_call_init_in_editor(true);
}

bool PhysicsBody::get_is_actor_static() const {
	if (!physxActor)
		return false;
	return physxActor->getType() == physx::PxActorType::eRIGID_STATIC;
}

void PhysicsBody::apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse) {
	ASSERT(has_initialized());
	if (has_initialized()) {
		physx::PxRigidBodyExt::addForceAtPos(*get_dynamic_actor(), glm_to_physx(impulse), glm_to_physx(worldspace),
											 physx::PxForceMode::eIMPULSE);
	}
}
void PhysicsBody::apply_force(const glm::vec3& worldspace, const glm::vec3& force) {
	ASSERT(has_initialized());
	if (has_initialized()) {
		physx::PxRigidBodyExt::addForceAtPos(*get_dynamic_actor(), glm_to_physx(force), glm_to_physx(worldspace),
											 physx::PxForceMode::eFORCE);
	}
}

glm::mat4 PhysicsBody::get_transform() const {
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();
	auto mat = glm::translate(glm::mat4(1), physx_to_glm(pose.p));
	mat = mat * glm::mat4_cast(physx_to_glm(pose.q));
	return mat;
}

physx::PxRigidDynamic* PhysicsBody::get_dynamic_actor() const {
	ASSERT(physxActor && !get_is_actor_static());
	return (physx::PxRigidDynamic*)physxActor;
}

glm::vec3 PhysicsBody::get_linear_velocity() const {
	return physx_to_glm(get_dynamic_actor()->getLinearVelocity());
}

void PhysicsBody::set_shape_flags(PxShape* shape) {
	ASSERT(shape);
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

void PhysicsBody::add_model_shape_to_actor(const Model* model) {
	ASSERT(model);
	physx::PxMaterial* material_to_use = physics_local_impl->default_material;
	PhysicsMaterialWrapper* wrapper = this->material;
	if (!wrapper)
		wrapper = model->get_physics_material_to_use();
	if (wrapper) {
		material_to_use = wrapper->material;
	}
	ASSERT(material_to_use);

	if (model->get_physics_body()) {
		auto body = model->get_physics_body();
		for (int i = 0; i < body->shapes.size(); i++) {
			auto& shape = body->shapes[i];
			if (shape.shape == ShapeType_e::ConvexShape) {
				PxMeshScale scale;
				scale.scale = glm_to_physx(get_owner()->get_ls_scale());
				PxShape* aConvexShape = PxRigidActorExt::createExclusiveShape(
					*physxActor, PxConvexMeshGeometry(shape.convex_mesh, scale), *material_to_use);
				set_shape_flags(aConvexShape);
			} else if (shape.shape == ShapeType_e::MeshShape) {
				PxMeshScale scale;
				scale.scale = glm_to_physx(get_owner()->get_ls_scale());

				PxShape* tri_shape = PxRigidActorExt::createExclusiveShape(
					*physxActor, PxTriangleMeshGeometry(shape.tri_mesh, scale), *material_to_use);
				set_shape_flags(tri_shape);
			}
		}
	} else {
		auto& aabb = model->get_bounds();

		auto boxGeom = PxBoxGeometry(glm_to_physx((aabb.bmax - aabb.bmin) * 0.5f));

		const float MIN_WIDTH = 0.01f;
		boxGeom.halfExtents.x = glm::max(boxGeom.halfExtents.x, MIN_WIDTH);
		boxGeom.halfExtents.y = glm::max(boxGeom.halfExtents.y, MIN_WIDTH);
		boxGeom.halfExtents.z = glm::max(boxGeom.halfExtents.z, MIN_WIDTH);

		auto shape = PxRigidActorExt::createExclusiveShape(*physxActor, boxGeom, *material_to_use);

		auto middle = (aabb.bmax + aabb.bmin) * 0.5f;

		shape->setLocalPose(PxTransform(glm_to_physx(middle)));

		set_shape_flags(shape);
	}
}

void PhysicsBody::add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius) {
	ASSERT(radius > 0.f);
	PxMaterial* material_to_use = (this->material) ? this->material->material : physics_local_impl->default_material;
	ASSERT(material_to_use);

	auto capGeom = PxCapsuleGeometry(radius, height * 0.5 - radius);
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor, capGeom, *material_to_use);

	auto localpose = glm::translate(glm::mat4(1), base) * glm::mat4_cast(glm::angleAxis(HALFPI, glm::vec3(0, 0, 1)));

	shape->setLocalPose(glm_to_physx(localpose));
	set_shape_flags(shape);
}

void PhysicsBody::add_sphere_shape_to_actor(const glm::vec3& pos, float radius) {
	ASSERT(radius > 0.f);
	PxMaterial* material_to_use = (this->material) ? this->material->material : physics_local_impl->default_material;
	ASSERT(material_to_use);

	auto boxGeom = PxSphereGeometry(radius);
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor, boxGeom, *material_to_use);
	shape->setLocalPose(PxTransform(glm_to_physx(pos)));
	set_shape_flags(shape);
}

void PhysicsBody::add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents) {
	ASSERT(halfExtents.x > 0.f && halfExtents.y > 0.f && halfExtents.z > 0.f);
	PxMaterial* material_to_use = (this->material) ? this->material->material : physics_local_impl->default_material;
	ASSERT(material_to_use);

	auto boxGeom = PxBoxGeometry(glm_to_physx(halfExtents));
	auto shape = PxRigidActorExt::createExclusiveShape(*physxActor, boxGeom, *material_to_use);

	set_shape_flags(shape);
}

void PhysicsBody::set_is_simulating(bool simulate_physics) {
	ASSERT(get_owner());
	if (simulate_physics != this->simulate_physics) {
		this->simulate_physics = simulate_physics;
		if (has_initialized()) {
			if (get_is_actor_static()) {
				sys_print(Warning, "set_simulating set on a static PhysicsActor\n");
			} else {
				auto dyn = get_dynamic_actor();
				dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, !this->simulate_physics);

				next_position = get_owner()->get_ls_position();
				next_rot = get_owner()->get_ls_rotation();

				update_bone_parent_animator();
			}
		}
	}
}

void PhysicsBody::set_is_enable(bool enabled) {
	ASSERT(get_owner());
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

void PhysicsBody::set_physics_layer(PhysicsLayer l) {
	ASSERT(get_owner());
	if (l == physics_layer)
		return;
	physics_layer = l;
	if (has_initialized())
		refresh_shapes();
}

void PhysicsBody::refresh_shapes() {
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

void PhysicsBody::force_set_transform(const glm::mat4& m) {
	if (has_initialized()) {
		physxActor->setGlobalPose(glm_to_physx(m), true);
	}
}

static void remove_scale_mat4(glm::mat4& m) {
	vec3 right = vec3(m[0][0], m[1][0], m[2][0]);
	vec3 up = vec3(m[0][1], m[1][1], m[2][1]);
	vec3 fwd = vec3(m[0][2], m[1][2], m[2][2]);

	// Remove scale
	right = normalize(right);
	up = normalize(up);
	fwd = normalize(fwd);

	// Write back
	m[0][0] = right.x;
	m[1][0] = right.y;
	m[2][0] = right.z;
	m[0][1] = up.x;
	m[1][1] = up.y;
	m[2][1] = up.z;
	m[0][2] = fwd.x;
	m[1][2] = fwd.y;
	m[2][2] = fwd.z;
}

void PhysicsBody::set_transform(const glm::mat4& transform, bool teleport) {
	ASSERT(get_owner());
	if (has_initialized()) {
		if (simulate_physics || get_is_actor_static()) {
			glm::mat4 temp = transform;
			remove_scale_mat4(temp);
			auto t = glm_to_physx(temp);
			t.q.normalize();
			physxActor->setGlobalPose(t);
			if (simulate_physics) {
				set_angular_velocity({});
				set_linear_velocity({});
			}
		} else {
			auto dyn = get_dynamic_actor();
			auto t = glm_to_physx(transform);
			t.q.normalize();

			dyn->setKinematicTarget(t);
		}
	}
}

void PhysicsBody::set_is_trigger(bool is_trig) {
	if (is_trig == is_trigger)
		return;
	is_trigger = is_trig;
	if (has_initialized())
		refresh_shapes();
}
void PhysicsBody::set_send_overlap(bool send_overlap) {
	this->send_overlap = send_overlap;
}
void PhysicsBody::set_send_hit(bool send_hit) {
	this->send_hit = send_hit;
}
void PhysicsBody::set_is_static(bool is_static) {
	ASSERT(get_owner());
	if (this->is_static != is_static) {
		this->is_static = is_static;
		on_actor_type_change();
	}
}

MeshBuilderComponent* PhysicsBody::get_editor_meshbuilder() const {
	ASSERT(editor_shape_id != 0);
	return (MeshBuilderComponent*)eng->get_level()->get_entity(editor_shape_id);
}
