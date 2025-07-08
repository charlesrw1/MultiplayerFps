#pragma once

#include <glm/glm.hpp>

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

private:
	PhysicsBody* self = nullptr;
	int cached_flags = 0;
	glm::vec3 position{};	// internal position
};