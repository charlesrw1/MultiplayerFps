// BikeCourseHardcoded.cpp
// Fully code-generated test circuits — no level spawners, no road network,
// flat ground. See build_hardcoded_circuit() in BikeCourse.h for the shapes.
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

// ============================================================
// Half-lap path definitions. Each must net exactly 180 deg of turning
// (sum of arc() turn_deg args) — see the "repeat identical relative turtle
// commands" note in BikeCourse.h. Called twice by finalize_hardcoded_course's
// caller to produce the full closed loop.
// ============================================================

// Rounded rectangle: two wide sweepers + two tight hairpins. Opposite corners
// share a radius so the turtle path closes exactly (each cardinal-direction
// leg's net displacement only cancels against its opposite leg if the corner
// radii match) — legs sized so the full loop's bbox fills -30..+30m on both
// axes.
void half_lap_classic(Turtle& t) {
	static constexpr float RADIUS_WIDE  = 12.f;  // fast sweeper corners
	static constexpr float RADIUS_TIGHT = 5.f;   // hairpin-like corners
	static constexpr float LEG          = 60.f - RADIUS_WIDE - RADIUS_TIGHT;
	t.straight(LEG);
	t.arc(RADIUS_WIDE, 90.f);
	t.straight(LEG);
	t.arc(RADIUS_TIGHT, 90.f);
}

// Chicanes and alternating-direction bends — tighter radii (7-14m) and
// several sign reversals per half-lap, so the direction keeps changing
// instead of settling into long straights between a few big corners. Sized
// to fill most of the -40..+40m play area (bbox ~72 x 68m).
// Turn sum: 50 - 25 + 55 - 20 + 30 + 90 = 180.
void half_lap_twisty(Turtle& t) {
	t.straight(14.f);
	t.arc(14.f,  50.f);
	t.straight(10.f);
	t.arc(9.f,  -25.f);  // chicane kick, opposite direction
	t.straight(12.f);
	t.arc(12.f,  55.f);
	t.straight(9.f);
	t.arc(7.f,  -20.f);  // small wiggle
	t.straight(12.f);
	t.arc(9.f,   30.f);
	t.straight(10.f);
	t.arc(7.f,   90.f);  // sharper corner heading back toward the return leg
}

// Distinct sharp-angle corners (90/45/60 deg) on tight radii, long straights
// between — reads as intersections/city-block corners rather than swept
// racetrack curves. Sized to fill most of the -40..+40m play area (bbox
// ~69 x 64m). Turn sum: 90 + 45 - 15 + 60 = 180.
void half_lap_sharp_angles(Turtle& t) {
	t.straight(28.f);
	t.arc(4.f,   90.f);  // sharp near-square corner
	t.straight(24.f);
	t.arc(4.5f,  45.f);  // medium corner
	t.straight(20.f);
	t.arc(4.5f, -15.f);  // slight bend the other way
	t.straight(24.f);
	t.arc(4.f,   60.f);  // medium-sharp corner
}

// Converts a completed turtle path into course.waypoints (forward/right/
// dist_from_start), centers it on the origin, and runs the racing line sim.
// Shared tail end of every build_hardcoded_circuit() variant.
void finalize_hardcoded_course(BikeCourse& course, Turtle& t, const char* label) {
	static constexpr float ROAD_HALF_WIDTH = 4.f * (2.f / 3.f);

	// The last generated sample coincides with the start (loop wrap handles the
	// closing segment) — drop it so waypoints aren't duplicated at the seam.
	if (t.positions.size() > 1 &&
	    glm::distance(t.positions.back(), t.positions.front()) < 0.05f) {
		t.positions.pop_back();
	}

	// Center the circuit on the origin — computed from the actual generated
	// bbox rather than assumed from any leg/radius constants.
	{
		glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
		for (const glm::vec3& p : t.positions) {
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const glm::vec3 center = glm::vec3((lo.x + hi.x) * 0.5f, 0.f, (lo.z + hi.z) * 0.5f);
		for (glm::vec3& p : t.positions)
			p -= center;
		sys_print(Info, "BikeCourse (hardcoded, %s): bbox %.0f x %.0f m\n", label, hi.x - lo.x, hi.z - lo.z);
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

	course.is_loop        = true;
	course.total_length_m = course.waypoints.back().dist_from_start
	                       + glm::distance(t.positions.back(), t.positions.front());
	course.is_built        = true;
	course.debug_fillets.clear();
	course.arc_ranges.clear();

	sys_print(Info, "BikeCourse (hardcoded, %s): %d waypoints, %.0f m circuit\n",
	          label, n, course.total_length_m);

	BikeCourse::compute_racing_line(course.waypoints, course.is_loop,
	                                 course.rl_k, course.rl_mass, course.rl_dt,
	                                 course.rl_num_iters, course.rl_smooth_passes, course.rl_smooth_w,
	                                 course.rl_margin);
}

} // namespace

void build_hardcoded_circuit(BikeCourse& course, BikeHardcodedCourseKind kind)
{
	Turtle t;
	t.pos     = glm::vec3(0.f, 0.02f, 0.f);  // small epsilon above ground 0
	t.theta   = 0.f;
	t.step_m  = glm::max(0.5f, course.sample_step_m);
	t.positions.push_back(t.pos);

	switch (kind) {
		case BikeHardcodedCourseKind::Twisty:
			half_lap_twisty(t);
			half_lap_twisty(t);
			break;
		case BikeHardcodedCourseKind::SharpAngles:
			half_lap_sharp_angles(t);
			half_lap_sharp_angles(t);
			break;
		case BikeHardcodedCourseKind::ClassicLoop:
		default:
			half_lap_classic(t);
			half_lap_classic(t);
			break;
	}

	finalize_hardcoded_course(course, t, bike_hardcoded_course_name(kind));
}
