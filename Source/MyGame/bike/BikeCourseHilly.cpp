// BikeCourseHilly.cpp
// Generates the "Hilly" hardcoded course: a curvy closed-loop road draped over
// procedural perlin-noise terrain, with the road's own gradient clamped to
// g_hilly_params.max_grade_pct (the terrain underneath can be locally steeper
// -- bike_hilly_height is queried directly by BikeObject's tick_transform, so
// the bike always follows the true terrain height; only the ROAD's placement
// is grade-limited, same idea as a real road cut into a hillside).
#include "BikeHeaders.h"
#include "BikeCourseHilly.h"
#include "BikeCourseTurtle.h"
#include "BikeTerrainNoise.h"
#include "Debug.h"
#include <glm/glm.hpp>
#include <cfloat>
#include <cmath>

extern BikeGameApplication* g_bike_app;

BikeHillyParams g_hilly_params;

namespace {
// Raw (pre vertical-offset) height -- amplitude-scaled fBm only. Cached
// Perlin2D instance re-seeded on demand, same as before.
float bike_hilly_raw_height(float x, float z) {
	static bike_noise::Perlin2D noise(g_hilly_params.seed);
	static unsigned cached_seed = g_hilly_params.seed;
	if (cached_seed != g_hilly_params.seed) {
		noise.reseed(g_hilly_params.seed);
		cached_seed = g_hilly_params.seed;
	}
	return g_hilly_params.amplitude_m * noise.fbm(x, z,
		glm::max(1, g_hilly_params.octaves), g_hilly_params.base_freq,
		g_hilly_params.lacunarity, g_hilly_params.gain);
}

// Vertical shift applied on top of the raw height so the LOWEST point anywhere
// in the terrain footprint sits at +1m -- keeps the level's existing flat
// ground plane permanently hidden underneath the terrain mesh. Recomputed by
// bike_hilly_recompute_height_offset() whenever the Hilly course (re)builds,
// since it depends on every noise/amplitude/size parameter.
float g_hilly_height_offset = 0.f;

// Above what the mesh's lowest point should sit, so the pre-existing flat
// ground plane in the level (which this course mode never removes -- it isn't
// this course's terrain) is always fully covered.
constexpr float HILLY_MIN_HEIGHT_M = 1.f;

void bike_hilly_recompute_height_offset() {
	const float half = g_hilly_params.terrain_size_m * 0.5f;
	constexpr int SCAN_RES = 64;  // coarse scan is enough -- fBm doesn't have sharp isolated minima
	float min_h = FLT_MAX;
	for (int i = 0; i <= SCAN_RES; ++i) {
		const float x = -half + 2.f * half * (float)i / (float)SCAN_RES;
		for (int j = 0; j <= SCAN_RES; ++j) {
			const float z = -half + 2.f * half * (float)j / (float)SCAN_RES;
			min_h = glm::min(min_h, bike_hilly_raw_height(x, z));
		}
	}
	g_hilly_height_offset = HILLY_MIN_HEIGHT_M - min_h;
}
} // namespace

float bike_hilly_height(float x, float z)
{
	return bike_hilly_raw_height(x, z) + g_hilly_height_offset;
}

namespace {
// Nearest-segment XZ projection onto the built road (course.waypoints),
// dedicated to terrain conforming rather than reusing BikeCourse::project:
// that one scores by full 3D distance (biases toward matching height too,
// which is exactly backwards here -- we're trying to FIND the road so we can
// pull the terrain's height to match it) and is tuned for per-rider frame-to-
// frame continuity (prev_dist_m window) that a one-off grid-vertex/wheel-
// contact query doesn't have.
struct RoadProjection { float road_y; float lateral_dist_xz; float road_half_width; };

bool project_onto_hilly_road(float x, float z, const BikeCourse& course, RoadProjection& out) {
	const int n = (int)course.waypoints.size();
	if (n < 2) return false;
	const int num_segs = course.is_loop ? n : n - 1;
	const glm::vec2 p(x, z);

	float best_dist_sq = FLT_MAX;
	for (int i = 0; i < num_segs; ++i) {
		const int j = (i + 1) % n;
		const glm::vec2 a(course.waypoints[i].position.x, course.waypoints[i].position.z);
		const glm::vec2 b(course.waypoints[j].position.x, course.waypoints[j].position.z);
		const glm::vec2 ab = b - a;
		const float ab_sq = glm::dot(ab, ab);
		const float t = (ab_sq > 1e-8f) ? glm::clamp(glm::dot(p - a, ab) / ab_sq, 0.f, 1.f) : 0.f;
		const glm::vec2 closest = a + t * ab;
		const glm::vec2 diff    = p - closest;
		const float dist_sq = glm::dot(diff, diff);
		if (dist_sq < best_dist_sq) {
			best_dist_sq         = dist_sq;
			out.road_y           = glm::mix(course.waypoints[i].position.y, course.waypoints[j].position.y, t);
			out.road_half_width  = glm::mix(course.waypoints[i].road_half_width, course.waypoints[j].road_half_width, t);
		}
	}
	out.lateral_dist_xz = std::sqrt(best_dist_sq);
	return true;
}
} // namespace

float bike_hilly_terrain_height(float x, float z)
{
	const float raw = bike_hilly_height(x, z);
	if (!g_bike_app || g_bike_app->course_variant != BikeHardcodedCourseKind::Hilly || !g_bike_app->course.is_built)
		return raw;

	RoadProjection proj;
	if (!project_onto_hilly_road(x, z, g_bike_app->course, proj))
		return raw;

	// Flush with the road surface under its full width, then smoothstep back
	// up/down to the natural terrain over road_blend_dist_m -- an embankment,
	// so the terrain always meets the (independently grade-limited) road with
	// no floating/buried gap, however steep the raw noise is at that point.
	const float blend_start = proj.road_half_width;
	const float blend_end   = proj.road_half_width + glm::max(0.1f, g_hilly_params.road_blend_dist_m);
	if (proj.lateral_dist_xz <= blend_start) return proj.road_y;
	if (proj.lateral_dist_xz >= blend_end)   return raw;

	const float t = (proj.lateral_dist_xz - blend_start) / (blend_end - blend_start);
	const float smooth_t = t * t * (3.f - 2.f * t);  // smoothstep
	return glm::mix(proj.road_y, raw, smooth_t);
}

namespace {

// Broad, flowing sweeping bends -- gentler/wider radii than half_lap_twisty's
// chicanes (this mode's "curvy" comes from the terrain read, not tight
// direction reversals). Sized to fill most of the 150x150m terrain footprint
// with a comfortable margin. Turn sum: 55 - 35 + 65 + 95 = 180.
void half_lap_hilly(BikeTurtle& t) {
	t.straight(24.f);
	t.arc(24.f,  55.f);
	t.straight(18.f);
	t.arc(18.f, -35.f);  // gentle S-bend the other way
	t.straight(18.f);
	t.arc(22.f,  65.f);
	t.straight(12.f);
	t.arc(15.f,  95.f);  // sharper corner heading back toward the return leg
}

} // namespace

void build_hilly_circuit(BikeCourse& course)
{
	static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

	// Must run before any bike_hilly_height() sampling below (and before
	// BikeCourseTerrainMesh.cpp's build_terrain_mesh(), which runs right after
	// this in rebuild_course()) -- depends on the current noise/amplitude/size
	// params, not on anything computed in this function.
	bike_hilly_recompute_height_offset();

	BikeTurtle t;
	t.pos    = glm::vec3(0.f, 0.f, 0.f);
	t.theta  = 0.f;
	t.step_m = glm::max(0.5f, course.sample_step_m);
	t.positions.push_back(t.pos);

	half_lap_hilly(t);
	half_lap_hilly(t);

	// The last generated sample coincides with the start (loop wrap handles the
	// closing segment) -- drop it so waypoints aren't duplicated at the seam.
	if (t.positions.size() > 1 &&
	    glm::distance(t.positions.back(), t.positions.front()) < 0.05f) {
		t.positions.pop_back();
	}

	// Center the circuit on the origin (XZ only) so it lines up with the fixed
	// -terrain_size_m/2..+terrain_size_m/2 world-space square the terrain mesh
	// is built over (BikeCourseTerrainMesh.cpp).
	{
		glm::vec3 lo(FLT_MAX), hi(-FLT_MAX);
		for (const glm::vec3& p : t.positions) {
			lo = glm::min(lo, p);
			hi = glm::max(hi, p);
		}
		const glm::vec3 center = glm::vec3((lo.x + hi.x) * 0.5f, 0.f, (lo.z + hi.z) * 0.5f);
		for (glm::vec3& p : t.positions)
			p -= center;
		sys_print(Info, "BikeCourse (hardcoded, Hilly): bbox %.0f x %.0f m\n", hi.x - lo.x, hi.z - lo.z);
	}

	const int n = (int)t.positions.size();

	// Sample raw terrain height per waypoint, then grade-limit it so the ROAD
	// never exceeds max_grade_pct even where the underlying terrain does --
	// iterative forward/backward clamping against neighbor-distance * max
	// grade, same "relax repeatedly until it settles" idiom as
	// BikeCourse::compute_racing_line's hinge-spring simulation.
	std::vector<float> elev(n);
	for (int i = 0; i < n; ++i)
		elev[i] = bike_hilly_height(t.positions[i].x, t.positions[i].z);

	const float max_grade = g_hilly_params.max_grade_pct * 0.01f;
	for (int pass = 0; pass < g_hilly_params.grade_smooth_passes; ++pass) {
		for (int i = 0; i < n; ++i) {
			const int prev = (i == 0) ? n - 1 : i - 1;
			const float dist = glm::distance(glm::vec2(t.positions[i].x, t.positions[i].z),
			                                  glm::vec2(t.positions[prev].x, t.positions[prev].z));
			const float max_step = dist * max_grade;
			elev[i] = glm::clamp(elev[i], elev[prev] - max_step, elev[prev] + max_step);
		}
		for (int i = n - 1; i >= 0; --i) {
			const int next = (i == n - 1) ? 0 : i + 1;
			const float dist = glm::distance(glm::vec2(t.positions[i].x, t.positions[i].z),
			                                  glm::vec2(t.positions[next].x, t.positions[next].z));
			const float max_step = dist * max_grade;
			elev[i] = glm::clamp(elev[i], elev[next] - max_step, elev[next] + max_step);
		}
	}
	for (int i = 0; i < n; ++i)
		t.positions[i].y = elev[i] + 0.02f;  // small epsilon above the terrain surface, matches other hardcoded courses

	static constexpr float ROAD_HALF_WIDTH = 4.f * (2.f / 3.f);
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
		// Matches BikeCourseRoadNetwork.cpp's wp.gradient convention: signed
		// pitch angle of the forward tangent itself, now that fwd carries a
		// real y component.
		wp.gradient = glm::asin(glm::clamp(fwd.y, -1.f, 1.f));

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

	sys_print(Info, "BikeCourse (hardcoded, Hilly): %d waypoints, %.0f m circuit\n",
	          n, course.total_length_m);

	BikeCourse::compute_racing_line(course.waypoints, course.is_loop,
	                                 course.rl_k, course.rl_mass, course.rl_dt,
	                                 course.rl_num_iters, course.rl_smooth_passes, course.rl_smooth_w,
	                                 course.rl_margin);
}
