#pragma once

#include "Game/EntityComponent.h"
#include "Physics/ChannelsAndPresets.h"
#include "Game/EntityPtr.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/MulticastDelegate.h"

class PhysicsActor;
class MeshBuilder;

namespace physx {
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
CLASS_H(PhysicsComponentBase, EntityComponent)
public:
	PhysicsComponentBase();
	~PhysicsComponentBase();

	void pre_start() override;
	void start() override;
	void end() override;
	void update() override;
	void on_changed_transform() override;

	bool get_is_kinematic() const { return !is_static && !simulate_physics; }
	void set_is_kinematic(bool kinematic);

	bool get_is_simulating() const { return simulate_physics; }
	void set_is_simulating(bool is_simulating);

	bool get_is_static() const { return is_static; }
	void set_is_static(bool is_static);

	bool get_is_enabled() const { return enabled; }
	void set_is_enable(bool enable);

	bool get_is_trigger() const { return is_trigger; }
	void set_is_trigger(bool is_trig);

	void set_send_overlap(bool send_overlap);
	void set_send_hit(bool send_hit);

	PhysicsLayer get_physics_layer() const {
		return physics_layer;
	}
	void set_physics_layer(PhysicsLayer l);

	glm::mat4 get_transform() const;

	// valid for dynamic actors only
	glm::vec3 get_linear_velocity() const;
	void set_linear_velocity(const glm::vec3& v);
	void set_angular_velocity(const glm::vec3& v);
	void apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse);
	void apply_force(const glm::vec3& worldspace, const glm::vec3& force);
	float get_mass() const;
	void set_objects_mass(float mass);
	void set_objects_density(float density);
	void set_transform(const glm::mat4& transform, bool teleport = false);

	void enable_with_initial_transforms(const glm::mat4& t0, const glm::mat4& t1, float dt);

	physx::PxRigidActor* get_physx_actor() const {
		return physxActor;
	}

	static const PropertyInfoList* get_props();

	// event delegates
	MulticastDelegate<PhysicsComponentBase*> on_trigger_start;
	MulticastDelegate<PhysicsComponentBase*> on_trigger_end;
	MulticastDelegate<PhysicsComponentBase*, glm::vec3 /* point */, glm::vec3/* normal */> on_collide;
protected:
	void add_model_shape_to_actor(const Model* m);
	void add_sphere_shape_to_actor(const glm::vec3& pos, float radius);
	void add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius);
	void add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents);
	void update_mass();


	MeshBuilderComponent* get_editor_meshbuilder() const;
private:
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

	PhysicsLayer physics_layer = PL::Default;
	bool enabled = true;
	
	bool simulate_physics = false;		// if true, then object is a DYNAMIC object driven by the physics simulation
	
	bool is_static = true;				// if true, then the object is a STATIC object driven that cant ever move
										// if false, then this object is KINEMATIC if simulate_physics is false or DYNAMIC if its true
										// isStatic and simulate_physics is illogical so it falls back to isStatic in that case

	bool is_trigger = false;			// if true, then the objects shapes are treated like triggers and sends OVERLAP events
										// for a generic static trigger box, use with is_static = true
	
	bool send_overlap = false;			// if true on both objects, then a overlap event will be sent (one of the objects has to be a trigger object)
	
	bool send_hit = false;				// if true on both objects, then a hit event will be sent when the 2 objects hit each other in the simulation

	bool interpolate_visuals = true;

	float density = 2.0;

	physx::PxRigidActor* physxActor = nullptr;

	uint64_t editor_shape_id = 0;

	// will interpolate position if simulated
	glm::vec3 last_position = glm::vec3(0.f);
	glm::quat last_rot = glm::quat();
	glm::vec3 next_position = glm::vec3(0.f);
	glm::quat next_rot = glm::quat();

	friend class PhysicsJointComponent;
};


CLASS_H(CapsuleComponent, PhysicsComponentBase)
public:
	~CapsuleComponent() override {}

	void add_actor_shapes() override;
	void add_editor_shapes() override;

	static const PropertyInfoList* get_props() {
		START_PROPS(CapsuleComponent)
			REG_FLOAT(height,PROP_DEFAULT,"2.0"),
			REG_FLOAT(radius,PROP_DEFAULT,"0.5"),
			REG_FLOAT(height_offset,PROP_DEFAULT,"0.0"),
		END_PROPS(CapsuleComponent)
	};

	float height = 2.f;
	float radius = 0.5;
	float height_offset = 0.0;
};
CLASS_H(BoxComponent, PhysicsComponentBase)
public:
	~BoxComponent() override {}
	void add_actor_shapes() override;
	void add_editor_shapes() override;

	static const PropertyInfoList* get_props() = delete;
};
CLASS_H(SphereComponent, PhysicsComponentBase)
public:
	~SphereComponent() override {}
	void add_actor_shapes() override;
	void add_editor_shapes() override;

	static const PropertyInfoList* get_props() {
		START_PROPS(SphereComponent)
			REG_FLOAT(radius, PROP_DEFAULT, "1.0"),
		END_PROPS(SphereComponent)
	};
	float radius = 1.f;
};
CLASS_H(MeshColliderComponent, PhysicsComponentBase)
public:
	void add_actor_shapes() override;

	static const PropertyInfoList* get_props() = delete;
};

CLASS_H(CompoundColliderComponent, PhysicsComponentBase)
public:

};

struct JointAnchor
{
	glm::quat q = glm::quat();
	glm::vec3 p = glm::vec3(0.f);
};

CLASS_H(PhysicsJointComponent, EntityComponent)
public:
	PhysicsJointComponent();
	~PhysicsJointComponent();

	void start() override;
	void end() override;
	void on_changed_transform() override;
	void editor_on_change_property() override;

	static const PropertyInfoList* get_props();

	void clear_joint() {
		set_target(nullptr);
	}
	Entity* get_target() {
		return target.get();
	}
	void set_target(Entity* e);
protected:

	PhysicsComponentBase* get_owner_physics();

	virtual void init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b) = 0;
	virtual physx::PxJoint* get_joint() const = 0;
	virtual void free_joint() = 0;
	virtual void draw_meshbuilder();

	float limit_spring = 0.f;
	float limit_damping = 0.f;
	
	JointAnchor anchor;

	int local_joint_axis = 0;	//0=x,1=y,2=z

	MeshBuilderComponent* editor_meshbuilder = nullptr;

	EntityPtr target;
private:

	void refresh_joint();
};

CLASS_H(HingeJointComponent, PhysicsJointComponent)
public:

private:
	void init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;


	static const PropertyInfoList* get_props() = delete;

	float limit_min = 0.f;
	float limit_max = 0.f;
	physx::PxRevoluteJoint* joint = nullptr;
};
CLASS_H(BallSocketJointComponent,PhysicsJointComponent)
public:
	void init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;

	static const PropertyInfoList* get_props() = delete;
	physx::PxSphericalJoint* joint = nullptr;
};

enum class JM : uint8_t {
	Locked,
	Limited,
	Free,
};
using JointMotion = JM;
ENUM_HEADER(JointMotion);

CLASS_H(AdvancedJointComponent, PhysicsJointComponent)
public:
	void init_joint(PhysicsComponentBase* a, PhysicsComponentBase* b) override;
	physx::PxJoint* get_joint() const override;
	void free_joint() override;
	static const PropertyInfoList* get_props() {
		START_PROPS(AdvancedJointComponent)
			REG_ENUM(x_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_ENUM(y_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_ENUM(z_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_ENUM(ang_x_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_ENUM(ang_y_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_ENUM(ang_z_motion,PROP_DEFAULT,"JM::Locked", JM),
			REG_FLOAT(linear_distance_max,PROP_DEFAULT,"0"),
			REG_FLOAT(linear_damp,PROP_DEFAULT,"0"),
			REG_FLOAT(linear_stiff,PROP_DEFAULT,"0"),
			REG_FLOAT(twist_limit_min,PROP_DEFAULT,"0"),
			REG_FLOAT(twist_limit_max,PROP_DEFAULT,"0"),
			REG_FLOAT(ang_y_limit,PROP_DEFAULT,"0"),
			REG_FLOAT(ang_z_limit,PROP_DEFAULT,"0"),
			REG_FLOAT(twist_damp,PROP_DEFAULT,"0"),
			REG_FLOAT(twist_stiff,PROP_DEFAULT,"0"),
			REG_FLOAT(cone_damp,PROP_DEFAULT,"0"),
			REG_FLOAT(cone_stiff,PROP_DEFAULT,"0"),
		END_PROPS(AdvancedJointComponent)
	}
	void draw_meshbuilder() override;

	JointMotion x_motion = JM::Locked;
	JointMotion y_motion = JM::Locked;
	JointMotion z_motion = JM::Locked;
	JointMotion ang_x_motion = JM::Locked;
	JointMotion ang_y_motion = JM::Locked;
	JointMotion ang_z_motion = JM::Locked;
	float linear_distance_max = 0.0;
	float linear_damp = 0.0;
	float linear_stiff = 0.0;
	float twist_limit_min = 0.0;
	float twist_limit_max = 0.0;
	float ang_y_limit = 0.0;
	float ang_z_limit = 0.0;
	float twist_damp = 0.0;
	float twist_stiff = 0.0;
	float cone_damp = 0.0;
	float cone_stiff = 0.0;


	physx::PxD6Joint* joint = nullptr;
};
CLASS_H(DistanceJointComponent, PhysicsJointComponent)
public:
};
CLASS_H(FixedJointComponent, PhysicsJointComponent)
public:
};