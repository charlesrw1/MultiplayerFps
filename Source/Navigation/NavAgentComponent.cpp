#include "NavAgentComponent.h"
#include "RuntimeNavManager.h"

#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Debug.h"

#include "glm/gtc/constants.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
#include "glm/gtx/norm.hpp"

#include <algorithm>

std::vector<NavAgentComponent*> NavAgentComponent::all_agents;

void NavAgentComponent::start() {
	corners.clear();
	next_corner = 0;
	arrived     = false;
	all_agents.push_back(this);
}

void NavAgentComponent::stop() {
	corners.clear();
	all_agents.erase(std::remove(all_agents.begin(), all_agents.end(), this), all_agents.end());
}

bool NavAgentComponent::request_path_to(glm::vec3 dest) {
	corners.clear();
	next_corner = 0;
	arrived     = false;
	if (!RuntimeNavManager::inst || !RuntimeNavManager::inst->has_navmesh())
		return false;
	glm::vec3 start = get_owner()->get_ws_position();
	if (!RuntimeNavManager::inst->find_path(start, dest, corners))
		return false;
	// Skip first corner (start position) so update() steers directly to next waypoint.
	next_corner = corners.size() > 1 ? 1 : 0;
	return true;
}

glm::vec3 NavAgentComponent::compute_avoidance_force(const glm::vec3& pos) const {
	glm::vec3 force(0.f);
	if (avoid_radius <= 0.f)
		return force;
	for (auto* other : all_agents) {
		if (other == this)
			continue;
		glm::vec3 away = pos - other->get_owner()->get_ws_position();
		away.y = 0.f;
		const float dist = glm::length(away);
		const float combined_radius = avoid_radius + other->avoid_radius;
		if (dist <= 1e-4f || dist >= combined_radius)
			continue;
		const float push = avoid_strength * (1.f - dist / combined_radius);
		force += (away / dist) * push;
	}
	return force;
}
#include <Framework/MathLib.h>
void NavAgentComponent::update() {
	const float dt  = (float)eng->get_dt();
	const glm::vec3 pos = get_owner()->get_ws_position();

	// Path-following velocity: zero once arrived / out of corners, so an idle agent contributes
	// nothing here but can still be pushed by avoidance below (e.g. multiple followers clustered
	// on the player, all sitting at move_speed == 0).
	glm::vec3 path_velocity(0.f);
	if (!arrived && !corners.empty() && next_corner < (int)corners.size()) {
		const glm::vec3 target = corners[next_corner];
		const glm::vec3 delta  = target - pos;
		const float dist       = glm::length(delta);

		if (dist <= arrive_radius) {
			next_corner++;
			if (next_corner >= (int)corners.size())
				arrived = true;
			// Don't return here — still let avoidance resolve this tick even on the arrival frame.
		} else {
			path_velocity = (delta / dist) * move_speed;
		}
	}

	// Avoidance is independent of move_speed/path state so it still separates stationary/arrived
	// agents; it is NOT part of the move_speed clamp below.
	const glm::vec3 velocity = path_velocity + compute_avoidance_force(pos);
	const float speed = glm::length(velocity);
	if (speed <= 1e-4f)
		return;

	const glm::vec3 move_dir = velocity / speed;
	const glm::vec3 new_pos  = pos + move_dir * speed * dt;

	// Face the actual (avoidance-adjusted) heading, flattened so slopes/steps don't tilt the agent.
	glm::vec3 face_dir = move_dir;
	face_dir.y = 0.f;
	if (glm::length2(face_dir) > 1e-8f) {
		face_dir = glm::normalize(face_dir);
		const glm::quat face_rot_target = glm::quat_cast(glm::inverse(glm::lookAt(new_pos, new_pos + face_dir, glm::vec3(0, 1, 0))));
		const auto current_rotation = damp_dt_independent<glm::quat>(face_rot_target, get_owner()->get_ws_rotation(),
														rotation_damping, eng->get_dt());
		get_owner()->set_ws_position_rotation(new_pos, current_rotation);
	} else {
		get_owner()->set_ws_position(new_pos);
	}
}
