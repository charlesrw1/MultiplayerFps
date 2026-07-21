// BikeCourseTurtle.h
// Turtle-graphics path builder shared by the code-generated hardcoded courses
// (BikeCourseHardcoded.cpp) and the hilly course (BikeCourseHilly.cpp): walks
// a heading (theta, radians from +Z toward +X, matching the rest of the bike
// code's atan2(dir.x, dir.z) convention) and appends world-space XZ sample
// points as it goes. Elevation (position.y) is not tracked here -- callers
// that need it (BikeCourseHilly.cpp) fill it in as a separate pass afterward.
#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

struct BikeTurtle {
	std::vector<glm::vec3> positions;
	glm::vec3 pos;
	float     theta;   // heading, radians from +Z axis toward +X
	float     step_m;

	static constexpr glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

	glm::vec3 dir() const { return { sinf(theta), 0.f, cosf(theta) }; }
	glm::vec3 right() const { return glm::normalize(glm::cross(WORLD_UP, dir())); }

	void straight(float length) {
		const int   steps = glm::max(1, (int)std::round(length / step_m));
		const float seg    = length / (float)steps;
		const glm::vec3 d  = dir();
		for (int i = 0; i < steps; ++i) {
			pos += d * seg;
			positions.push_back(pos);
		}
	}

	// turn_deg > 0 = turn right (clockwise, theta increases); < 0 = turn left.
	// Subdivided by BOTH arc length (step_m) and angle (MAX_DEG_PER_STEP): a
	// length-only subdivision collapses a short/tight-radius arc to 1-2 giant
	// facets -- a real heading kink, not a smooth curve. That broke tick_steer's
	// curvature-based lean (central-difference over a 3m window landing inside
	// one facet) and min_turn_radius_ahead's braking scan (sees an artificially
	// tiny radius right at the kink) on the tighter hardcoded corners.
	void arc(float radius, float turn_deg) {
		static constexpr float MAX_DEG_PER_STEP = 6.f;
		const float turn_rad  = glm::radians(turn_deg);
		const float signed_r  = (turn_deg >= 0.f) ? radius : -radius;
		const glm::vec3 center = pos + right() * signed_r;
		const float arc_len       = glm::abs(turn_rad) * radius;
		const int   steps_by_len  = (int)std::round(arc_len / step_m);
		const int   steps_by_deg  = (int)std::ceil(glm::abs(turn_deg) / MAX_DEG_PER_STEP);
		const int   steps         = glm::max(1, glm::max(steps_by_len, steps_by_deg));
		const float dtheta    = turn_rad / (float)steps;
		for (int i = 0; i < steps; ++i) {
			theta += dtheta;
			pos = center - right() * signed_r;
			positions.push_back(pos);
		}
	}
};
