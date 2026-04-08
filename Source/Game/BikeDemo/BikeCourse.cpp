#include "BikeCourse.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Game/Entity.h"
#include "Debug.h"
#include "Framework/Util.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cstdlib>
#include <cfloat>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Catmull-Rom spline through four control points, parameter t in [0,1] covers p1->p2.
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

	// Sort by editor_name parsed as integer
	std::sort(spawners.begin(), spawners.end(), [](SpawnerComponent* a, SpawnerComponent* b) {
		int ia = std::atoi(a->get_owner()->get_editor_name().c_str());
		int ib = std::atoi(b->get_owner()->get_editor_name().c_str());
		return ia < ib;
	});

	std::vector<glm::vec3> positions;
	std::vector<float>     half_widths;
	positions.reserve(spawners.size());
	half_widths.reserve(spawners.size());

	for (auto* s : spawners) {
		positions.push_back(s->get_ws_position());
		float hw = s->has_key("road_half_width") ? s->get_float("road_half_width") : 4.f;
		half_widths.push_back(hw);
	}

	build(positions, half_widths);
}

// ============================================================
// build
// ============================================================

void BikeCourse::build(const std::vector<glm::vec3>& positions, const std::vector<float>& road_half_widths)
{
	waypoints.clear();
	total_length_m = 0.f;
	is_built       = false;

	const int n = (int)positions.size();
	if (n < 2) {
		sys_print(Warning, "BikeCourse::build: need at least 2 waypoints, got %d\n", n);
		return;
	}

	waypoints.resize(n);

	for (int i = 0; i < n; ++i) {
		BikeWaypoint& wp    = waypoints[i];
		wp.position         = positions[i];
		wp.road_half_width  = (i < (int)road_half_widths.size()) ? road_half_widths[i] : 4.f;

		// Forward: direction toward the next node (last node reuses direction from previous)
		glm::vec3 fwd;
		if (i < n - 1)
			fwd = positions[i + 1] - positions[i];
		else
			fwd = positions[i] - positions[i - 1];

		const float fwd_len = glm::length(fwd);
		if (fwd_len > 1e-4f)
			fwd /= fwd_len;
		else
			fwd = (i > 0) ? waypoints[i - 1].forward : glm::vec3(0, 0, 1);

		wp.forward  = fwd;
		wp.gradient = glm::asin(glm::clamp(fwd.y, -1.f, 1.f));

		// Road-right: perpendicular to forward in the horizontal plane
		glm::vec3 right = glm::cross(WORLD_UP, fwd);
		const float rlen = glm::length(right);
		wp.right = (rlen > 1e-4f) ? (right / rlen) : glm::vec3(1, 0, 0);

		// Arc-length accumulation
		if (i == 0) {
			wp.dist_from_start = 0.f;
		} else {
			wp.dist_from_start = waypoints[i - 1].dist_from_start
			                     + glm::distance(positions[i], positions[i - 1]);
		}
	}

	total_length_m = waypoints.back().dist_from_start;
	is_built       = true;

	sys_print(Info, "BikeCourse::build: %d waypoints, %.0f m total length\n", n, total_length_m);
}

// ============================================================
// project
// ============================================================

float BikeCourse::project(glm::vec3 world_pos, float* out_lateral, int* out_segment, int hint_segment) const
{
	if ((int)waypoints.size() < 2) return 0.f;

	const int num_segs = (int)waypoints.size() - 1;

	float best_dist_sq = FLT_MAX;
	float best_dist_m  = 0.f;
	float best_t       = 0.f;
	int   best_seg     = 0;

	for (int i = 0; i < num_segs; ++i) {
		const glm::vec3& a  = waypoints[i].position;
		const glm::vec3& b  = waypoints[i + 1].position;
		const glm::vec3  ab = b - a;
		const float ab_len_sq = glm::dot(ab, ab);

		float t = 0.f;
		if (ab_len_sq > 1e-8f)
			t = glm::clamp(glm::dot(world_pos - a, ab) / ab_len_sq, 0.f, 1.f);

		const glm::vec3 closest  = a + t * ab;
		const glm::vec3 diff     = world_pos - closest;
		const float     dist_sq  = glm::dot(diff, diff);

		if (dist_sq < best_dist_sq) {
			best_dist_sq = dist_sq;
			best_seg     = i;
			best_t       = t;

			const float seg_len = waypoints[i + 1].dist_from_start - waypoints[i].dist_from_start;
			best_dist_m = waypoints[i].dist_from_start + t * seg_len;
		}
	}

	if (out_segment) *out_segment = best_seg;

	if (out_lateral) {
		const glm::vec3& a  = waypoints[best_seg].position;
		const glm::vec3& b  = waypoints[best_seg + 1].position;
		const glm::vec3  closest = a + best_t * (b - a);
		const glm::vec3  offset  = world_pos - closest;

		// Interpolate the right vector across the segment
		const glm::vec3 right = glm::normalize(
			glm::mix(waypoints[best_seg].right, waypoints[best_seg + 1].right, best_t));
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

	dist_m = glm::clamp(dist_m, 0.f, total_length_m);

	const int n = (int)waypoints.size();

	// Binary search: find segment i such that waypoints[i].dist_from_start <= dist_m
	int lo = 0, hi = n - 2;
	while (lo < hi) {
		const int mid = (lo + hi + 1) / 2;
		if (waypoints[mid].dist_from_start <= dist_m)
			lo = mid;
		else
			hi = mid - 1;
	}
	const int i = lo;

	const BikeWaypoint& wp0 = waypoints[i];
	const BikeWaypoint& wp1 = waypoints[glm::min(i + 1, n - 1)];
	const float seg_len = wp1.dist_from_start - wp0.dist_from_start;
	const float t = (seg_len > 1e-6f) ? glm::clamp((dist_m - wp0.dist_from_start) / seg_len, 0.f, 1.f) : 0.f;

	// Catmull-Rom for position, falls back to linear at the endpoints
	glm::vec3 pos;
	if (i == 0 || i >= n - 2) {
		pos = glm::mix(wp0.position, wp1.position, t);
	} else {
		pos = catmull_rom(
			waypoints[i - 1].position,
			wp0.position,
			wp1.position,
			waypoints[glm::min(i + 2, n - 1)].position,
			t);
	}

	BikeWaypoint result;
	result.position        = pos;
	result.forward         = glm::normalize(glm::mix(wp0.forward, wp1.forward, t));
	result.right           = glm::normalize(glm::mix(wp0.right,   wp1.right,   t));
	result.road_half_width = glm::mix(wp0.road_half_width, wp1.road_half_width, t);
	result.dist_from_start = dist_m;
	result.gradient        = glm::mix(wp0.gradient, wp1.gradient, t);
	return result;
}

// ============================================================
// lookahead
// ============================================================

glm::vec3 BikeCourse::lookahead(float from_dist_m, float ahead_m) const
{
	return sample(from_dist_m + ahead_m).position;
}

// ============================================================
// debug_draw
// ============================================================

void BikeCourse::debug_draw() const
{
	if (!is_built || (int)waypoints.size() < 2) return;

	const int n = (int)waypoints.size();

	for (int i = 0; i < n - 1; ++i) {
		const BikeWaypoint& wp = waypoints[i];

		// Colour by gradient: yellow = uphill, blue = downhill, green = flat
		const float grad_deg = glm::degrees(wp.gradient);
		Color32 line_color;
		if      (grad_deg >  3.f) line_color = Color32(0xff, 0xff, 0x00, 0xff); // yellow
		else if (grad_deg < -2.f) line_color = Color32(0x66, 0xaa, 0xff, 0xff); // blue
		else                      line_color = COLOR_GREEN;

		Debug::add_line(wp.position, waypoints[i + 1].position, line_color, -1.f);

		// Road-width tick marks every 5th waypoint
		if (i % 5 == 0) {
			const glm::vec3 left_edge  = wp.position - wp.right * wp.road_half_width;
			const glm::vec3 right_edge = wp.position + wp.right * wp.road_half_width;
			Debug::add_line(left_edge, right_edge, Color32(0xff, 0xff, 0xff, 0x66), -1.f);
		}
	}
}
