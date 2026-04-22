#include "BikeCourse.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Game/Entity.h"
#include "Debug.h"
#include "Framework/Util.h"
#include "Physics/ChannelsAndPresets.h"
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Terrain snap ray: cast this far above and below the placed waypoint position.
static constexpr float SNAP_CAST_UP   = 200.f;
static constexpr float SNAP_CAST_DOWN = 200.f;

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

	// Sort by editor_name parsed as integer
	std::sort(spawners.begin(), spawners.end(), [](SpawnerComponent* a, SpawnerComponent* b) {
		int ia = std::atoi(a->get_owner()->get_editor_name().c_str());
		int ib = std::atoi(b->get_owner()->get_editor_name().c_str());
		return ia < ib;
	});

	// Extract positions and road half-widths
	std::vector<glm::vec3> positions;
	std::vector<float>     half_widths;
	positions.reserve(spawners.size());
	half_widths.reserve(spawners.size());

	for (auto* s : spawners) {
		positions.push_back(s->get_ws_position());
		float hw = s->has_key("road_half_width") ? s->get_float("road_half_width") : 4.f;
		half_widths.push_back(hw);
	}

	// Terrain-snap each position downward
	const int terrain_mask = (1 << (int)PL::Default) | (1 << (int)PL::StaticObject);
	int snapped = 0;
	for (auto& pos : positions) {
		const glm::vec3 ray_start = pos + glm::vec3(0.f, SNAP_CAST_UP,   0.f);
		const glm::vec3 ray_end   = pos - glm::vec3(0.f, SNAP_CAST_DOWN, 0.f);
		HitResult hit = GameplayStatic::cast_ray(ray_start, ray_end, terrain_mask, nullptr);
		if (hit.hit) {
			pos.y = hit.pos.y;
			++snapped;
		}
	}
	sys_print(Info, "BikeCourse: snapped %d / %d waypoints to terrain\n", snapped, (int)positions.size());

	build(positions, half_widths, /*loop=*/true);

	if (is_built)
		BikeCourse::compute_racing_line(waypoints, is_loop);
}

// ============================================================
// build
// ============================================================

void BikeCourse::build(const std::vector<glm::vec3>& positions,
                       const std::vector<float>&     road_half_widths,
                       bool                          loop)
{
	waypoints.clear();
	total_length_m = 0.f;
	is_built       = false;
	is_loop        = loop;

	const int n = (int)positions.size();
	if (n < 2) {
		sys_print(Warning, "BikeCourse::build: need at least 2 waypoints, got %d\n", n);
		return;
	}

	waypoints.resize(n);

	for (int i = 0; i < n; ++i) {
		BikeWaypoint& wp   = waypoints[i];
		wp.position        = positions[i];
		wp.road_half_width = (i < (int)road_half_widths.size()) ? road_half_widths[i] : 4.f;

		// Forward: direction toward the next node.
		// For the last node in a non-loop, reuse the direction from the previous node.
		// For the last node in a loop, point toward the first node.
		glm::vec3 fwd;
		if (i < n - 1)
			fwd = positions[i + 1] - positions[i];
		else if (loop)
			fwd = positions[0] - positions[i];
		else
			fwd = positions[i] - positions[i - 1];

		const float fwd_len = glm::length(fwd);
		if (fwd_len > 1e-4f)
			fwd /= fwd_len;
		else
			fwd = (i > 0) ? waypoints[i - 1].forward : glm::vec3(0, 0, 1);

		wp.forward  = fwd;
		wp.gradient = glm::asin(glm::clamp(fwd.y, -1.f, 1.f));

		// Road-right: perpendicular to forward in the horizontal plane.
		glm::vec3 right  = glm::cross(WORLD_UP, fwd);
		const float rlen = glm::length(right);
		wp.right = (rlen > 1e-4f) ? (right / rlen) : glm::vec3(1, 0, 0);

		// Arc-length from start
		if (i == 0)
			wp.dist_from_start = 0.f;
		else
			wp.dist_from_start = waypoints[i - 1].dist_from_start + glm::distance(positions[i], positions[i - 1]);
	}

	// Total length: add wrap segment if this is a loop
	if (loop)
		total_length_m = waypoints.back().dist_from_start + glm::distance(positions[n - 1], positions[0]);
	else
		total_length_m = waypoints.back().dist_from_start;

	is_built = true;

	sys_print(Info, "BikeCourse::build: %d waypoints, %.0f m total%s\n",
	          n, total_length_m, loop ? " (loop)" : "");
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

// ============================================================
// build_from_road_network — internal helpers
// ============================================================

// Return index into nodes vector for a given node id, or -1 if not found.
static int find_node_idx(const std::vector<RoadNode>& nodes, int id)
{
	for (int i = 0; i < (int)nodes.size(); ++i)
		if (nodes[i].id == id) return i;
	return -1;
}

// A* on the road graph. Returns ordered list of node IDs from start to goal.
// Edges are treated as undirected. Returns empty on failure.
static std::vector<int> road_astar(
	const std::vector<RoadNode>& nodes,
	const std::vector<RoadEdge>& edges,
	int start_id, int goal_id)
{
	if (start_id == goal_id) return { start_id };

	// Position lookup
	std::unordered_map<int, glm::vec3> pos_map;
	for (const auto& n : nodes) pos_map[n.id] = n.position;

	if (!pos_map.count(start_id) || !pos_map.count(goal_id)) return {};

	// Adjacency list: node_id -> [(neighbor_id, cost)]
	std::unordered_map<int, std::vector<std::pair<int,float>>> adj;
	for (const auto& e : edges) {
		if (!pos_map.count(e.node_a_id) || !pos_map.count(e.node_b_id)) continue;
		const float cost = glm::distance(pos_map[e.node_a_id], pos_map[e.node_b_id]);
		adj[e.node_a_id].emplace_back(e.node_b_id, cost);
		adj[e.node_b_id].emplace_back(e.node_a_id, cost);
	}

	const glm::vec3 goal_pos = pos_map[goal_id];

	// Priority queue: (f_cost, node_id)
	using Entry = std::pair<float, int>;
	std::priority_queue<Entry, std::vector<Entry>, std::greater<Entry>> open;

	std::unordered_map<int, float>  g;
	std::unordered_map<int, int>    came_from;
	std::unordered_set<int>         closed;

	g[start_id] = 0.f;
	open.push({ glm::distance(pos_map[start_id], goal_pos), start_id });

	while (!open.empty()) {
		auto [f, cur] = open.top(); open.pop();
		if (closed.count(cur)) continue;
		closed.insert(cur);
		if (cur == goal_id) break;

		const float cur_g = g.count(cur) ? g[cur] : FLT_MAX;
		for (auto [nb, cost] : adj[cur]) {
			if (closed.count(nb)) continue;
			const float new_g = cur_g + cost;
			if (!g.count(nb) || new_g < g[nb]) {
				g[nb]         = new_g;
				came_from[nb] = cur;
				const float h = glm::distance(pos_map[nb], goal_pos);
				open.push({ new_g + h, nb });
			}
		}
	}

	if (!came_from.count(goal_id)) return {};  // no path

	std::vector<int> path;
	for (int cur = goal_id; cur != start_id; ) {
		path.push_back(cur);
		if (!came_from.count(cur)) return {};
		cur = came_from[cur];
	}
	path.push_back(start_id);
	std::reverse(path.begin(), path.end());
	return path;
}

// Find the node id nearest to a world position.
static int nearest_node_id(const std::vector<RoadNode>& nodes, glm::vec3 pos)
{
	int   best_id  = -1;
	float best_sq  = FLT_MAX;
	for (const auto& n : nodes) {
		const float sq = glm::dot(n.position - pos, n.position - pos);
		if (sq < best_sq) { best_sq = sq; best_id = n.id; }
	}
	return best_id;
}

// Densely sample a single road edge at step_m intervals, appending to out_positions/out_widths.
// node_from/node_to are positions for the edge endpoints.
// skip_first skips the very first sample (avoids duplicate junction points).
static void sample_edge(
	const RoadEdge& edge,
	glm::vec3 pos_a, glm::vec3 pos_b,
	bool going_a_to_b,   // traversal direction
	float step_m,
	bool skip_first,
	std::vector<glm::vec3>& out_positions,
	std::vector<float>&     out_widths)
{
	const float half_w = edge.width * 0.5f;

	if (!edge.curved) {
		// Straight edge: uniform subdivisions
		const float seg_len = glm::distance(pos_a, pos_b);
		if (seg_len < 1e-4f) return;
		const int steps = glm::max(1, (int)(seg_len / step_m));
		for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
			const float t = (float)i / steps;
			out_positions.push_back(glm::mix(pos_a, pos_b, t));
			out_widths.push_back(half_w);
		}
	} else {
		// Cubic Bezier: determine control points based on traversal direction.
		// pos_a is always the traversal start, pos_b the traversal end.
		// When going A→B: ctrl_a is near start, ctrl_b is near end.
		// When going B→A: ctrl_b is near start (node_b), ctrl_a is near end (node_a).
		glm::vec3 p0, p1, p2, p3;
		if (going_a_to_b) {
			p0 = pos_a; p1 = edge.ctrl_a; p2 = edge.ctrl_b; p3 = pos_b;
		} else {
			p0 = pos_a; p1 = edge.ctrl_b; p2 = edge.ctrl_a; p3 = pos_b;
		}

		// Pre-sample to build arc-length table
		static constexpr int PRESAMPLE = 100;
		std::vector<float> arc(PRESAMPLE + 1, 0.f);
		std::vector<glm::vec3> pts(PRESAMPLE + 1);
		pts[0] = p0;
		for (int k = 1; k <= PRESAMPLE; ++k) {
			const float t = (float)k / PRESAMPLE;
			const float u = 1.f - t;
			pts[k] = u*u*u*p0 + 3.f*u*u*t*p1 + 3.f*u*t*t*p2 + t*t*t*p3;
			arc[k] = arc[k-1] + glm::distance(pts[k], pts[k-1]);
		}
		const float total_len = arc[PRESAMPLE];
		if (total_len < 1e-4f) return;

		const int steps = glm::max(1, (int)(total_len / step_m));
		for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
			const float want = (float)i / steps * total_len;
			// Binary search in arc table
			int lo = 0, hi = PRESAMPLE;
			while (lo < hi) {
				const int mid = (lo + hi + 1) / 2;
				(arc[mid] <= want) ? lo = mid : hi = mid - 1;
			}
			glm::vec3 pt;
			if (lo >= PRESAMPLE) {
				pt = pts[PRESAMPLE];
			} else {
				const float seg = arc[lo + 1] - arc[lo];
				const float frac = (seg > 1e-6f) ? (want - arc[lo]) / seg : 0.f;
				pt = glm::mix(pts[lo], pts[lo + 1], frac);
			}
			out_positions.push_back(pt);
			out_widths.push_back(half_w);
		}
	}
}

// ============================================================
// Racing line: box-blur helper
// ============================================================

static void rl_smooth_scalar(std::vector<float>& v, bool loop, int r)
{
	const int n = (int)v.size();
	std::vector<float> out(n);
	for (int i = 0; i < n; ++i) {
		float sum = 0.f;
		int   cnt = 0;
		for (int j = -r; j <= r; ++j) {
			int k = i + j;
			if      (loop)            k = (k % n + n) % n;
			else if (k < 0 || k >= n) continue;
			sum += v[k];
			++cnt;
		}
		out[i] = cnt > 0 ? sum / (float)cnt : v[i];
	}
	v = out;
}

// ============================================================
// compute_racing_line — production-quality
// ============================================================
//
// Matches the approach used by iRacing / Gran Turismo / F1-series AI:
//
// Phase 1 — Signed curvature profile (Menger formula, noise-smoothed).
//           κ > 0 = left turn  (inside = −right)
//           κ < 0 = right turn (inside = +right)
//
// Phase 2 — Corner detection. Apex = local max |κ| in each corner
//           region.  Physics-based braking/accel distances come from
//           bicycle tyre dynamics (mu_lat, g, max_decel/accel).
//
// Phase 3 — Outside-inside-outside target offsets.
//           Entry zone  → outside edge (opposite apex side).
//           Apex        → inside  edge.
//           Exit zone   → outside edge.
//           Smoothstep blends entry→apex and apex→exit.
//           Dominant-corner rule merges overlapping zones.
//
// Phase 4 — Wide box-blur smooths the raw targets so compound corners
//           and chicanes blend naturally.
//
// Phase 5 — Gauss-Seidel minimum-curvature relaxation seeded from the
//           corner-aware targets.  Eliminates residual kinks and
//           produces the globally minimum-curvature path that stays
//           inside the road corridor — naturally outside-in-outside.
//
// Bike dynamics constants (matched to BikeObject):
//   mu_lateral = 0.55   road tyre lateral friction
//   g          = 9.81 m/s²
//   max_decel  = 7.0  m/s²  (hard brake — matches BikeObject)
//   max_accel  = 3.0  m/s²  (conservative exit acceleration)

void BikeCourse::compute_racing_line(std::vector<BikeWaypoint>& wps, bool loop,
                                     float strength, int num_iters)
{
	const int n = (int)wps.size();
	if (n < 3) return;

	// Total arc-length used only for loop-wrap arithmetic in Phase 3.
	const float total_len = loop && n > 1
		? wps.back().dist_from_start + glm::distance(wps.back().position, wps[0].position)
		: wps.back().dist_from_start;

	// ----------------------------------------------------------------
	// Phase 1 — Signed curvature at every waypoint.
	// ----------------------------------------------------------------
	std::vector<float> kappa(n, 0.f);
	for (int i = 0; i < n; ++i) {
		const int im = loop ? (i - 1 + n) % n : std::max(0, i - 1);
		const int ip = loop ? (i + 1) % n     : std::min(n - 1, i + 1);
		if (im == i || ip == i) continue;

		const glm::vec3 a  = wps[im].position;
		const glm::vec3 b  = wps[i].position;
		const glm::vec3 c  = wps[ip].position;
		const glm::vec3 ba = a - b;
		const glm::vec3 bc = c - b;
		const float la  = glm::length(ba);
		const float lc  = glm::length(bc);
		const float ac  = glm::distance(a, c);
		if (la < 1e-4f || lc < 1e-4f || ac < 1e-4f) continue;

		const glm::vec3 cross       = glm::cross(ba / la, bc / lc);
		const float kappa_unsigned  = glm::length(cross) / (0.5f * ac);
		// cross.y < 0 → counterclockwise in XZ → left turn → κ > 0
		const float sign_val = (cross.y < 0.f) ? 1.f : -1.f;
		kappa[i] = sign_val * kappa_unsigned;
	}

	// Two box-blur passes to kill quantisation noise from dense sampling
	// without collapsing tight-corner peaks.
	rl_smooth_scalar(kappa, loop, 6);
	rl_smooth_scalar(kappa, loop, 6);

	// ----------------------------------------------------------------
	// Phase 2 — Corner detection: contiguous run of |κ| > threshold.
	// ----------------------------------------------------------------
	static constexpr float KAPPA_THRESH  = 0.005f; // radius < 200 m → corner
	static constexpr float MU_LATERAL    = 0.55f;
	static constexpr float G_ACC         = 9.81f;
	static constexpr float BIKE_DECEL    = 7.0f;
	static constexpr float BIKE_ACCEL    = 3.0f;

	struct Corner {
		int   apex_idx;
		float apex_kappa;   // signed κ at apex
		float braking_dist; // m before apex (turn-in to apex)
		float accel_dist;   // m after  apex (apex to exit complete)
	};
	std::vector<Corner> corners;

	{
		int i = 0;
		while (i < n) {
			if (std::abs(kappa[i]) < KAPPA_THRESH) { ++i; continue; }

			int   apex_idx = i;
			float peak_abs = std::abs(kappa[i]);
			while (i < n && std::abs(kappa[i]) >= KAPPA_THRESH) {
				if (std::abs(kappa[i]) > peak_abs) {
					peak_abs = std::abs(kappa[i]);
					apex_idx = i;
				}
				++i;
			}
			if (peak_abs < 1e-6f) continue;

			const float radius  = 1.f / peak_abs;
			const float v_apex  = std::sqrt(MU_LATERAL * G_ACC * radius);
			const float bk_dist = glm::clamp(v_apex * v_apex / (2.f * BIKE_DECEL), 5.f, 120.f);
			const float ac_dist = glm::clamp(v_apex * v_apex / (2.f * BIKE_ACCEL), 5.f, 150.f);
			corners.push_back({ apex_idx, kappa[apex_idx], bk_dist, ac_dist });
		}

		// For loops: check if the corner at the very end connects to one at the very
		// start (seam-crossing corner).  Merge by keeping only the sharper apex.
		if (loop && corners.size() >= 2) {
			const Corner& first = corners.front();
			const Corner& last  = corners.back();
			const float d_first = wps[first.apex_idx].dist_from_start;
			const float d_last  = wps[last.apex_idx].dist_from_start;
			const bool same_sign = (first.apex_kappa >= 0.f) == (last.apex_kappa >= 0.f);
			// If the last apex is within accel_dist of the seam AND first within braking_dist of seam
			if (same_sign && d_last + last.accel_dist >= total_len && d_first <= first.braking_dist) {
				// Keep the one with tighter curvature; the other becomes an
				// extension of it (Phase 3 loop-wrap handles the rest).
				if (std::abs(last.apex_kappa) >= std::abs(first.apex_kappa))
					corners.erase(corners.begin());
				else
					corners.pop_back();
			}
		}
	}

	// ----------------------------------------------------------------
	// Phase 3 — Assign target lateral offsets (outside-inside-outside).
	//   sign_k =  1 → left turn  : inside = –right (offset < 0), outside = +right (offset > 0)
	//   sign_k = –1 → right turn : inside = +right (offset > 0), outside = –right (offset < 0)
	//   inside_off  = –sign_k * hw
	//   outside_off = +sign_k * hw
	// ----------------------------------------------------------------
	std::vector<float> target(n, 0.f);

	for (const Corner& c : corners) {
		const float d_apex  = wps[c.apex_idx].dist_from_start;
		const float sign_k  = (c.apex_kappa >= 0.f) ? 1.f : -1.f;

		for (int i = 0; i < n; ++i) {
			const float hw = wps[i].road_half_width * strength;

			// Signed arc-length distance from this waypoint to the apex,
			// wrapped into [−total/2, +total/2] for loop-safe comparison.
			float d_rel = wps[i].dist_from_start - d_apex;
			if (loop && total_len > 0.f) {
				while (d_rel >  0.5f * total_len) d_rel -= total_len;
				while (d_rel < -0.5f * total_len) d_rel += total_len;
			}

			const float out_off =  sign_k * hw;
			const float in_off  = -sign_k * hw;
			float t_offset = 0.f;

			if (d_rel >= -c.braking_dist && d_rel <= 0.f) {
				// Entry (braking) zone: outside → inside via smoothstep
				const float t = (d_rel + c.braking_dist) / c.braking_dist;
				t_offset = glm::mix(out_off, in_off, glm::smoothstep(0.f, 1.f, t));
			} else if (d_rel > 0.f && d_rel <= c.accel_dist) {
				// Exit (acceleration) zone: inside → outside via smoothstep
				const float t = d_rel / c.accel_dist;
				t_offset = glm::mix(in_off, out_off, glm::smoothstep(0.f, 1.f, t));
			}

			// Dominant-corner rule: the corner with the larger offset wins.
			if (std::abs(t_offset) > std::abs(target[i]))
				target[i] = t_offset;
		}
	}

	// ----------------------------------------------------------------
	// Phase 4 — Wide box-blur to smooth corner entry/exit transitions
	// and handle chicane compromises naturally.
	// ----------------------------------------------------------------
	rl_smooth_scalar(target, loop, 10);

	for (int i = 0; i < n; ++i)
		target[i] = glm::clamp(target[i], -wps[i].road_half_width * strength, wps[i].road_half_width * strength);

	// ----------------------------------------------------------------
	// Phase 5 — Minimum-curvature Gauss-Seidel relaxation.
	//   Seeded from corner targets (not from centerline), so the solver
	//   starts close to the answer and converges in far fewer iterations.
	//   Each step projects point i to the foot of the perpendicular
	//   through its neighbours (zero local curvature), then clamps to
	//   the corridor.  Result: globally minimum curvature path that
	//   respects the outside-inside-outside pattern.
	// ----------------------------------------------------------------
	std::vector<glm::vec3> rl(n);
	for (int i = 0; i < n; ++i)
		rl[i] = wps[i].position + wps[i].right * target[i];

	const int i_start = loop ? 0 : 1;
	const int i_end   = loop ? n : n - 1;

	for (int iter = 0; iter < num_iters; ++iter) {
		for (int i = i_start; i < i_end; ++i) {
			const int prev = loop ? (i + n - 1) % n : i - 1;
			const int next = loop ? (i + 1) % n     : i + 1;

			const glm::vec3 ab    = rl[next] - rl[prev];
			const float     ab_sq = glm::dot(ab, ab);
			glm::vec3 ideal;
			if (ab_sq < 1e-8f) {
				ideal = rl[i];
			} else {
				const float t = glm::dot(rl[i] - rl[prev], ab) / ab_sq;
				ideal = rl[prev] + t * ab;
			}

			const float hw  = wps[i].road_half_width * strength;
			const float lat = glm::clamp(glm::dot(ideal - wps[i].position, wps[i].right), -hw, hw);
			rl[i] = wps[i].position + wps[i].right * lat;
		}
	}

	// ----------------------------------------------------------------
	// Stamp results
	// ----------------------------------------------------------------
	for (int i = 0; i < n; ++i) {
		wps[i].racing_line_pos     = rl[i];
		wps[i].racing_line_lateral = glm::dot(rl[i] - wps[i].position, wps[i].right);
	}
}

// ============================================================
// BikeCourse::build_from_road_network
// ============================================================

void BikeCourse::build_from_road_network(
	const RoadNetworkComponent& network,
	const std::vector<glm::vec3>& route_hints,
	float sample_step_m,
	bool  loop)
{
	const auto& nodes = network.get_nodes();
	const auto& edges = network.get_edges();

	if (nodes.empty() || edges.empty()) {
		sys_print(Warning, "BikeCourse::build_from_road_network: empty network\n");
		return;
	}
	if (route_hints.size() < 2) {
		sys_print(Warning, "BikeCourse::build_from_road_network: need >= 2 route hints\n");
		return;
	}

	// Build edge-id lookup and directed edge direction per node pair
	// edge_map[(a,b)] = edge index (canonical a <= b)
	std::unordered_map<int, int> node_id_to_edge_a;  // not used directly
	// We'll look up edges by the two node ids when sampling
	// Build: for each edge, store as map (a_id, b_id) -> edge*
	struct EdgeKey {
		int a, b;
		bool operator==(const EdgeKey& o) const { return a == o.a && b == o.b; }
	};
	struct EdgeKeyHash {
		size_t operator()(const EdgeKey& k) const {
			return std::hash<int>()(k.a) ^ (std::hash<int>()(k.b) << 16);
		}
	};
	std::unordered_map<EdgeKey, const RoadEdge*, EdgeKeyHash> edge_lookup;
	for (const auto& e : edges) {
		edge_lookup[{e.node_a_id, e.node_b_id}] = &e;
		edge_lookup[{e.node_b_id, e.node_a_id}] = &e;
	}

	// Node position lookup
	std::unordered_map<int, glm::vec3> node_pos;
	for (const auto& n : nodes) node_pos[n.id] = n.position;

	// Snap each route hint to its nearest road node
	std::vector<int> hint_node_ids;
	hint_node_ids.reserve(route_hints.size());
	for (const auto& hint : route_hints)
		hint_node_ids.push_back(nearest_node_id(nodes, hint));

	// Build full ordered node sequence by routing between consecutive hint nodes
	std::vector<int> full_node_path;
	const int num_hints = (int)hint_node_ids.size();
	const int num_legs  = loop ? num_hints : num_hints - 1;

	for (int leg = 0; leg < num_legs; ++leg) {
		const int from_id = hint_node_ids[leg];
		const int to_id   = hint_node_ids[(leg + 1) % num_hints];

		std::vector<int> seg = road_astar(nodes, edges, from_id, to_id);
		if (seg.empty()) {
			sys_print(Warning, "BikeCourse: no road path from hint %d to hint %d\n", leg, leg + 1);
			continue;
		}

		if (full_node_path.empty()) {
			full_node_path = seg;
		} else {
			// Avoid duplicate junction node
			full_node_path.insert(full_node_path.end(), seg.begin() + 1, seg.end());
		}
	}

	if (full_node_path.size() < 2) {
		sys_print(Warning, "BikeCourse::build_from_road_network: could not build a valid path\n");
		return;
	}

	sys_print(Info, "BikeCourse: routed %d road nodes across %d hints\n",
	          (int)full_node_path.size(), num_hints);

	// Dense-sample all edges in the path
	std::vector<glm::vec3> positions;
	std::vector<float>     half_widths;
	positions.reserve(full_node_path.size() * 10);
	half_widths.reserve(positions.capacity());

	const int path_n = (int)full_node_path.size();
	const int seg_count = loop ? path_n : path_n - 1;

	for (int s = 0; s < seg_count; ++s) {
		const int id_a = full_node_path[s];
		const int id_b = full_node_path[(s + 1) % path_n];

		if (!node_pos.count(id_a) || !node_pos.count(id_b)) continue;

		const RoadEdge* ep = edge_lookup.count({id_a, id_b}) ? edge_lookup[{id_a, id_b}] : nullptr;
		const bool skip_first = (s > 0);  // avoid duplicate at junction

		if (ep) {
			const bool going_a_to_b = (ep->node_a_id == id_a);
			sample_edge(*ep, node_pos[id_a], node_pos[id_b],
			            going_a_to_b, sample_step_m, skip_first,
			            positions, half_widths);
		} else {
			// No edge found (shouldn't happen if A* is correct): straight fallback
			const glm::vec3 pa = node_pos[id_a], pb = node_pos[id_b];
			const float len = glm::distance(pa, pb);
			const int steps = glm::max(1, (int)(len / sample_step_m));
			for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
				positions.push_back(glm::mix(pa, pb, (float)i / steps));
				half_widths.push_back(3.f);
			}
		}
	}

	if (positions.size() < 2) {
		sys_print(Warning, "BikeCourse::build_from_road_network: sampling produced < 2 points\n");
		return;
	}

	// Snap every sampled point down to terrain so the course follows the road surface.
	// Mirrors the snap done in rebuild_mesh() inside RoadNetworkComponent.
	{
		const int terrain_mask = (1 << (int)PL::Default) | (1 << (int)PL::StaticObject);
		int snapped = 0;
		for (auto& p : positions) {
			const glm::vec3 from = p + glm::vec3(0.f, SNAP_CAST_UP,   0.f);
			const glm::vec3 to   = p - glm::vec3(0.f, SNAP_CAST_DOWN, 0.f);
			HitResult hit = GameplayStatic::cast_ray(from, to, terrain_mask, nullptr);
			if (hit.hit) { p.y = hit.pos.y; ++snapped; }
		}
		sys_print(Info, "BikeCourse (road network): snapped %d / %d samples to terrain\n",
		          snapped, (int)positions.size());
	}

	build(positions, half_widths, loop);

	// Compute racing line offsets now that waypoints are built
	if (is_built)
		BikeCourse::compute_racing_line(waypoints, is_loop);
}

// ============================================================
// debug_draw
// ============================================================

void BikeCourse::debug_draw() const
{
	if (!is_built || (int)waypoints.size() < 2) return;

	const int n        = (int)waypoints.size();
	const int num_segs = is_loop ? n : n - 1;

	for (int i = 0; i < num_segs; ++i) {
		const BikeWaypoint& wp   = waypoints[i];
		const BikeWaypoint& next = waypoints[(i + 1) % n];

		// Colour by gradient: yellow = uphill, blue = downhill, green = flat
		const float grad_deg = glm::degrees(wp.gradient);
		Color32 line_color;
		if      (grad_deg >  3.f) line_color = Color32(0xff, 0xff, 0x00, 0xff); // yellow
		else if (grad_deg < -2.f) line_color = Color32(0x66, 0xaa, 0xff, 0xff); // blue
		else                      line_color = COLOR_GREEN;

		// Highlight the wrap-around segment in pink so it's obvious
		if (is_loop && i == n - 1)
			line_color = Color32(0xff, 0x66, 0xff, 0xff);

		Debug::add_line(wp.position, next.position, line_color, -1.f);

		// Road-width tick marks every 5th waypoint
		if (i % 5 == 0) {
			const glm::vec3 left_edge  = wp.position - wp.right * wp.road_half_width;
			const glm::vec3 right_edge = wp.position + wp.right * wp.road_half_width;
			Debug::add_line(left_edge, right_edge, Color32(0xff, 0xff, 0xff, 0x66), -1.f);
		}

		// Racing line — use absolute world positions to avoid junction artefacts
		Debug::add_line(wp.racing_line_pos, next.racing_line_pos,
		                Color32(0xff, 0x99, 0x00, 0xff), -1.f);
	}
}
