#include "CharacterController.h"
#include "Physics/Physics2.h"
#include "Debug.h"
#include "Physics/ChannelsAndPresets.h"

// movement code taken from physx character controller and quake

static glm::vec3 project_onto_plane(const glm::vec3& plane_normal, const glm::vec3& vector) {
	glm::vec3 penetration_velocity = dot(vector, plane_normal) * plane_normal;
	glm::vec3 slide_velocity = vector - penetration_velocity;
	return slide_velocity;
}

static glm::vec3 collision_response(const glm::vec3& current_direction, const glm::vec3& hit_normal,
									const glm::vec3& current_position, const glm::vec3& wanted_position, float friction,
									float bump, glm::vec3& in_out_velocity) {
	// Compute reflect direction
	glm::vec3 reflect_dir = glm::normalize(glm::reflect(current_direction, hit_normal));

	// Decompose it
	glm::vec3 tangent = project_onto_plane(hit_normal, current_direction);

	// Compute new destination position
	float distance_to_target = glm::length(wanted_position - current_position);

	glm::vec3 out = current_position;
	if (bump != 0.0f) {
		out += hit_normal * bump * distance_to_target;
	}
	if (friction != 0.0f) {
		out += tangent * friction * distance_to_target;
	}

	in_out_velocity = project_onto_plane(hit_normal, in_out_velocity);

	return out;
}

void CharacterController::move(const glm::vec3& disp, float dt, float min_dist, int& out_ccfg_flags,
							   glm::vec3& out_velocity) {
	const float actual_half_height = capsule_height * 0.5 - capsule_radius;
	vertical_capsule_def_t shape_def;
	shape_def.half_height = actual_half_height;
	shape_def.radius = capsule_radius;

	glm::vec3 current_velocity = disp / dt;
	glm::vec3 current_pos = position;
	glm::vec3 target_pos = position + disp;

	int max_iter = 5;
	out_ccfg_flags = 0;

	TraceIgnoreVec ignore;
	ignore.push_back(self);

	for (; max_iter >= 0; max_iter--) {
		auto current_direction = target_pos - current_pos;
		float length = glm::length(current_direction);
		if (length <= min_dist) {
			break;
		}
		current_direction /= length;
		if (dot(current_direction, disp) <= 0)
			break;
		world_query_result wqr;
		{
			// do collision test
			glm::vec3 actual_capsule_pos = current_pos + glm::vec3(0, capsule_height * 0.5, 0);

			uint32_t flags = (1 << (int)PL::Default) | (1 << (int)PL::Character);

			bool has_hit = g_physics.sweep_capsule(wqr, shape_def, actual_capsule_pos, current_direction,
												   length + skin_size, flags, &ignore);
			if (!has_hit) {
				current_pos = target_pos;
				break;
			} else if (wqr.had_initial_overlap) {
				current_pos -= wqr.hit_normal * (wqr.distance - 0.0001f /* epsilon */);
				// Debug::add_sphere(wqr.hit_pos, 0.5, COLOR_GREEN, 0.05);
				continue;
			}
			// else
			// Debug::add_sphere(wqr.hit_pos, 0.5, COLOR_BLUE, 0.0);
		}

		if (wqr.hit_normal.y >= 0.85)
			out_ccfg_flags |= CharacterControllerCollisionFlags::CCCF_BELOW;
		else if (wqr.hit_normal.y <= -0.85)
			out_ccfg_flags |= CharacterControllerCollisionFlags::CCCF_ABOVE;
		else
			out_ccfg_flags |= CharacterControllerCollisionFlags::CCCF_SIDES;

		if (wqr.distance >= skin_size)
			current_pos = current_pos + current_direction * (wqr.distance - skin_size);

		target_pos =
			collision_response(current_direction, wqr.hit_normal, current_pos, target_pos, 1.0, 0.0, current_velocity);
	}
	cached_flags = out_ccfg_flags;
	out_velocity = current_velocity;
	position = current_pos;

	// Debug::add_sphere(current_pos + glm::vec3(0, capsule_radius, 0), capsule_radius, COLOR_PINK, 0);
	// Debug::add_sphere(current_pos + glm::vec3(0, capsule_height - capsule_radius, 0), capsule_radius, COLOR_PINK, 0);
}

void SpringPogoController::move(const glm::vec3& velocity_in, float dt, glm::vec3& out_velocity,
								int& out_ccfg_flags) {
	glm::vec3 feet_pos = wall_controller.get_character_pos();
	glm::vec3 velocity = velocity_in;

	// probe from max_probe_dist above the feet, down through ride_height and another
	// max_probe_dist below it, so ground anywhere in [-max_probe_dist, +max_probe_dist]
	// around the rest height is detected.
	glm::vec3 probe_origin = feet_pos + glm::vec3(0, max_probe_dist, 0);
	float probe_length = max_probe_dist * 2.f + ride_height;

	TraceIgnoreVec ignore;
	ignore.push_back(self);
	uint32_t channel_mask = (1 << (int)PL::Default) | (1 << (int)PL::Character);

	world_query_result wqr;
	bool hit = g_physics.trace_ray(wqr, probe_origin, glm::vec3(0, -1, 0), probe_length, &ignore, channel_mask);

	last_probe_origin = probe_origin;

	if (hit) {
		float current_gap = wqr.distance - max_probe_dist; // how far the feet currently are above the ground
		float compression = ride_height - current_gap;		// + = compressed (push up), - = stretched (pull down)

		// don't let the spring's pull-down fight an active upward jump; it still pushes
		// if something rises up under the character (a step) while ascending.
		if (velocity.y > 0.f)
			compression = glm::max(compression, 0.f);

		float spring_accel = spring_strength * compression - spring_damping * velocity.y;
		velocity.y += spring_accel * dt;

		grounded = true;
		last_ground_dist = wqr.distance;
		last_compression = compression;
		last_ground_point = wqr.hit_pos;
	} else {
		velocity.y -= gravity * dt;

		grounded = false;
		last_ground_dist = -1.f;
		last_compression = 0.f;
		last_ground_point = probe_origin + glm::vec3(0, -probe_length, 0);
	}

	wall_controller.move(velocity * dt, dt, 0.001f, out_ccfg_flags, out_velocity);
}
