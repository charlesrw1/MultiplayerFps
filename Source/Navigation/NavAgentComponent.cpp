#include "NavAgentComponent.h"
#include "RuntimeNavManager.h"

#include "Game/Entity.h"
#include "GameEnginePublic.h"
#include "Debug.h"

#include "glm/gtc/constants.hpp"

void NavAgentComponent::start() {
	corners.clear();
	next_corner = 0;
	arrived     = false;
}

void NavAgentComponent::stop() {
	corners.clear();
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

void NavAgentComponent::update() {
	if (arrived || corners.empty() || next_corner >= (int)corners.size())
		return;
	const float dt = (float)eng->get_dt();
	const glm::vec3 pos    = get_owner()->get_ws_position();
	const glm::vec3 target = corners[next_corner];
	const glm::vec3 delta  = target - pos;
	const float dist       = glm::length(delta);

	if (dist <= arrive_radius) {
		next_corner++;
		if (next_corner >= (int)corners.size()) {
			arrived = true;
			return;
		}
		return;
	}
	const float step = std::min(move_speed * dt, dist);
	const glm::vec3 dir = delta / dist;
	get_owner()->set_ws_position(pos + dir * step);
}
