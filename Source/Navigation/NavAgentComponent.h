#pragma once
// Ticks each frame, walks an A* corner string, drives owner transform toward the next corner.
// Single-agent v1: no avoidance, no replanning on environment changes — caller re-requests as needed.
//
// @docs [[navigation#agent-component]]

#include "Game/EntityComponent.h"
#include <vector>

class NavAgentComponent : public Component
{
public:
	CLASS_BODY(NavAgentComponent);
	NavAgentComponent() { set_ticking(true); }

	void start() final;
	void update() final;
	void stop() final;

	REF float move_speed    = 3.f;
	REF float arrive_radius = 0.25f;

	// Per-agent debug overlay; honored independently of the global nav.debug.agents cvar.
	REF bool  debug_draw_path = false;

	REF bool request_path_to(glm::vec3 dest);
	REF bool is_path_valid() const { return !corners.empty(); }
	REF bool has_arrived() const { return arrived; }

	const std::vector<glm::vec3>& get_corners() const { return corners; }
	int get_next_corner_index() const { return next_corner; }

private:
	std::vector<glm::vec3> corners;
	int  next_corner = 0;
	bool arrived     = false;
};
