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
}


// wrapper around physx actors
class PhysicsJointComponent;
class MeshBuilderComponent;
class BillboardComponent;
class Model;

// hack bs to work with script fixme
struct PhysicsBodyEventArg {
	STRUCT_BODY();
	REF obj<Entity> who;
	REF bool entered_trigger = false;	// false = left
};
class IPhysicsEventCallback  : public ClassBase {
public:
	CLASS_BODY(IPhysicsEventCallback,scriptable);
	REF virtual void on_event(PhysicsBodyEventArg event) {}
};

class PhysicsBody : public Component
{
public:
	CLASS_BODY(PhysicsBody);

	PhysicsBody();
	~PhysicsBody();

	void pre_start() override;
	void start() override;
	void stop() override;
	void update() override;
	void on_changed_transform() override;

	REF bool get_is_kinematic() const { return !is_static && !simulate_physics; }
	void set_is_kinematic(bool kinematic);

	REF bool get_is_simulating() const { return simulate_physics; }
	REF void set_is_simulating(bool is_simulating);

	REF bool get_is_static() const { return is_static; }
	REF void set_is_static(bool is_static);

	REF bool get_is_enabled() const { return enabled; }
	REF void set_is_enable(bool enable);

	REF bool get_is_trigger() const { return is_trigger; }
	REF void set_is_trigger(bool is_trig);

	REF void set_send_overlap(bool send_overlap);
	REF void set_send_hit(bool send_hit);

	REF PL get_physics_layer() const {
		return physics_layer;
	}
	REF void set_physics_layer(PL l);

	glm::mat4 get_transform() const;

	// valid for dynamic actors only
	glm::vec3 get_linear_velocity() const;
	REF void set_linear_velocity(const glm::vec3& v);
	REF void set_angular_velocity(const glm::vec3& v);
	REF void apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse);
	REF void apply_force(const glm::vec3& worldspace, const glm::vec3& force);

	float get_mass() const;
	void set_objects_mass(float mass);
	void set_objects_density(float density);
	void set_transform(const glm::mat4& transform, bool teleport = false);

	void enable_with_initial_transforms(const glm::mat4& t0, const glm::mat4& t1, float dt);

	physx::PxRigidActor* get_physx_actor() const {
		return physxActor;
	}
	
	// allocate but DO NOT FREE IPhysicsEventCallback. ownership is taken
	// this is for lua code, c++ use on_trigger. bs fixme etc
	// 
	// see add_physics_callback and PhysicsEventCallbackImpl in lua for usage
	REF void add_triggered_callback(IPhysicsEventCallback* callback);

	MulticastDelegate<PhysicsBodyEventArg> on_trigger;
protected:
	void add_model_shape_to_actor(const Model* m);
	void add_sphere_shape_to_actor(const glm::vec3& pos, float radius);
	void add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius);
	void add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents);
	void update_mass();

	void on_shape_changes();

	MeshBuilderComponent* get_editor_meshbuilder() const;
private:
	void on_actor_type_change();

	void force_set_transform(const glm::mat4& m);

	void update_bone_parent_animator();

	void refresh_shapes();

	friend class PhysicsManImpl;
	void fetch_new_transform();

	// override this
	virtual void add_actor_shapes() {}
	virtual void add_editor_shapes() {}

	bool has_initialized() const { return physxActor != nullptr; }
	bool get_is_actor_static() const;
	physx::PxRigidDynamic* get_dynamic_actor() const;
	void set_shape_flags(physx::PxShape* shape);


	REF PL physics_layer = PL::Default;
	REF bool enabled = true;
	REF bool simulate_physics = false;		// if true, then object is a DYNAMIC object driven by the physics simulation
	REF bool is_static = true;				// if true, then the object is a STATIC object driven that cant ever move
		 									// if false, then this object is KINEMATIC if simulate_physics is false or DYNAMIC if its true
	 										// isStatic and simulate_physics is illogical so it falls back to isStatic in that case
	REF bool is_trigger = false;			// if true, then the objects shapes are treated like triggers and sends OVERLAP events
										// for a generic static trigger box, use with is_static = true
	REF bool send_overlap = false;			// if true on both objects, then a overlap event will be sent (one of the objects has to be a trigger object)
	REF bool send_hit = false;				// if true on both objects, then a hit event will be sent when the 2 objects hit each other in the simulation
	REF bool interpolate_visuals = true;
	REF float density = 2.0;

	physx::PxRigidActor* physxActor = nullptr;

	uint64_t editor_shape_id = 0;

	// will interpolate position if simulated
	glm::vec3 last_position = glm::vec3(0.f);
	glm::quat last_rot = glm::quat();
	glm::vec3 next_position = glm::vec3(0.f);
	glm::quat next_rot = glm::quat();

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
		return get_is_simulating() ? "eng/editor/phys_capsule_simulate.png" : "eng/editor/phys_capsule.png";
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
		return get_is_simulating() ? "eng/editor/phys_box_simulate.png" : "eng/editor/phys_box.png";
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
		return get_is_simulating() ? "eng/editor/phys_sphere_simulate.png" : "eng/editor/phys_sphere.png";
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
		return get_is_simulating() ? "eng/editor/phys_mesh_simulate.png" : "eng/editor/phys_mesh.png";
	}
#endif
};


struct JointAnchor
{
	STRUCT_BODY();
	REF glm::vec3 p = glm::vec3(0.f);
	REF glm::quat q = glm::quat();
};

class PhysicsJointComponent : public PhysicsBody
{
public:
	CLASS_BODY(PhysicsJointComponent);

	PhysicsJointComponent();
	~PhysicsJointComponent();

	void start() final;
	void stop() final;
	void on_changed_transform() override;
	void editor_on_change_property() override;

	void clear_joint() {
		set_target(nullptr);
	}
	Entity* get_target() {
		return target.get();
	}
	void set_target(Entity* e);

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/phys_joint.png";
	}
#endif
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
	REF int local_joint_axis = 0;	//0=x,1=y,2=z

	MeshBuilderComponent* editor_meshbuilder = nullptr;

private:

	void refresh_joint();
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


NEWENUM(JM,int8_t)
{
	Locked,
	Limited,
	Free,
};
using JointMotion = JM;


class AdvancedJointComponent : public PhysicsJointComponent
{
public:
	CLASS_BODY(AdvancedJointComponent);

	void init_joint(PhysicsBody* a, PhysicsBody* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;

	void draw_meshbuilder() override;

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
