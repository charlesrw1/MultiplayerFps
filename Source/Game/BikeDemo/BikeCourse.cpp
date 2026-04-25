#include "BikeCourse.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Debug.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Catmull-Rom: smooth curve through p1->p2, using p0 and p3 as tangent guides.
static glm::vec3 catmull_rom(glm::vec3 p0, glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, float t)
{
	return 0.5f * (
		(2.f * p1)
		+ (-p0 + p2) * t
		+ (2.f * p0 - 5.f * p1 + 4.f * p2 - p3) * (t * t)
		+ (-p0 + 3.f * p1 - 3.f * p2 + p3) * (t * t * t)
	);
}

// ============================================================
// build_from_spawners
// ============================================================

void BikeCourse::build_from_spawners()
{
	auto spawners = GameplayStatic::find_spawners_in_class("bike_waypoint");
	if (spawners.empty()) {
		sys_print(Warning, "BikeCourse: no 'bike_waypoint' spawners found in level\n");
		return;
	}

	auto road_comps = GameplayStatic::find_components(&RoadNetworkComponent::StaticType);
	if (road_comps.empty()) {
		sys_print(Warning, "BikeCourse: no road network found in level\n");
		return;
	}

	std::sort(spawners.begin(), spawners.end(), [](SpawnerComponent* a, SpawnerComponent* b) {
		return std::atoi(a->get_owner()->get_editor_name().c_str())
		     < std::atoi(b->get_owner()->get_editor_name().c_str());
	});

	std::vector<glm::vec3> hints;
	hints.reserve(spawners.size());
	for (auto* s : spawners)
		hints.push_back(s->get_ws_position());

	build_from_road_network(*static_cast<RoadNetworkComponent*>(road_comps[0]),
	                        hints, sample_step_m, /*loop=*/true);
}

// ============================================================
// rebuild_racing_line
// ============================================================

void BikeCourse::rebuild_racing_line()
{
	if (!is_built || waypoints.empty()) return;
	BikeCourse::compute_racing_line(waypoints, is_loop, rl_k, rl_mass, rl_dt, rl_num_iters);
}

// ============================================================
// project
// ============================================================

float BikeCourse::project(glm::vec3 world_pos, float* out_lateral, int* out_segment) const
{
	if ((int)waypoints.size() < 2) return 0.f;

	const int n        = (int)waypoints.size();
	const int num_segs = is_loop ? n : n - 1;

	float best_dist_sq = FLT_MAX;
	float best_dist_m  = 0.f;
	float best_t       = 0.f;
	int   best_seg     = 0;

	for (int i = 0; i < num_segs; ++i) {
		const glm::vec3& a  = waypoints[i].position;
		const glm::vec3& b  = waypoints[(i + 1) % n].position;
		const glm::vec3  ab = b - a;
		const float ab_sq   = glm::dot(ab, ab);

		float t = 0.f;
		if (ab_sq > 1e-8f)
			t = glm::clamp(glm::dot(world_pos - a, ab) / ab_sq, 0.f, 1.f);

		const glm::vec3 closest = a + t * ab;
		const glm::vec3 diff    = world_pos - closest;
		const float     dist_sq = glm::dot(diff, diff);

		if (dist_sq < best_dist_sq) {
			best_dist_sq = dist_sq;
			best_seg     = i;
			best_t       = t;

			// Arc-length at closest point
			const float seg_arc_start = waypoints[i].dist_from_start;
			const float seg_arc_end   = (i < n - 1) ? waypoints[i + 1].dist_from_start : total_length_m;
			best_dist_m = seg_arc_start + t * (seg_arc_end - seg_arc_start);
		}
	}

	if (out_segment) *out_segment = best_seg;

	if (out_lateral) {
		const int next_idx       = (best_seg + 1) % n;
		const glm::vec3& a       = waypoints[best_seg].position;
		const glm::vec3& b       = waypoints[next_idx].position;
		const glm::vec3  closest = a + best_t * (b - a);
		const glm::vec3  offset  = world_pos - closest;
		const glm::vec3  right   = glm::normalize(
			glm::mix(waypoints[best_seg].right, waypoints[next_idx].right, best_t));
		*out_lateral = glm::dot(offset, right);
	}

	return best_dist_m;
}

// ============================================================
// sample
// ============================================================

BikeWaypoint BikeCourse::sample(float dist_m) const
{
	if (waypoints.empty()) return {};

	const int n = (int)waypoints.size();

	// Wrap for loops; clamp for open courses
	if (is_loop && total_length_m > 0.f)
		dist_m = std::fmod(dist_m, total_length_m);
	if (dist_m < 0.f) dist_m += total_length_m;
	dist_m = glm::clamp(dist_m, 0.f, total_length_m);

	// Find which segment dist_m falls in
	int   seg_idx;
	float seg_arc_start, seg_arc_end;

	if (is_loop && dist_m >= waypoints[n - 1].dist_from_start) {
		// Wrap segment: from waypoints[n-1] back to waypoints[0]
		seg_idx       = n - 1;
		seg_arc_start = waypoints[n - 1].dist_from_start;
		seg_arc_end   = total_length_m;
	} else {
		// Binary search: largest i where waypoints[i].dist_from_start <= dist_m
		int lo = 0, hi = n - 2;
		while (lo < hi) {
			const int mid = (lo + hi + 1) / 2;
			if (waypoints[mid].dist_from_start <= dist_m)
				lo = mid;
			else
				hi = mid - 1;
		}
		seg_idx       = lo;
		seg_arc_start = waypoints[seg_idx].dist_from_start;
		seg_arc_end   = waypoints[seg_idx + 1].dist_from_start;
	}

	const float seg_len = seg_arc_end - seg_arc_start;
	const float t = (seg_len > 1e-6f)
	                ? glm::clamp((dist_m - seg_arc_start) / seg_len, 0.f, 1.f)
	                : 0.f;

	const int   next_idx = (seg_idx + 1) % n;
	const BikeWaypoint& wp0 = waypoints[seg_idx];
	const BikeWaypoint& wp1 = waypoints[next_idx];

	// Catmull-Rom for position.
	// Loops always have valid neighbours via modular indexing.
	// Non-loops fall back to linear at the endpoints.
	glm::vec3 pos;
	if (is_loop) {
		const glm::vec3& p0 = waypoints[(seg_idx - 1 + n) % n].position;
		const glm::vec3& p1 = wp0.position;
		const glm::vec3& p2 = wp1.position;
		const glm::vec3& p3 = waypoints[(seg_idx + 2) % n].position;
		pos = catmull_rom(p0, p1, p2, p3, t);
	} else if (seg_idx == 0 || seg_idx >= n - 2) {
		pos = glm::mix(wp0.position, wp1.position, t);
	} else {
		pos = catmull_rom(waypoints[seg_idx - 1].position,
		                  wp0.position,
		                  wp1.position,
		                  waypoints[seg_idx + 2].position, t);
	}

	BikeWaypoint result;
	result.position             = pos;
	result.forward              = glm::normalize(glm::mix(wp0.forward, wp1.forward, t));
	result.right                = glm::normalize(glm::mix(wp0.right,   wp1.right,   t));
	result.road_half_width      = glm::mix(wp0.road_half_width,      wp1.road_half_width,      t);
	result.dist_from_start      = dist_m;
	result.gradient             = glm::mix(wp0.gradient,             wp1.gradient,             t);
	result.racing_line_lateral  = glm::mix(wp0.racing_line_lateral,  wp1.racing_line_lateral,  t);
	result.racing_line_pos      = glm::mix(wp0.racing_line_pos,      wp1.racing_line_pos,      t);
	return result;
}

// ============================================================
// lookahead
// ============================================================

glm::vec3 BikeCourse::lookahead(float from_dist_m, float ahead_m) const
{
	float target = from_dist_m + ahead_m;
	if (is_loop && total_length_m > 0.f)
		target = std::fmod(target, total_length_m);
	return sample(target).position;
}

// ============================================================
// min_turn_radius_ahead
// ============================================================

float BikeCourse::min_turn_radius_ahead(float from_dist_m, float ahead_m) const
{
	if (!is_built || waypoints.size() < 2) return 1e6f;

	static constexpr float STEP = 2.f;
	float min_r       = 1e6f;
	float prev_yaw    = FLT_MAX;

	for (float d = 0.f; d <= ahead_m + STEP; d += STEP) {
		const BikeWaypoint wp = sample(from_dist_m + d);
		const float yaw = std::atan2(wp.forward.x, wp.forward.z);

		if (prev_yaw < FLT_MAX) {
			float dyaw = yaw - prev_yaw;
			// Wrap to [-π, π]
			if (dyaw >  glm::pi<float>()) dyaw -= 2.f * glm::pi<float>();
			if (dyaw < -glm::pi<float>()) dyaw += 2.f * glm::pi<float>();
			const float curvature = glm::abs(dyaw) / STEP;
			if (curvature > 1e-5f)
				min_r = glm::min(min_r, 1.f / curvature);
		}
		prev_yaw = yaw;
	}
	return min_r;
}

// ============================================================
// racing_line_lookahead
// ============================================================

glm::vec3 BikeCourse::racing_line_lookahead(float from_dist_m, float ahead_m) const
{
	float target = from_dist_m + ahead_m;
	if (is_loop && total_length_m > 0.f)
		target = std::fmod(target, total_length_m);
	return sample(target).racing_line_pos;
}
