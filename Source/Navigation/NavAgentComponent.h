#pragma once
// Ticks each frame, walks an A* corner string, drives owner transform toward the next corner.
// Faces owner along its actual (avoidance-adjusted) heading each tick.
// Local avoidance is force-based separation against other NavAgentComponents in the level,
// no replanning on environment changes — caller re-requests as needed.
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

	// Plain REF data fields are only live-writable from Lua in the editor (property-grid shadow
	// sync); at runtime `obj.field = x` just rawsets a table entry and never touches the C++
	// member. Anything Lua needs to mutate during play must go through a REF setter instead.
	REF float get_move_speed() const { return move_speed; }
	REF void  set_move_speed(float s) { move_speed = s; }
	REF float get_arrive_radius() const { return arrive_radius; }
	REF void  set_arrive_radius(float r) { arrive_radius = r; }

	// Local avoidance: agents within (avoid_radius + other.avoid_radius) push apart,
	// falloff linear to zero at the combined radius. Set avoid_radius to 0 to disable.
	// Editor-tuned only (not written from Lua at runtime), so plain REF fields are fine here.
	REF float avoid_radius   = 0.5f;
	REF float avoid_strength = 3.f;

	// Per-agent debug overlay; honored independently of the global nav.debug.agents cvar.
	REF bool  debug_draw_path = false;

	REF bool request_path_to(glm::vec3 dest);
	REF bool is_path_valid() const { return !corners.empty(); }
	REF bool has_arrived() const { return arrived; }

	const std::vector<glm::vec3>& get_corners() const { return corners; }
	int get_next_corner_index() const { return next_corner; }

	// All live NavAgentComponents, for avoidance queries. Self-registered in start()/stop().
	static const std::vector<NavAgentComponent*>& get_all_agents() { return all_agents; }

private:
	glm::vec3 compute_avoidance_force(const glm::vec3& pos) const;

	float move_speed    = 3.f;
	float arrive_radius = 0.25f;

	std::vector<glm::vec3> corners;
	int  next_corner = 0;
	bool arrived     = false;

	static std::vector<NavAgentComponent*> all_agents;
};
