#include "BikeCourse.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/RoadNetworkComponent.h"
#include "Debug.h"
#include "Physics/ChannelsAndPresets.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <queue>
#include <unordered_map>
#include <unordered_set>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Terrain snap ray: cast this far above and below the placed waypoint position.
static constexpr float SNAP_CAST_UP   = 200.f;
static constexpr float SNAP_CAST_DOWN = 200.f;

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

// ============================================================
// Corner-fillet helpers for build_from_road_network
// ============================================================

struct FilletInfo {
	bool      active       = false;
	glm::vec3 center       = {};
	float     radius       = 0.f;
	glm::vec3 pt_in        = {};    // arc entry: point on incoming road centerline
	glm::vec3 pt_out       = {};    // arc exit:  point on outgoing road centerline
	float     in_cutback   = 0.f;   // meters trimmed from END of incoming edge
	float     out_cutback  = 0.f;   // meters trimmed from START of outgoing edge
	float     arc_angle    = 0.f;   // full deflection angle (radians)
	bool      left_turn    = false;
	float     width_in     = 0.f;
	float     width_out    = 0.f;
};

// Actual spline tangent of a road edge at one of its traversal endpoints.
// at_traversal_end=false → unit tangent leaving the start node (t=0 derivative)
// at_traversal_end=true  → unit tangent arriving at the end node (t=1 derivative)
static glm::vec3 edge_tangent_at(
	const RoadEdge& edge,
	glm::vec3 pos_a, glm::vec3 pos_b,  // pos_a = traversal start, pos_b = traversal end
	bool going_a_to_b,
	bool at_traversal_end)
{
	if (!edge.curved) {
		// Straight edge: tangent is always the traversal direction pos_a→pos_b.
		// going_a_to_b is irrelevant — a straight edge has one direction.
		const glm::vec3 d = pos_b - pos_a;
		const float len = glm::length(d);
		return len > 1e-4f ? d / len : glm::vec3(0, 0, 1);
	}
	// Bezier: pos_a is traversal start, pos_b is traversal end.
	// When going B→A the ctrl points are swapped, but p0/p3 stay at traversal start/end.
	// (Matches the same layout used in sample_edge.)
	glm::vec3 p0, p1, p2, p3;
	if (going_a_to_b) {
		p0 = pos_a; p1 = edge.ctrl_a; p2 = edge.ctrl_b; p3 = pos_b;
	} else {
		p0 = pos_a; p1 = edge.ctrl_b; p2 = edge.ctrl_a; p3 = pos_b;
	}
	// Cubic bezier derivative: at t=0 → direction (p1-p0), at t=1 → direction (p3-p2)
	const glm::vec3 tan = at_traversal_end ? (p3 - p2) : (p1 - p0);
	const float len = glm::length(tan);
	return len > 1e-4f ? tan / len : glm::vec3(0, 0, 1);
}

// Compute fillet arc for a junction between two road centerlines.
// d_in  = unit tangent arriving at junction (pointing TOWARD junction)
// d_out = unit tangent leaving junction   (pointing AWAY from junction)
// in/out edge lengths are used only for the guard check.
static FilletInfo compute_fillet(
	glm::vec3 junction_pos,
	glm::vec3 d_in, glm::vec3 d_out,
	float width_in, float width_out,
	float in_edge_len, float out_edge_len,
	float min_angle_rad)
{
	FilletInfo fi;

	// Full deflection angle between the two road directions
	const float cos_phi = glm::clamp(glm::dot(d_in, d_out), -1.f, 1.f);
	const float phi     = glm::pi<float>() - std::acos(cos_phi);
	if ((glm::pi<float>() - phi) < min_angle_rad) return fi;

	const float theta     = phi * 0.5f;       // half-deflection
	const float sin_theta = std::sin(theta);
	const float tan_theta = std::tan(theta);
	if (sin_theta < 1e-4f || tan_theta < 1e-4f) return fi;

	const float R       = glm::max(width_in, width_out);
	const float cutback = R / tan_theta;      // arc-length trimmed from each edge

	// Disable if fillet would consume more than 80% of either edge
	if (cutback > 0.8f * in_edge_len || cutback > 0.8f * out_edge_len) return fi;

	// -d_in + d_out bisects the angle looking "backward" along incoming and
	// "forward" along outgoing — this points toward the inside of the corner.
	// (d_in + d_out would point to the outside.)
	const glm::vec3 hv_raw = d_out - d_in;
	const float hv_len = glm::length(hv_raw);
	if (hv_len < 1e-4f) return fi;   // ~180-degree U-turn — skip

	fi.active      = true;
	fi.center      = junction_pos + (hv_raw / hv_len) * (R / sin_theta);
	fi.radius      = R;
	fi.pt_in       = junction_pos - d_in  * cutback;
	fi.pt_out      = junction_pos + d_out * cutback;
	fi.in_cutback  = cutback;
	fi.out_cutback = cutback;
	fi.arc_angle   = phi;
	// cross(d_in, d_out).y < 0 → left turn (CCW from above)
	fi.left_turn   = (glm::cross(d_in, d_out).y < 0.f);
	fi.width_in    = width_in;
	fi.width_out   = width_out;
	return fi;
}

// Append dense arc samples sweeping from fi.pt_in to fi.pt_out.
// i=0 (= pt_in) is skipped — caller guarantees pt_in is already the last entry.
// include_last: if false, also skip i=steps (= pt_out); used for the loop-seam arc
// to avoid duplicating the very first position in the array.
static void sample_arc_into(
	const FilletInfo& fi,
	float step_m,
	std::vector<glm::vec3>& out_positions,
	std::vector<float>&     out_widths,
	bool include_last = true)
{
	const float arc_len = fi.radius * fi.arc_angle;
	const int   steps   = glm::max(2, (int)(arc_len / step_m));

	const glm::vec3 from_c_in  = fi.pt_in  - fi.center;
	const glm::vec3 from_c_out = fi.pt_out - fi.center;
	const float angle_in  = std::atan2(from_c_in.z,  from_c_in.x);
	const float angle_out = std::atan2(from_c_out.z, from_c_out.x);

	float delta = angle_out - angle_in;
	if (fi.left_turn) {
		if (delta < 0.f) delta += 2.f * glm::pi<float>();
	} else {
		if (delta > 0.f) delta -= 2.f * glm::pi<float>();
	}

	const int end_i = include_last ? steps : steps - 1;
	for (int i = 1; i <= end_i; ++i) {
		const float t     = (float)i / steps;
		const float alpha = angle_in + t * delta;
		const float y     = glm::mix(fi.pt_in.y, fi.pt_out.y, t);
		out_positions.push_back({ fi.center.x + fi.radius * std::cos(alpha),
		                          y,
		                          fi.center.z + fi.radius * std::sin(alpha) });
		out_widths.push_back(glm::mix(fi.width_in, fi.width_out, t));
	}
}

// ============================================================
// Densely sample a single road edge into out_positions/out_widths.
// trim_start_m / trim_end_m: arc-length to skip from the start / end of the
// edge (used to carve out the region replaced by a fillet arc).
// skip_first: when true, the sample at the start of the effective range is
// omitted because it is already the last entry written by the previous arc.
// ============================================================
static void sample_edge(
	const RoadEdge& edge,
	glm::vec3 pos_a, glm::vec3 pos_b,
	bool going_a_to_b,
	float step_m,
	bool skip_first,
	float trim_start_m,
	float trim_end_m,
	std::vector<glm::vec3>& out_positions,
	std::vector<float>&     out_widths)
{
	const float half_w = edge.width * 0.5f;

	if (!edge.curved) {
		const float seg_len = glm::distance(pos_a, pos_b);
		if (seg_len < 1e-4f) return;
		const float eff_start = trim_start_m;
		const float eff_end   = seg_len - trim_end_m;
		if (eff_end <= eff_start + 1e-4f) return;
		const float eff_len = eff_end - eff_start;
		const int steps = glm::max(1, (int)(eff_len / step_m));
		for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
			const float arc = eff_start + (float)i / steps * eff_len;
			out_positions.push_back(glm::mix(pos_a, pos_b, arc / seg_len));
			out_widths.push_back(half_w);
		}
	} else {
		// Cubic Bezier — build arc-length table, then sample the trimmed range.
		glm::vec3 p0, p1, p2, p3;
		if (going_a_to_b) {
			p0 = pos_a; p1 = edge.ctrl_a; p2 = edge.ctrl_b; p3 = pos_b;
		} else {
			p0 = pos_a; p1 = edge.ctrl_b; p2 = edge.ctrl_a; p3 = pos_b;
		}

		static constexpr int PRESAMPLE = 100;
		std::vector<float>     arc(PRESAMPLE + 1, 0.f);
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

		const float eff_start = trim_start_m;
		const float eff_end   = total_len - trim_end_m;
		if (eff_end <= eff_start + 1e-4f) return;
		const float eff_len = eff_end - eff_start;
		const int steps = glm::max(1, (int)(eff_len / step_m));

		auto arc_lookup = [&](float want) -> glm::vec3 {
			int lo = 0, hi = PRESAMPLE;
			while (lo < hi) {
				const int mid = (lo + hi + 1) / 2;
				(arc[mid] <= want) ? lo = mid : hi = mid - 1;
			}
			if (lo >= PRESAMPLE) return pts[PRESAMPLE];
			const float seg  = arc[lo + 1] - arc[lo];
			const float frac = (seg > 1e-6f) ? (want - arc[lo]) / seg : 0.f;
			return glm::mix(pts[lo], pts[lo + 1], frac);
		};

		for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
			out_positions.push_back(arc_lookup(eff_start + (float)i / steps * eff_len));
			out_widths.push_back(half_w);
		}
	}
}

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

	const int path_n   = (int)full_node_path.size();
	const int seg_count = loop ? path_n : path_n - 1;

	// ---- Pre-compute fillet arcs at each junction node ----
	// fillets[s] describes the fillet AT node full_node_path[s], between
	// the incoming segment (s-1 → s) and the outgoing segment (s → s+1).
	const float fillet_min_rad = fillet_enabled
		? (fillet_min_angle_deg * glm::pi<float>() / 180.f)
		: glm::pi<float>() + 1.f;   // > pi — no deflection can exceed this, disables all fillets

	debug_fillets.clear();
	std::vector<FilletInfo> fillets(path_n);
	{
		const int fi_first = loop ? 0 : 1;
		const int fi_last  = loop ? path_n : path_n - 1;

		for (int s = fi_first; s < fi_last; ++s) {
			const int prev = (s - 1 + path_n) % path_n;
			const int next = (s + 1) % path_n;

			const int id_prev = full_node_path[prev];
			const int id_cur  = full_node_path[s];
			const int id_next = full_node_path[next];

			if (!node_pos.count(id_prev) || !node_pos.count(id_cur) || !node_pos.count(id_next)) continue;

			const glm::vec3& p_prev = node_pos[id_prev];
			const glm::vec3& p_cur  = node_pos[id_cur];
			const glm::vec3& p_next = node_pos[id_next];

			// Incoming edge (prev → cur)
			const RoadEdge* ep_in = edge_lookup.count({id_prev, id_cur}) ? edge_lookup[{id_prev, id_cur}] : nullptr;
			glm::vec3 d_in;
			float     hw_in, len_in;
			if (ep_in) {
				const bool gab = (ep_in->node_a_id == id_prev);
				d_in   = edge_tangent_at(*ep_in, p_prev, p_cur, gab, /*at_end=*/true);
				hw_in  = ep_in->width * 0.5f;
				len_in = glm::distance(p_prev, p_cur);
			} else {
				const glm::vec3 dv = p_cur - p_prev;
				len_in = glm::length(dv);
				d_in   = len_in > 1e-4f ? dv / len_in : glm::vec3(0, 0, 1);
				hw_in  = 3.f;
			}

			// Outgoing edge (cur → next)
			const RoadEdge* ep_out = edge_lookup.count({id_cur, id_next}) ? edge_lookup[{id_cur, id_next}] : nullptr;
			glm::vec3 d_out;
			float     hw_out, len_out;
			if (ep_out) {
				const bool gab = (ep_out->node_a_id == id_cur);
				d_out   = edge_tangent_at(*ep_out, p_cur, p_next, gab, /*at_end=*/false);
				hw_out  = ep_out->width * 0.5f;
				len_out = glm::distance(p_cur, p_next);
			} else {
				const glm::vec3 dv = p_next - p_cur;
				len_out = glm::length(dv);
				d_out   = len_out > 1e-4f ? dv / len_out : glm::vec3(0, 0, 1);
				hw_out  = 3.f;
			}

			fillets[s] = compute_fillet(p_cur, d_in, d_out, hw_in, hw_out, len_in, len_out,
			                            fillet_min_rad);
		}

		int n_fillets = 0;
		for (const auto& f : fillets) {
			if (!f.active) continue;
			++n_fillets;
			debug_fillets.push_back({ f.center, f.pt_in, f.pt_out, f.radius, f.arc_angle, f.left_turn });
		}
		sys_print(Info, "BikeCourse: applied %d corner fillets across %d junctions\n",
		          n_fillets, fi_last - fi_first);
	}

	// ---- Dense-sample all edges, carving fillet zones and inserting arcs ----
	std::vector<glm::vec3> positions;
	std::vector<float>     half_widths;
	positions.reserve(full_node_path.size() * 12);
	half_widths.reserve(positions.capacity());

	// Tracks the positions[] index range for each fillet arc so we can force
	// exact radial right-vectors on those waypoints after build().
	struct FilletRange { int start_idx, end_idx; glm::vec3 center; bool left_turn; };
	std::vector<FilletRange> fillet_ranges;

	for (int s = 0; s < seg_count; ++s) {
		const int id_a = full_node_path[s];
		const int id_b = full_node_path[(s + 1) % path_n];

		if (!node_pos.count(id_a) || !node_pos.count(id_b)) continue;

		const FilletInfo& fi_start = fillets[s];
		const FilletInfo& fi_end   = fillets[(s + 1) % path_n];

		const float trim_start = fi_start.active ? fi_start.out_cutback : 0.f;
		const float trim_end   = fi_end.active   ? fi_end.in_cutback    : 0.f;

		// skip_first: at s==0 (start of path/loop) always include the first sample;
		// for s>0 the junction/pt_out was already written by the previous edge or arc.
		const bool skip_first = (s > 0);

		const RoadEdge* ep = edge_lookup.count({id_a, id_b}) ? edge_lookup[{id_a, id_b}] : nullptr;
		if (ep) {
			const bool going_a_to_b = (ep->node_a_id == id_a);
			sample_edge(*ep, node_pos[id_a], node_pos[id_b],
			            going_a_to_b, sample_step_m, skip_first,
			            trim_start, trim_end,
			            positions, half_widths);
		} else {
			// Straight fallback (no matching edge — shouldn't happen for valid A* paths)
			const glm::vec3 pa = node_pos[id_a], pb = node_pos[id_b];
			const float seg_len = glm::distance(pa, pb);
			const float eff_s = trim_start, eff_e = seg_len - trim_end;
			if (eff_e > eff_s + 1e-4f) {
				const float eff_len = eff_e - eff_s;
				const int steps = glm::max(1, (int)(eff_len / sample_step_m));
				for (int i = (skip_first ? 1 : 0); i <= steps; ++i) {
					const float arc = eff_s + (float)i / steps * eff_len;
					positions.push_back(glm::mix(pa, pb, arc / seg_len));
					half_widths.push_back(3.f);
				}
			}
		}

		// After sampling this edge, insert the fillet arc at the end node (if any).
		// For a loop, the arc at node 0 is inserted last; we omit its final sample
		// (pt_out[0]) to avoid duplicating the very first position in the array.
		if (fi_end.active) {
			const bool is_loop_seam = (loop && (s + 1) % path_n == 0);
			// Record pt_in index (last edge sample) before pushing arc samples.
			const int pt_in_idx = (int)positions.size() - 1;
			sample_arc_into(fi_end, sample_step_m, positions, half_widths,
			                /*include_last=*/!is_loop_seam);
			// pt_out is positions[0] for the loop-seam arc (added by edge s=0).
			const int pt_out_idx = is_loop_seam ? 0 : (int)positions.size() - 1;
			if (pt_in_idx >= 0)
				fillet_ranges.push_back({ pt_in_idx, pt_out_idx, fi_end.center, fi_end.left_turn });
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

	// Populate waypoints from sampled positions.
	waypoints.clear();
	total_length_m = 0.f;
	is_built       = false;
	is_loop        = loop;

	const int n = (int)positions.size();
	waypoints.resize(n);
	for (int i = 0; i < n; ++i) {
		BikeWaypoint& wp   = waypoints[i];
		wp.position        = positions[i];
		wp.road_half_width = (i < (int)half_widths.size()) ? half_widths[i] : 4.f;

		glm::vec3 fwd;
		if      (i < n - 1) fwd = positions[i + 1] - positions[i];
		else if (loop)      fwd = positions[0]     - positions[i];
		else                fwd = positions[i]     - positions[i - 1];

		const float fwd_len = glm::length(fwd);
		fwd = (fwd_len > 1e-4f) ? fwd / fwd_len
		    : (i > 0 ? waypoints[i - 1].forward : glm::vec3(0, 0, 1));

		wp.forward  = fwd;
		wp.gradient = glm::asin(glm::clamp(fwd.y, -1.f, 1.f));

		const glm::vec3 right = glm::cross(WORLD_UP, fwd);
		const float     rlen  = glm::length(right);
		wp.right = (rlen > 1e-4f) ? (right / rlen) : glm::vec3(1, 0, 0);

		wp.dist_from_start = (i == 0) ? 0.f
		    : waypoints[i - 1].dist_from_start + glm::distance(positions[i], positions[i - 1]);
	}
	total_length_m = loop
	    ? waypoints.back().dist_from_start + glm::distance(positions[n - 1], positions[0])
	    : waypoints.back().dist_from_start;
	is_built = true;
	sys_print(Info, "BikeCourse: %d waypoints, %.0f m total%s\n",
	          n, total_length_m, loop ? " (loop)" : "");

	// Force exact radial right-vectors on fillet arc waypoints (including pt_in / pt_out).
	// The chord-direction approximation above is corrected here for arc sections: the exact
	// perpendicular is the radial direction from the fillet center.
	//   left_turn  → center is to the LEFT  → right points AWAY  from center (+radial)
	//   right_turn → center is to the RIGHT → right points TOWARD center     (−radial)
	if (is_built) {
		for (const auto& fr : fillet_ranges) {
			auto fix_right = [&](int idx) {
				if (idx < 0 || idx >= (int)waypoints.size()) return;
				BikeWaypoint& wp = waypoints[idx];
				glm::vec3 radial = wp.position - fr.center;
				radial.y = 0.f;
				const float rlen = glm::length(radial);
				if (rlen < 1e-4f) return;
				wp.right = (fr.left_turn ? 1.f : -1.f) * (radial / rlen);
			};
			if (fr.start_idx <= fr.end_idx) {
				for (int i = fr.start_idx; i <= fr.end_idx; ++i)
					fix_right(i);
			} else {
				// Loop-seam wrap: [start_idx .. size-1] then [0 .. end_idx]
				for (int i = fr.start_idx; i < (int)waypoints.size(); ++i)
					fix_right(i);
				for (int i = 0; i <= fr.end_idx; ++i)
					fix_right(i);
			}
		}
	}

	// Stash arc waypoint ranges for audit dumps.
	arc_ranges.clear();
	arc_ranges.reserve(fillet_ranges.size());
	for (const auto& fr : fillet_ranges)
		arc_ranges.emplace_back(fr.start_idx, fr.end_idx);

	// Compute racing line offsets now that waypoints are built
	if (is_built)
		BikeCourse::compute_racing_line(waypoints, is_loop, rl_k, rl_mass, rl_dt, rl_num_iters, rl_smooth_passes, rl_smooth_w);
}
