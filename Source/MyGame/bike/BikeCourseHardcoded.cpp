// BikeCourseHardcoded.cpp
// Fully code-generated test circuit — no level spawners, no road network,
// flat ground. See build_hardcoded_circuit() in BikeCourse.h for the shape.
#include "BikeCourse.h"
#include "Debug.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include <cfloat>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Turtle-graphics path builder: walks a heading (theta, radians from +Z toward
// +X, matching the rest of the bike code's atan2(dir.x, dir.z) convention) and
// appends world-space sample points as it goes.
namespace {

struct Turtle {
	std::vector<glm::vec3> positions;
	glm::vec3 pos;
	float     theta;   // heading, radians from +Z axis toward +X
	float     step_m;

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
	void arc(float radius, float turn_deg) {
		const float turn_rad  = glm::radians(turn_deg);
		const float signed_r  = (turn_deg >= 0.f) ? radius : -radius;
		const glm::vec3 center = pos + right() * signed_r;
		const float arc_len   = glm::abs(turn_rad) * radius;
		const int   steps     = glm::max(1, (int)std::round(arc_len / step_m));
		const float dtheta    = turn_rad / (float)steps;
		for (int i = 0; i < steps; ++i) {
			theta += dtheta;
			pos = center - right() * signed_r;
			positions.push_back(pos);
		}
	}
};

} // namespace

void build_hardcoded_circuit(BikeCourse& course)
{
	static constexpr float ROAD_HALF_WIDTH = 4.f;
	static constexpr float Y_EPS           = 0.02f;   // small epsilon above ground 0
	static constexpr float LEG_NS          = 22.f;    // north/south straight length (m)
	static constexpr float LEG_EW          = 14.f;    // east/west straight length (m)
	static constexpr float RADIUS_WIDE     = 12.f;    // fast sweeper corners (NE, SW)
	static constexpr float RADIUS_TIGHT    = 5.f;      // hairpin-like corners (SE, NW)

	// Opposite corners must share a radius for the turtle path to close exactly
	// in position (each cardinal-direction leg's net displacement only cancels
	// against its opposite leg if the corner radii match). Still gives two
	// tight and two wide corners for cornering variety.
	Turtle t;
	t.pos     = glm::vec3(0.f, Y_EPS, 0.f);
	t.theta   = 0.f;
	t.step_m  = glm::max(0.5f, course.sample_step_m);
	t.positions.push_back(t.pos);

	t.straight(LEG_NS);
	t.arc(RADIUS_WIDE, 90.f);   // NE
	t.straight(LEG_EW);
	t.arc(RADIUS_TIGHT, 90.f);  // SE
	t.straight(LEG_NS);
	t.arc(RADIUS_WIDE, 90.f);   // SW
	t.straight(LEG_EW);
	t.arc(RADIUS_TIGHT, 90.f);  // NW — closes back onto the start point/heading

	// The last generated sample coincides with the start (loop wrap handles the
	// closing segment) — drop it so waypoints aren't duplicated at the seam.
	if (t.positions.size() > 1 &&
	    glm::distance(t.positions.back(), t.positions.front()) < 0.05f) {
		t.positions.pop_back();
	}

	// Center the circuit on the origin (requested: fit in an 80x80m box around
	// origin) — computed from the actual generated bbox rather than assumed from
	// the leg/radius constants, so this stays correct if those are ever tuned.
	{
		glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
		for (const glm::vec3& p : t.positions) {
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const glm::vec3 center = glm::vec3((lo.x + hi.x) * 0.5f, 0.f, (lo.z + hi.z) * 0.5f);
		for (glm::vec3& p : t.positions)
			p -= center;
		sys_print(Info, "BikeCourse (hardcoded): bbox %.0f x %.0f m\n", hi.x - lo.x, hi.z - lo.z);
	}

	const int n = (int)t.positions.size();
	course.waypoints.clear();
	course.waypoints.resize(n);
	for (int i = 0; i < n; ++i) {
		BikeWaypoint& wp = course.waypoints[i];
		wp.position        = t.positions[i];
		wp.road_half_width = ROAD_HALF_WIDTH;

		glm::vec3 fwd = (i < n - 1) ? (t.positions[i + 1] - t.positions[i])
		                            : (t.positions[0]     - t.positions[i]);
		const float fwd_len = glm::length(fwd);
		fwd = (fwd_len > 1e-4f) ? fwd / fwd_len
		    : (i > 0 ? course.waypoints[i - 1].forward : glm::vec3(0, 0, 1));
		wp.forward  = fwd;
		wp.gradient = 0.f;  // flat course

		const glm::vec3 right = glm::cross(WORLD_UP, fwd);
		const float     rlen  = glm::length(right);
		wp.right = (rlen > 1e-4f) ? (right / rlen) : glm::vec3(1, 0, 0);

		wp.dist_from_start = (i == 0) ? 0.f
		    : course.waypoints[i - 1].dist_from_start + glm::distance(t.positions[i], t.positions[i - 1]);
	}

	course.is_loop       = true;
	course.total_length_m = course.waypoints.back().dist_from_start
	                       + glm::distance(t.positions.back(), t.positions.front());
	course.is_built       = true;
	course.debug_fillets.clear();
	course.arc_ranges.clear();

	sys_print(Info, "BikeCourse (hardcoded): %d waypoints, %.0f m circuit\n",
	          n, course.total_length_m);

	BikeCourse::compute_racing_line(course.waypoints, course.is_loop,
	                                 course.rl_k, course.rl_mass, course.rl_dt,
	                                 course.rl_num_iters, course.rl_smooth_passes, course.rl_smooth_w);
}
