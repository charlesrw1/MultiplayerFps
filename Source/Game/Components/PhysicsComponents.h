#pragma once

#include "Game/EntityComponent.h"
#include "Physics/ChannelsAndPresets.h"
#include "Framework/ClassTypePtr.h"
#include "Render/RenderObj.h"

class PhysicsActor;
class MeshBuilder;

namespace physx {
	class PxRigidActor;
	class PxShape;
	class PxRigidDynamic;
}

// wrapper around physx actors

class MeshBuilderComponent;
class BillboardComponent;
class Model;
CLASS_H(PhysicsComponentBase, EntityComponent)
public:
	PhysicsComponentBase();
	~PhysicsComponentBase();

	void on_init() override;
	void on_deinit() override;
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

	glm::mat4 get_transform() const;

	// valid for dynamic actors only
	glm::vec3 get_linear_velocity() const;
	void set_linear_velocity(const glm::vec3& v);
	void apply_impulse(const glm::vec3& worldspace, const glm::vec3& impulse);
	void set_objects_mass(float mass);
	void set_objects_density(float density);
	void set_transform(const glm::mat4& transform, bool teleport = false);

	static const PropertyInfoList* get_props() {
		START_PROPS(PhysicsComponentBase)
			REG_CLASSTYPE_PTR(physics_preset, PROP_DEFAULT),
			REG_BOOL(enabled, PROP_DEFAULT, "1"),
			REG_BOOL(simulate_physics, PROP_DEFAULT, "0"),
			REG_BOOL(is_trigger, PROP_DEFAULT, "0"),
			REG_BOOL(send_hit,PROP_DEFAULT,"0"),
			REG_BOOL(send_overlap,PROP_DEFAULT,"0"),
			REG_BOOL(is_static,PROP_DEFAULT,"1"),
		END_PROPS(PhysicsComponentBase)
	};
protected:
	void add_model_shape_to_actor(const Model* m);
	void add_sphere_shape_to_actor(const glm::vec3& pos, float radius);
	void add_vertical_capsule_to_actor(const glm::vec3& base, float height, float radius);
	void add_box_shape_to_actor(const glm::mat4& localTransform, const glm::vec3& halfExtents);
	void update_mass();


	MeshBuilderComponent* get_editor_meshbuilder() const;
private:
	// override this
	virtual void add_actor_shapes() {}
	virtual void add_editor_shapes() {}

	bool has_initialized() const { return physxActor != nullptr; }
	bool get_is_actor_static() const;
	physx::PxRigidDynamic* get_dynamic_actor() const;
	void set_shape_flags(physx::PxShape* shape);

	ClassTypePtr<PhysicsFilterPresetBase> physics_preset;
	bool enabled = true;
	
	bool simulate_physics = false;		// if true, then object is a DYNAMIC object driven by the physics simulation
	
	bool is_static = true;				// if true, then the object is a STATIC object driven that cant ever move
										// if false, then this object is KINEMATIC if simulate_physics is false or DYNAMIC if its true
										// isStatic and simulate_physics is illogical so it falls back to isStatic in that case

	bool is_trigger = false;			// if true, then the objects shapes are treated like triggers and sends OVERLAP events
										// for a generic static trigger box, use with is_static = true
	
	bool send_overlap = false;			// if true on both objects, then a overlap event will be sent (one of the objects has to be a trigger object)
	
	bool send_hit = false;				// if true on both objects, then a hit event will be sent when the 2 objects hit each other in the simulation

	physx::PxRigidActor* physxActor = nullptr;

	uint64_t editor_shape_id = 0;
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