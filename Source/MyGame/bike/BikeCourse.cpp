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
#include <fstream>

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
	BikeCourse::compute_racing_line(waypoints, is_loop, rl_k, rl_mass, rl_dt, rl_num_iters, rl_smooth_passes, rl_smooth_w, rl_margin);
}

// ============================================================
// project
// ============================================================

// ============================================================
// project — internal helpers
// ============================================================

// Binary search: largest segment index i where waypoints[i].dist_from_start <= d.
// Assumes waypoints are sorted by dist_from_start (always true after build).
static int arc_to_seg_idx(const std::vector<BikeWaypoint>& wps, float d)
{
	const int n = (int)wps.size();
	int lo = 0, hi = n - 2;
	while (lo < hi) {
		const int mid = (lo + hi + 1) / 2;
		if (wps[mid].dist_from_start <= d) lo = mid;
		else                               hi = mid - 1;
	}
	return lo;
}

// ============================================================
// project
// ============================================================

float BikeCourse::project(glm::vec3 world_pos, float* out_lateral, int* out_segment,
                           float prev_dist_m) const
{
	if ((int)waypoints.size() < 2) return 0.f;

	const int n        = (int)waypoints.size();
	const int num_segs = is_loop ? n : n - 1;

	// Score one segment by plain world-space dist_sq. Sets t and raw_dist_sq.
	// No heading weight: fillet arcs rotate heading continuously, so any
	// heading penalty destabilises the projection as the bike rounds the curve.
	// The arc-length window (below) is sufficient to prevent far-loop aliasing.
	auto score_seg = [&](int i, float& out_t, float& raw_dist_sq) -> float {
		const glm::vec3& a  = waypoints[i].position;
		const glm::vec3& b  = waypoints[(i + 1) % n].position;
		const glm::vec3  ab = b - a;
		const float      ab_sq = glm::dot(ab, ab);

		float t = 0.f;
		if (ab_sq > 1e-8f)
			t = glm::clamp(glm::dot(world_pos - a, ab) / ab_sq, 0.f, 1.f);

		out_t = t;
		const glm::vec3 closest = a + t * ab;
		raw_dist_sq             = glm::dot(world_pos - closest, world_pos - closest);
		return raw_dist_sq;
	};

	float best_score      = FLT_MAX;
	float best_raw_dist_sq = FLT_MAX;
	float best_dist_m     = 0.f;
	float best_t          = 0.f;
	int   best_seg        = 0;

	// Update best if this segment scores better.
	auto try_seg = [&](int i) {
		float t, raw_dsq;
		const float score = score_seg(i, t, raw_dsq);
		if (score < best_score) {
			best_score       = score;
			best_raw_dist_sq = raw_dsq;
			best_seg         = i;
			best_t           = t;
			const float arc0 = waypoints[i].dist_from_start;
			const float arc1 = (i < n - 1) ? waypoints[i + 1].dist_from_start : total_length_m;
			best_dist_m      = arc0 + t * (arc1 - arc0);
		}
	};

	// Search a contiguous range of segment indices [i_lo, i_hi) with loop-wrap.
	auto search_range = [&](int i_lo, int i_hi) {
		for (int ii = i_lo; ii < i_hi; ++ii)
			try_seg((ii % n + n) % n);
	};

	// --- Windowed search (primary path when prev_dist_m is known) ---
	//
	// Restricts the search to an arc-length window around the previous position.
	// Prevents the global scan from matching a distant part of the loop that
	// happens to be close in world-space (the classic loop-aliasing bug that
	// causes course_dist_m to teleport to the far side of the circuit).
	//
	// Falls back to the full global search only if the rider is more than
	// FALLBACK_DIST_M from every segment in the window (off-track recovery).

	static constexpr float BACK_M         = 10.f;   // allow 10 m backward (braking / noise)
	static constexpr float FORWARD_M      = 50.f;   // allow 50 m forward  (high-speed frame)
	static constexpr float FALLBACK_DIST_M = 30.f;  // global fallback if >30 m from road

	bool used_window = false;

	if (prev_dist_m >= 0.f && total_length_m > 1.f) {
		const float win_lo = prev_dist_m - BACK_M;
		const float win_hi = prev_dist_m + FORWARD_M;

		if (!is_loop) {
			const int i0 = arc_to_seg_idx(waypoints, glm::max(win_lo, 0.f));
			const int i1 = arc_to_seg_idx(waypoints, glm::min(win_hi, total_length_m)) + 1;
			search_range(i0, i1);
		} else {
			// Loop: window may wrap past 0 or past total_length_m.
			if (win_lo < 0.f) {
				// Wraps past start — two sub-ranges.
				const float lo_w = win_lo + total_length_m;
				search_range(arc_to_seg_idx(waypoints, lo_w), n);
				search_range(0, arc_to_seg_idx(waypoints, win_hi) + 1);
			} else if (win_hi >= total_length_m) {
				// Wraps past end — two sub-ranges.
				const float hi_w = win_hi - total_length_m;
				search_range(arc_to_seg_idx(waypoints, win_lo), n);
				search_range(0, arc_to_seg_idx(waypoints, hi_w) + 1);
			} else {
				// No wrap.
				search_range(arc_to_seg_idx(waypoints, win_lo),
				             arc_to_seg_idx(waypoints, win_hi) + 1);
			}
		}
		used_window = true;
	}

	// --- Global fallback ---
	// Used on the first frame (no prev_dist_m) and as a recovery search when
	// the rider is far from every segment in the window (crash / teleport).
	if (!used_window || best_raw_dist_sq > FALLBACK_DIST_M * FALLBACK_DIST_M) {
		best_score = best_raw_dist_sq = FLT_MAX;
		search_range(0, num_segs);
	}

	// --- Fill outputs ---
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

float BikeCourse::get_road_half_width(int segment) const
{
	if (segment < 0 || segment >= (int)waypoints.size())
		return 2.f;
	return waypoints.at(segment).road_half_width;
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
	// Derive racing_line_pos from the Catmull-Rom position rather than linearly
	// interpolating stored world-space positions — avoids the racing line cutting
	// across curves (especially visible at the loop seam on looping circuits).
	result.racing_line_pos      = result.position + result.right * result.racing_line_lateral;
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

	// Central-difference curvature: sample yaw at d±AVG_HALF_M instead of adjacent
	// steps. This averages the heading change over a 6m window, washing out kinks
	// narrower than that without distorting genuine corners.
	static constexpr float STEP       = 2.f;
	static constexpr float AVG_HALF_M = 3.f;

	float min_r = 1e6f;

	for (float d = 0.f; d <= ahead_m + STEP; d += STEP) {
		const float yaw_back = std::atan2(
			sample(from_dist_m + d - AVG_HALF_M).forward.x,
			sample(from_dist_m + d - AVG_HALF_M).forward.z);
		const float yaw_fwd  = std::atan2(
			sample(from_dist_m + d + AVG_HALF_M).forward.x,
			sample(from_dist_m + d + AVG_HALF_M).forward.z);

		float dyaw = yaw_fwd - yaw_back;
		if (dyaw >  glm::pi<float>()) dyaw -= 2.f * glm::pi<float>();
		if (dyaw < -glm::pi<float>()) dyaw += 2.f * glm::pi<float>();

		const float curvature = glm::abs(dyaw) / (2.f * AVG_HALF_M);
		if (curvature > 1e-5f)
			min_r = glm::min(min_r, 1.f / curvature);
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

// ============================================================
// dump_audit_csv — racing-line / fillet diagnostic dump
// ============================================================
//
// Per-row diagnostics so glitches localise:
//   source             0=edge sample, 1=fillet arc sample
//   step_m             distance to previous waypoint (jumps in spacing show up as outliers)
//   d_heading_deg      change in xz-heading vs previous waypoint
//   curvature_inv_m    d_heading_rad / step_m (1/R), high values = tight bend
//   alpha_clamped      1 if racing_line_lateral is at the ±0.9·hw cap (line saturating)
//   d_rl_lat           change in racing_line_lateral vs previous waypoint
//   d_rl_world_m       3D distance between consecutive racing_line_pos values
//
// Sort by abs(d_rl_world_m) or abs(d_heading_deg) to find the worst rows.
bool BikeCourse::dump_audit_csv(const char* path) const
{
	std::ofstream f(path);
	if (!f.is_open()) {
		sys_print(Warning, "BikeCourse::dump_audit_csv: failed to open %s\n", path);
		return false;
	}

	// Build per-waypoint source flag from arc_ranges (handles loop-seam wrap).
	const int n = (int)waypoints.size();
	std::vector<int> src(n, 0);
	for (const auto& [a, b] : arc_ranges) {
		if (a <= b) {
			for (int i = a; i <= b && i < n; ++i) src[i] = 1;
		} else {
			for (int i = a; i < n; ++i) src[i] = 1;
			for (int i = 0; i <= b && i < n; ++i) src[i] = 1;
		}
	}

	f << "wp_idx,source,pos_x,pos_y,pos_z,fwd_x,fwd_y,fwd_z,right_x,right_z,"
	     "road_hw,dist_from_start,step_m,d_heading_deg,curvature_inv_m,"
	     "rl_lateral,alpha_clamped,rl_pos_x,rl_pos_y,rl_pos_z,d_rl_lat,d_rl_world_m\n";

	auto wrap_prev = [&](int i) {
		if (i > 0) return i - 1;
		return is_loop ? (n - 1) : 0;
	};

	const float to_deg = 180.f / glm::pi<float>();
	for (int i = 0; i < n; ++i) {
		const BikeWaypoint& wp = waypoints[i];
		const int   pi      = wrap_prev(i);
		const BikeWaypoint& wprev = waypoints[pi];
		const bool  has_prev = (i > 0) || is_loop;

		const float step_m = has_prev ? glm::distance(wp.position, wprev.position) : 0.f;

		// xz-only heading delta
		float d_head_deg = 0.f;
		if (has_prev) {
			const glm::vec2 a(wprev.forward.x, wprev.forward.z);
			const glm::vec2 b(wp.forward.x,    wp.forward.z);
			const float al = glm::length(a), bl = glm::length(b);
			if (al > 1e-4f && bl > 1e-4f) {
				const float dot = glm::clamp(glm::dot(a, b) / (al * bl), -1.f, 1.f);
				d_head_deg = std::acos(dot) * to_deg;
			}
		}
		const float curvature = (step_m > 1e-3f) ? (d_head_deg / to_deg / step_m) : 0.f;

		const float hw_cap     = wp.road_half_width * 0.9f;
		const int   clamped    = (std::abs(wp.racing_line_lateral) >= hw_cap - 1e-3f) ? 1 : 0;
		const float d_rl_lat   = has_prev ? (wp.racing_line_lateral - wprev.racing_line_lateral) : 0.f;
		const float d_rl_world = has_prev ? glm::distance(wp.racing_line_pos, wprev.racing_line_pos) : 0.f;

		f << i                   << ',' << src[i]               << ','
		  << wp.position.x       << ',' << wp.position.y        << ',' << wp.position.z      << ','
		  << wp.forward.x        << ',' << wp.forward.y         << ',' << wp.forward.z       << ','
		  << wp.right.x          << ',' << wp.right.z           << ','
		  << wp.road_half_width  << ',' << wp.dist_from_start   << ','
		  << step_m              << ',' << d_head_deg           << ',' << curvature          << ','
		  << wp.racing_line_lateral << ',' << clamped           << ','
		  << wp.racing_line_pos.x   << ',' << wp.racing_line_pos.y << ',' << wp.racing_line_pos.z << ','
		  << d_rl_lat            << ',' << d_rl_world           << '\n';
	}

	sys_print(Info, "BikeCourse::dump_audit_csv: wrote %d rows to %s (%d arcs)\n",
	          n, path, (int)arc_ranges.size());
	return true;
}
