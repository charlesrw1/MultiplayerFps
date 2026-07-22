#pragma once

#include <glm/glm.hpp>
#include "Game/EntityComponent.h"
#include "Game/Components/PhysicsComponents.h"

// A collide+slide character controller that can be hooked up with players/ai

// bitmask for what controller collided with
enum CharacterControllerCollisionFlags
{
	CCCF_ABOVE = 1,
	CCCF_BELOW = 2,
	CCCF_SIDES = 4
};
class PhysicsBody;
class CharacterController
{
public:
	CharacterController(PhysicsBody* self) : self(self) {}

	// continuous move
	void move(const glm::vec3& displacement, // how far to move the character this time step (velocity*dt)
			  float dt,						 // time step
			  float min_dist,				 // min dist to move
			  int& out_ccfg_flags,			 // bitmask of CharacterControllerCollisionFlags
			  glm::vec3& out_velocity // internally, the Controller doesnt store velocity, out_velocity is just a useful
									  // return of what the characters velocity should be after sliding along surfaces
									  // by clipping the in velocity ie in_velocity = displacement/dt. If no collision,
									  // then out_velocity = in_velocity the caller can use this value how they want
	);

	// teleport move
	void set_position(const glm::vec3& v) { position = v; }

	const glm::vec3& get_character_pos() const { return position; }

	// parameters
	float capsule_height = 2.f;
	float capsule_radius = 0.25f;
	float step_height = 0.1;
	float skin_size = 0.05;

	void set_physics_body(PhysicsBody* self) { this->self = self; }

private:
	PhysicsBody* self = nullptr;
	int cached_flags = 0;
	glm::vec3 position{}; // internal position
};

// Alternative to CharacterController: floats the capsule above the ground on a virtual
// damped spring (downward raycast probe) instead of hard collision response against the
// ground. Since the capsule never actually touches the floor, small ledges/stairs within
// max_probe_dist just compress/extend the spring instead of stopping the character or
// requiring an explicit step-up case. Walls/ceilings still use the normal collide+slide
// CharacterController internally. Falls back to plain gravity when no ground is within
// probe range (e.g. jumping, walking off a big ledge).
class SpringPogoController
{
public:
	SpringPogoController(PhysicsBody* self) : self(self), wall_controller(self) {}

	// velocity_in.y is the character's current vertical velocity; the spring (or gravity,
	// if airborne) updates it internally. out_velocity is the resulting velocity after both
	// the spring/gravity integration and wall collision clipping.
	void move(const glm::vec3& velocity_in, float dt, glm::vec3& out_velocity, int& out_ccfg_flags);

	void set_position(const glm::vec3& v) { wall_controller.set_position(v); }
	const glm::vec3& get_character_pos() const { return wall_controller.get_character_pos(); }
	void set_physics_body(PhysicsBody* body) {
		self = body;
		wall_controller.set_physics_body(body);
	}
	void set_capsule_info(float height, float radius, float skinsize) {
		wall_controller.capsule_height = height;
		wall_controller.capsule_radius = radius;
		wall_controller.skin_size = skinsize;
	}

	// spring tuning
	float ride_height = 0.6f;		// rest distance the feet float above the ground
	float max_probe_dist = 0.5f;	// how far above/below ride_height the spring still reaches (max smooth step height/drop)
	float spring_strength = 400.f;
	float spring_damping = 40.f;
	float gravity = 18.f; // used only while airborne (no ground within probe range)

	// results of the last move(), useful for debug visualization
	bool grounded = false;
	float last_ground_dist = -1.f; // raw raycast distance from the probe origin, -1 if no hit
	float last_compression = 0.f;	// + = spring pushing up, - = spring pulling down, 0 = resting
	glm::vec3 last_probe_origin{};
	glm::vec3 last_ground_point{};

private:
	PhysicsBody* self = nullptr;
	CharacterController wall_controller;
};

// component wrapper

class CharacterMovementComponent : public Component
{
public:
	CLASS_BODY(CharacterMovementComponent);
	CharacterMovementComponent() : controller(nullptr) {}

	REF void move(glm::vec3 displacement, float dt, float min_dist) {
		controller.move(displacement, dt, min_dist, flags, velocity);
	}

	REF void set_physics_body(PhysicsBody* body) { controller.set_physics_body(body); }
	REF bool is_touching_top() { return flags & (CCCF_ABOVE); }
	REF bool is_touching_side() { return flags & (CCCF_SIDES); }
	REF bool is_touching_down() { return flags & (CCCF_BELOW); }
	REF glm::vec3 get_result_velocity() { return velocity; }
	REF void set_capsule_info(float height, float radius, float skinsize) {
		controller.skin_size = skinsize;
		controller.capsule_height = height;
		controller.capsule_radius = radius;
	}
	REF glm::vec3 get_position() { return controller.get_character_pos(); }
	REF void set_position(glm::vec3 v) { controller.set_position(v); }

private:
	glm::vec3 velocity{};
	int flags = 0;
	CharacterController controller;
};
