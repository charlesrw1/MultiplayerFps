#pragma once

#include "Game/EntityComponent.h"
#include "Physics/ChannelsAndPresets.h"
#include "Game/EntityPtr.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/MulticastDelegate.h"
#include "Framework/Reflection2.h"
#include "Framework/StructReflection.h"

class PhysicsActor;
class MeshBuilder;

/* blank */ namespace physx {
class PxRigidActor;
class PxShape;
class PxRigidDynamic;
class PxJoint;
class PxRevoluteJoint;
class PxSphericalJoint;
class PxD6Joint;
} // namespace physx
class PhysicsMaterialWrapper;

// wrapper around physx actors
class PhysicsJointComponent;
class MeshBuilderComponent;
class BillboardComponent;
class Model;

// Canonical encoding of "what kind of physics object is this?".
// Static    — RigidStatic; immovable, scene-query only.
// Kinematic — RigidDynamic with eKINEMATIC; moved by user code, not the simulation.
// Dynamic   — RigidDynamic; driven by the simulation (gravity, forces, joints).
// Orthogonal: PhysicsBody::set_is_enable() gates whether the actor ticks at all.
NEWENUM(BodyType, uint8_t){Static, Kinematic, Dynamic};

class PhysicsBody : public Component
{
public:
	CLASS_BODY(PhysicsBody);

	PhysicsBody();
	~PhysicsBody();

	void start() override;
	void stop() override;
	void update() override;
	void on_changed_transform() override;

	// ── Transform ownership model (READ THIS before touching transforms) ──
	// A PhysicsBody has ONE of two relationships with its Entity's transform,
	// selected by get_body_type():
	//
	//   Static / Kinematic : the ENTITY drives the body. Move the Entity
	//       (Entity::set_ws_transform) and the body follows -- Static snaps
	//       instantly, Kinematic sweeps toward the target (generates contacts).
	//   Dynamic (simulating): PHYSX drives the Entity. Each simulation step the
	//       solver writes the resulting pose back into the Entity transform, so
	//       you READ a Dynamic body via entity->get_ws_transform(). Do NOT push
	//       Entity transforms into a Dynamic body every frame -- it fights the
	//       solver. To reposition one, call teleport_to().
	//
	// Unity/Unreal cheat-sheet in docs/physics/transforms.md.

	// --- Canonical body-type API. Prefer these in C++. ---
	REF BodyType get_body_type() const { return body_type; }
	REF void set_body_type(BodyType t);

	// `enabled` is orthogonal to body type: it gates whether the actor ticks at
	// all. (set_is_enable kept as the real setter -- there is no BodyType for it.)
	REF bool get_is_enabled() const { return enabled; }
	REF void set_is_enable(bool enable);

	REF bool get_is_trigger() const { return is_trigger; }
	REF void set_is_trigger(bool is_trig);

	// Opt this body into simulation contact events. When set, a collision against
	// another body fires Component::on_hit on this entity's components. Cheap to
	// leave off: PhysX only reports contacts for pairs where a body opts in.
	REF void set_send_hit(bool send_hit);
	REF bool get_send_hit() const { return send_hit; }

	REF PL get_physics_layer() const { return physics_layer; }
	REF void set_physics_layer(PL l);

	// --- Transform. Intent is explicit; neither call ever mutates velocity. ---
	// Instant reposition (PhysX setGlobalPose). Valid for any body type; keeps
	// current velocity. Unity Rigidbody.position / Unreal SetWorldLocation(Teleport).
	REF void teleport_to(const glm::mat4& world_transform);
	// Swept move for KINEMATIC bodies (PhysX setKinematicTarget) -- generates
	// contacts along the path. Asserts if the body is not Kinematic. Unity MovePosition.
	REF void move_to(const glm::mat4& world_transform);
	// PhysX world pose (position+rotation only, NO scale). For a Dynamic body you
	// usually want entity->get_ws_transform() instead -- physics writes it there.
	glm::mat4 get_physics_pose() const;

	// --- Velocity / forces (dynamic actors only) ---
	REF glm::vec3 get_linear_velocity() const;
	REF void set_linear_velocity(const glm::vec3& v);
	REF void set_angular_velocity(const glm::vec3& v);
	REF void apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse);
	REF void apply_force(const glm::vec3& worldspace, const glm::vec3& force);

	REF float get_mass() const;
	// Mass authoring — two mutually exclusive modes for a Dynamic body:
	//   density mode (default): mass/inertia derived from shape volume * density.
	//   explicit-mass mode:     total mass is fixed; inertia is derived from the
	//                           shape distribution scaled to hit that mass.
	// The last setter called wins. Both recompute on the next shape change.
	REF void set_density(float d) {
		this->density = d;
		this->use_explicit_mass = false;
		on_shape_changes();
	}
	REF void set_mass(float m) {
		this->mass = m;
		this->use_explicit_mass = true;
		on_shape_changes();
	}
	// Overrides the local-space center of mass. Applies on top of either mass mode.
	// Without this, PhysX computes the COM from the shape distribution.
	REF void set_center_of_gravity(const glm::vec3& local_com) {
		this->center_of_gravity = local_com;
		this->override_center_of_gravity = true;
		on_shape_changes();
	}
	// Reverts to the PhysX-computed center of mass (undoes set_center_of_gravity).
	REF void clear_center_of_gravity() {
		this->override_center_of_gravity = false;
		on_shape_changes();
	}
	REF glm::vec3 get_center_of_gravity() const;

	void enable_with_initial_transforms(const glm::mat4& t0, const glm::mat4& t1, float dt);


	physx::PxRigidActor* get_physx_actor() const { return physxActor; }

	// Introspection (mostly for tests / asserts): read the underlying PhysX actor's
	// type, not the configured field. These can diverge before apply_actor_config
	// runs, so use these — not get_body_type() — to check what physics actually sees.
	bool get_is_actor_static() const;
	bool get_is_actor_kinematic() const;

	// doesnt take ownership, physics materials have forever lifetimes, see class PhysicsMaterialWrapper
	REF void set_material(PhysicsMaterialWrapper* material) {
		this->material = material;
		on_shape_changes();
	}

protected:
	void add_model_shape_to_actor(const Model* m);
	void add_sphere_shape_to_actor(const glm::vec3& pos, float radius);
	void add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius);
	void add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents);
	void update_mass();

	void on_shape_changes();

	MeshBuilderComponent* get_editor_meshbuilder() const;

private:
	// Single funnel: reconciles physxActor with (enabled, body_type).
	// Rebuilds the actor when static<->dynamic, otherwise just toggles flags. Call
	// after mutating body_type or enabled.
	void apply_actor_config();
	void on_actor_type_change();
	void refresh_shapes();
	virtual void add_actor_shapes() {}
	void set_shape_flags(physx::PxShape* shape);
	
	virtual void add_editor_shapes() {}

	void update_bone_parent_animator();

	friend class PhysicsManImpl;
	void fetch_new_transform();

	// override this

	bool has_initialized() const { return physxActor != nullptr; }
	physx::PxRigidDynamic* get_dynamic_actor() const;

	PL physics_layer = PL::Default;
	bool enabled = true;
	// Static / Kinematic / Dynamic. See get_body_type() and the ownership model above.
	BodyType body_type = BodyType::Static;
	bool is_trigger = false; // if true, then the objects shapes are treated like triggers and sends OVERLAP events
								 // for a generic static trigger box, use with body_type = Static

	bool send_hit = false;	   // if true on both objects, then a hit event will be sent when the 2 objects hit each
								   // other in the simulation

	float density = 2.0; // used when use_explicit_mass == false
	float mass = 1.0;	 // used when use_explicit_mass == true
	bool use_explicit_mass = false;
	// Local-space center-of-mass override, active only when override_center_of_gravity is set.
	glm::vec3 center_of_gravity = glm::vec3(0.f);
	bool override_center_of_gravity = false;

	physx::PxRigidActor* physxActor = nullptr;
	PhysicsMaterialWrapper* material = nullptr;

	uint64_t editor_shape_id = 0;

	friend class PhysicsJointComponent;
};

class CapsuleComponent : public PhysicsBody
{
public:
	CLASS_BODY(CapsuleComponent);

	~CapsuleComponent() override {}

	void add_actor_shapes() override;
	void add_editor_shapes() override;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return get_body_type() == BodyType::Dynamic ? "eng/editor/phys_capsule_simulate.png" : "eng/editor/phys_capsule.png";
	}
#endif
	REF void set_data(float height, float radius, float height_ofs) {
		this->height = height;
		this->radius = radius;
		this->height_offset = height_ofs;
		on_shape_changes();
	}

	REF float height = 2.f;
	REF float radius = 0.5;
	REF float height_offset = 0.0;
};

class BoxComponent : public PhysicsBody
{
public:
	CLASS_BODY(BoxComponent);
	~BoxComponent() override {}
	void add_actor_shapes() override;
	void add_editor_shapes() override;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return get_body_type() == BodyType::Dynamic ? "eng/editor/phys_box_simulate.png" : "eng/editor/phys_box.png";
	}
#endif
};
class SphereComponent : public PhysicsBody
{
public:
	CLASS_BODY(SphereComponent);
	~SphereComponent() override {}
	void add_actor_shapes() override;
	void add_editor_shapes() override;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return get_body_type() == BodyType::Dynamic ? "eng/editor/phys_sphere_simulate.png" : "eng/editor/phys_sphere.png";
	}
#endif

	REF void set_radius(float r) {
		this->radius = r;
		on_shape_changes();
	}

	REFLECT();
	float radius = 1.f;
};

class MeshColliderComponent : public PhysicsBody
{
public:
	CLASS_BODY(MeshColliderComponent);
	MeshColliderComponent() {}
	void add_actor_shapes() override;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return get_body_type() == BodyType::Dynamic ? "eng/editor/phys_mesh_simulate.png" : "eng/editor/phys_mesh.png";
	}
#endif
};

// Bake a static mesh-collider RigidStatic without owning a PhysicsBody Component.
// Used by Level's static-prop strip path. Returns nullptr when the model has no usable
// collision (matches `add_collision_if_available && model->get_physics_body()` semantics).
// userData on the returned actor is set to nullptr — world_query_result::component will be
// null for raycasts that hit a stripped prop.
// Caller owns the actor and must release via release_static_meshcomponent_physics().
physx::PxRigidActor* bake_static_meshcomponent_physics(const Model* model, const glm::mat4& ws_transform);
void release_static_meshcomponent_physics(physx::PxRigidActor* actor);

struct JointAnchor
{
	STRUCT_BODY();
	REF glm::vec3 p = glm::vec3(0.f);
	REF glm::quat q = glm::quat();
};

class PhysicsJointComponent : public Component
{
public:
	CLASS_BODY(PhysicsJointComponent);

	PhysicsJointComponent();
	~PhysicsJointComponent();

	void start() final;
	void stop() final;
	void on_changed_transform() override;
#ifdef EDITOR_BUILD
	void editor_on_change_property() override;
#endif
	void clear_joint() { set_target(nullptr); }
	REF Entity* get_target() { return target.get(); }
	REF void set_target(Entity* e);

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/phys_joint.png"; }
#endif

	REF void set_joint_anchor(glm::vec3 p, glm::quat q, int axis) {
		anchor.p = p;
		anchor.q = q;
		if (axis < 0 || axis > 2)
			axis = 0;
		local_joint_axis = axis;
	}

	// Extra local-space offset applied ONLY to the target/world side of the joint (never to this
	// component's own attached body frame -- see make_joint_shared in PhysicsJoints.cpp). Use this
	// to bias where a swing cone's limits are centered: PxD6 swing limits are always symmetric
	// around whatever the joint's own attached-body frame is, so baking a bias rotation into
	// `anchor` instead gets conjugated away for any bias that shares an axis with the swing itself
	// (rotations about the same axis commute, so the bias cancels out of the conjugation exactly).
	// Putting the SAME bias here instead -- multiplied onto the world/other-actor side only --
	// shifts the limit's center for real, without that cancellation. Defaults to identity (no-op).
	REF void set_target_anchor(glm::vec3 p, glm::quat q) {
		target_anchor.p = p;
		target_anchor.q = q;
	}

	// call after changing stuff
	REF void refresh_joint();

protected:
	PhysicsBody* get_owner_physics();

	virtual void init_joint(PhysicsBody* a, PhysicsBody* b) = 0;
	virtual physx::PxJoint* get_joint() const = 0;
	virtual void free_joint() = 0;
	virtual void draw_meshbuilder();

	float limit_spring = 0.f;
	float limit_damping = 0.f;

	REF obj<Entity> target;
	REF JointAnchor anchor;
	REF JointAnchor target_anchor; // identity by default -- see set_target_anchor()
	REF int local_joint_axis = 0; // 0=x,1=y,2=z

	MeshBuilderComponent* editor_meshbuilder = nullptr;

private:
};

class HingeJointComponent : public PhysicsJointComponent
{
public:
	CLASS_BODY(HingeJointComponent);

private:
	void init_joint(PhysicsBody* a, PhysicsBody* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;

	float limit_min = 0.f;
	float limit_max = 0.f;
	physx::PxRevoluteJoint* joint = nullptr;
};

class BallSocketJointComponent : public PhysicsJointComponent
{
public:
	CLASS_BODY(BallSocketJointComponent);

	void init_joint(PhysicsBody* a, PhysicsBody* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;

	physx::PxSphericalJoint* joint = nullptr;
};

NEWENUM(JM, int8_t){
	Locked,
	Limited,
	Free,
};
using JointMotion = JM;

class AdvancedJointComponent : public PhysicsJointComponent
{
public:
	CLASS_BODY(AdvancedJointComponent);

	void init_joint(PhysicsBody* a, PhysicsBody* b) final;
	physx::PxJoint* get_joint() const final;
	void free_joint() final;
	void draw_meshbuilder() final;

	REF void set_translate_joint_motion(JM x, JM y, JM z) {
		x_motion = x;
		y_motion = y;
		z_motion = z;
	}
	REF void set_rotation_joint_motion(JM x, JM y, JM z) {
		ang_x_motion = x;
		ang_y_motion = y;
		ang_z_motion = z;
	}
	REF void set_twist_vars(float twist_min, float twist_max, float twist_damp, float twist_stiff) {
		this->twist_limit_min = twist_min;
		this->twist_limit_max = twist_max;
		this->twist_damp = twist_damp;
		this->twist_stiff = twist_stiff;
	}
	REF void set_cone_vars(float ang_y_limit, float ang_z_limit, float cone_damp, float cone_stiff) {
		this->ang_y_limit = ang_y_limit;
		this->ang_z_limit = ang_z_limit;
		this->cone_damp = cone_damp;
		this->cone_stiff = cone_stiff;
	}

	REF JM x_motion = JM::Locked;
	REF JM y_motion = JM::Locked;
	REF JM z_motion = JM::Locked;
	REF JM ang_x_motion = JM::Locked;
	REF JM ang_y_motion = JM::Locked;
	REF JM ang_z_motion = JM::Locked;
	REF float linear_distance_max = 0.0;
	REF float linear_damp = 0.0;
	REF float linear_stiff = 0.0;
	REF float twist_limit_min = 0.0;
	REF float twist_limit_max = 0.0;
	REF float ang_y_limit = 0.0;
	REF float ang_z_limit = 0.0;
	REF float twist_damp = 0.0;
	REF float twist_stiff = 0.0;
	REF float cone_damp = 0.0;
	REF float cone_stiff = 0.0;

	physx::PxD6Joint* joint = nullptr;
};
