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
	void move(
		const glm::vec3& displacement,	// how far to move the character this time step (velocity*dt)
		float dt,						// time step
		float min_dist,					// min dist to move
		int& out_ccfg_flags,		// bitmask of CharacterControllerCollisionFlags
		glm::vec3& out_velocity			// internally, the Controller doesnt store velocity, out_velocity is just a useful 
										// return of what the characters velocity should be after sliding along surfaces by clipping the in velocity
										// ie in_velocity = displacement/dt. If no collision, then out_velocity = in_velocity
										// the caller can use this value how they want
	);

	// teleport move
	void set_position(const glm::vec3& v) {
		position = v;
	}

	const glm::vec3& get_character_pos() const {
		return position;
	}

	// parameters
	float capsule_height = 2.f;
	float capsule_radius = 0.25f;
	float step_height = 0.1;
	float skin_size = 0.05;

	void set_physics_body(PhysicsBody* self) {
		this->self = self;
	}
private:
	PhysicsBody* self = nullptr;
	int cached_flags = 0;
	glm::vec3 position{};	// internal position
};

// component wrapper

class CharacterMovementComponent : public Component {
public:
	CLASS_BODY(CharacterMovementComponent);
	CharacterMovementComponent() :controller(nullptr){}

	REF void move(glm::vec3 displacement, float dt, float min_dist) {
		controller.move(displacement, dt, min_dist, flags, velocity);
	}

	REF void set_physics_body(PhysicsBody* body) {
		controller.set_physics_body(body);
	}
	REF bool is_touching_top() {
		return flags & (CCCF_ABOVE);
	}
	REF bool is_touching_side() {
		return flags & (CCCF_SIDES);
	}
	REF bool is_touching_down() {
		return flags & (CCCF_BELOW);
	}
	REF glm::vec3 get_result_velocity() {
		return velocity;
	}
	REF void set_capsule_info(float height, float radius, float skinsize) {
		controller.skin_size = skinsize;
		controller.capsule_height = height;
		controller.capsule_radius = radius;
	}
	REF glm::vec3 get_position() {
		return controller.get_character_pos();
	}
	REF void set_position(glm::vec3 v) {
		controller.set_position(v);
	}
private:
	glm::vec3 velocity{};
	int flags = 0;
	CharacterController controller;
};
