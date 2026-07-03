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

// Called once per sim step for each Dynamic body that moved: PhysX drives the
// Entity, so we write the solver's pose back into the owner transform. The
// resulting on_changed_transform is a no-op for Dynamic bodies (see ownership
// model in the header), so this does not feed back into the solver.
void PhysicsBody::fetch_new_transform() {
	ASSERT(get_body_type() == BodyType::Dynamic);
	ASSERT(has_initialized());
	auto pose = physxActor->getGlobalPose();
	get_owner()->set_ws_transform(physx_to_glm(pose.p), physx_to_glm(pose.q), get_owner()->get_ls_scale());
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
	ASSERT(has_initialized());        // actor must exist; configure shape/type before enabling
	ASSERT(!get_is_actor_static());              // velocities only meaningful on a dynamic actor
	ASSERT(get_body_type() == BodyType::Dynamic); // kinematic bodies don't track velocity
	set_is_enable(true);
	auto rot0 = glm::quat_cast(t0);
	auto rot1 = glm::quat_cast(t1);
	glm::vec3 lin_vel = (glm::vec3(t1[3]) - glm::vec3(t0[3])) / dt;
	glm::vec3 ang_vel = calc_angular_vel(rot0, rot1, dt);
	set_linear_velocity(lin_vel);
	set_angular_velocity(ang_vel);
	get_owner()->set_is_top_level(true);
	get_owner()->set_ws_transform(t1);
	// Entity is now top-level Dynamic, so the set_ws_transform above is ignored by
	// physics -- push the pose in explicitly.
	teleport_to(t1);
}

void PhysicsBody::update() {
	// interpolation is currently disabled (see fetch_new_transform `if (0 && ...)`).
	// keep ticking off until that path is rebuilt.
	set_ticking(false);
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
	if (body_type == BodyType::Static) {
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
		dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, body_type != BodyType::Dynamic);
	}

	physics_local_impl->scene->addActor(*physxActor);
	physxActor->userData = this;
	physxActor->setActorFlag(physx::PxActorFlag::eSEND_SLEEP_NOTIFIES, true);
	physxActor->setActorFlag(physx::PxActorFlag::eDISABLE_SIMULATION, !enabled);

	assert(has_initialized());

	on_shape_changes();

	// Any joint that already references the released actor must be rebuilt against
	// the new one. We only walk this entity's own joints — joints on OTHER entities
	// that target this body are not refreshed here (TODO: needs reverse lookup).
	// refresh_joint() is idempotent: free_joint handles a null joint, so it is safe
	// to call even on joints whose own start() hasn't run yet.
	for (auto* c : get_owner()->get_components()) {
		if (c && c->is_a<PhysicsJointComponent>()) {
			((PhysicsJointComponent*)c)->refresh_joint();
		}
	}
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

		if (get_body_type() == BodyType::Dynamic) {
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
	// Don't reset configured fields here — a re-start() should pick up whatever
	// was configured (matches behaviour of body_type / enabled which are also kept).
	update_bone_parent_animator();
}

void PhysicsBody::update_mass() {
	ASSERT(physxActor);
	if (!get_is_actor_static()) {
		auto dyn = (PxRigidDynamic*)physxActor;
		// Kinematic bodies are position-driven — mass/inertia are irrelevant.
		// Triangle mesh shapes also have no volume so updateMassAndInertia would
		// divide by zero. Both cases are covered by skipping when not simulating.
		if (body_type != BodyType::Dynamic) return;
		if (dyn->getNbShapes() == 0) return;
		// Optional local-space center-of-mass override; nullptr lets PhysX compute it.
		PxVec3 com;
		const PxVec3* com_ptr = nullptr;
		if (override_center_of_gravity) {
			com = glm_to_physx(center_of_gravity);
			com_ptr = &com;
		}
		if (use_explicit_mass)
			PxRigidBodyExt::setMassAndUpdateInertia(*dyn, mass, com_ptr);
		else
			PxRigidBodyExt::updateMassAndInertia(*dyn, density, com_ptr);
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

glm::vec3 PhysicsBody::get_center_of_gravity() const {
	ASSERT(physxActor);
	if (!get_is_actor_static() && physxActor) {
		auto dyn = (PxRigidDynamic*)physxActor;
		return physx_to_glm(dyn->getCMassLocalPose().p);
	}
	return glm::vec3(0.f);
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
	// Push the Entity transform into physics per the ownership model (see header).
	// A Dynamic body is driven BY physics, so an external Entity move is ignored
	// here -- reposition it with teleport_to() instead.
	switch (get_body_type()) {
	case BodyType::Static:    teleport_to(get_ws_transform()); break;
	case BodyType::Kinematic: move_to(get_ws_transform());     break;
	case BodyType::Dynamic:   break; // physics owns the transform
	}
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

bool PhysicsBody::get_is_actor_kinematic() const {
	if (!physxActor || get_is_actor_static())
		return false;
	auto dyn = (physx::PxRigidDynamic*)physxActor;
	return dyn->getRigidBodyFlags().isSet(physx::PxRigidBodyFlag::eKINEMATIC);
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

glm::mat4 PhysicsBody::get_physics_pose() const {
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
	filter.word1 = g_physics.get_collision_mask_for_layer((int)physics_layer);
	filter.word2 = send_hit ? 1u : 0u; // request contact-notify for this pair (see my_filter_shader)
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

// Mirrors the slice of (PhysicsBody::on_actor_type_change + add_model_shape_to_actor)
// needed for a static mesh prop. Default physics layer = PL::Default; not configurable
// because eligibility intentionally excludes anything that would care.
physx::PxRigidActor* bake_static_meshcomponent_physics(const Model* model, const glm::mat4& ws_transform) {
	ASSERT(model);
	if (!model->get_physics_body())
		return nullptr; // no usable collision; matches MeshColliderComponent's "skip if no body"

	glm::vec3 p, s; glm::quat q;
	decompose_transform(ws_transform, p, q, s);
	PxTransform t;
	t.p = glm_to_physx(p);
	t.q = glm_to_physx(q);
	t.q.normalize();

	auto* factory = physics_local_impl->physics_factory;
	PxRigidStatic* actor = factory->createRigidStatic(t);
	actor->userData = nullptr;          // see header comment — null on purpose
	actor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, false);

	PxMaterial* material_to_use = physics_local_impl->default_material;
	if (auto* wrapper = model->get_physics_material_to_use())
		material_to_use = wrapper->material;
	ASSERT(material_to_use);

	PxFilterData filter;
	filter.word0 = (1ul << uint32_t(PL::Default));
	filter.word1 = g_physics.get_collision_mask_for_layer((int)PL::Default);

	auto* body = model->get_physics_body();
	for (auto& shape : body->shapes) {
		PxShape* px_shape = nullptr;
		PxMeshScale scale;
		scale.scale = glm_to_physx(s);
		if (shape.shape == ShapeType_e::ConvexShape) {
			px_shape = PxRigidActorExt::createExclusiveShape(
				*actor, PxConvexMeshGeometry(shape.convex_mesh, scale), *material_to_use);
		} else if (shape.shape == ShapeType_e::MeshShape) {
			px_shape = PxRigidActorExt::createExclusiveShape(
				*actor, PxTriangleMeshGeometry(shape.tri_mesh, scale), *material_to_use);
		}
		if (px_shape) {
			px_shape->setFlags(PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eVISUALIZATION);
			px_shape->setQueryFilterData(filter);
			px_shape->setSimulationFilterData(filter);
		}
	}

	physics_local_impl->scene->addActor(*actor);
	return actor;
}

void release_static_meshcomponent_physics(physx::PxRigidActor* actor) {
	if (!actor)
		return;
	physics_local_impl->scene->removeActor(*(physx::PxActor*)actor);
	actor->release();
}

void PhysicsBody::set_body_type(BodyType t) {
	ASSERT(get_owner());
	if (t == body_type)
		return;
	body_type = t;
	apply_actor_config();
}

void PhysicsBody::set_is_enable(bool enabled) {
	ASSERT(get_owner());
	// enabled is orthogonal to body type — funnel directly through apply_actor_config.
	if (enabled == this->enabled)
		return;
	this->enabled = enabled;
	apply_actor_config();
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

// Instant reposition for any body type. Preserves velocity (no hidden zeroing).
// Scale is stripped: PhysX poses are rigid, and a scaled matrix would corrupt
// the rotation quaternion. Shapes keep the scale baked at creation time.
void PhysicsBody::teleport_to(const glm::mat4& world_transform) {
	ASSERT(get_owner());
	if (!has_initialized())
		return;
	glm::mat4 temp = world_transform;
	remove_scale_mat4(temp);
	auto t = glm_to_physx(temp);
	t.q.normalize();
	physxActor->setGlobalPose(t, /*autowake*/ true);
}

// Swept move toward the target for a Kinematic body (generates contacts along
// the path). Only valid on a Kinematic body -- Static/Dynamic must use teleport_to.
void PhysicsBody::move_to(const glm::mat4& world_transform) {
	ASSERT(get_owner());
	ASSERT(get_body_type() == BodyType::Kinematic &&
		   "move_to() is Kinematic-only; use teleport_to() for Static/Dynamic bodies");
	if (!has_initialized())
		return;
	glm::mat4 temp = world_transform;
	remove_scale_mat4(temp);
	auto t = glm_to_physx(temp);
	t.q.normalize();
	get_dynamic_actor()->setKinematicTarget(t);
}

void PhysicsBody::set_is_trigger(bool is_trig) {
	if (is_trig == is_trigger)
		return;
	is_trigger = is_trig;
	if (has_initialized())
		refresh_shapes();
}

void PhysicsBody::set_send_hit(bool send_hit) {
	if (send_hit == this->send_hit)
		return;
	this->send_hit = send_hit;
	// Filter data carries the opt-in bit; re-push it so PhysX re-filters existing pairs.
	if (has_initialized())
		refresh_shapes();
}
void PhysicsBody::apply_actor_config() {
	ASSERT(get_owner());
	if (!has_initialized()) {
		// Pre-start: fields are picked up by start() -> on_actor_type_change().
		return;
	}
	// Top-level mirrors "this body drives the entity transform" (Dynamic only).
	const bool drives_transform = enabled && body_type == BodyType::Dynamic;
	const bool want_static = (body_type == BodyType::Static);
	const bool have_static = get_is_actor_static();
	if (want_static != have_static) {
		// Actor kind change (Static <-> RigidDynamic) requires a full rebuild.
		// on_actor_type_change reads body_type to set eKINEMATIC and eDISABLE_SIMULATION,
		// and refreshes joints attached to this entity.
		on_actor_type_change();
		get_owner()->set_is_top_level(drives_transform);
		update_bone_parent_animator();
		return;
	}
	// Same actor kind: cheap in-place flag updates.
	physxActor->setActorFlag(PxActorFlag::eDISABLE_SIMULATION, !enabled);
	if (!have_static) {
		auto dyn = (PxRigidDynamic*)physxActor;
		dyn->setRigidBodyFlag(PxRigidBodyFlag::eKINEMATIC, body_type != BodyType::Dynamic);
	}
	get_owner()->set_is_top_level(drives_transform);
	update_bone_parent_animator();
}

MeshBuilderComponent* PhysicsBody::get_editor_meshbuilder() const {
	ASSERT(editor_shape_id != 0);
	return (MeshBuilderComponent*)eng->get_level()->get_entity(editor_shape_id);
}
