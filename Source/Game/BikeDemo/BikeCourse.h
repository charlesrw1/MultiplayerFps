#pragma once
#include <vector>
#include <glm/glm.hpp>
class RoadNetworkComponent;

// One node along the race course spline.
// All derived fields (forward, right, dist_from_start, gradient) are precomputed at build time.
struct BikeWaypoint {
	glm::vec3 position           = {};
	glm::vec3 forward            = { 0, 0, 1 };  // normalized tangent along the course
	glm::vec3 right              = { 1, 0, 0 };  // road-right, perpendicular to forward in road plane
	float     road_half_width    = 4.f;           // half the usable road width (m)
	float     dist_from_start    = 0.f;           // arc-length from start to this node (m)
	float     gradient           = 0.f;           // road gradient in radians (+ve = uphill)
	float     racing_line_lateral = 0.f;          // ideal racing-line offset from centre (+ve = road-right) — kept for AI error computation
	glm::vec3 racing_line_pos     = {};            // absolute world-space position on the racing line (use this for all rendering)
	float     speed_mps           = 0.f;           // target speed on the racing line (m/s) — from gradient-aware kinematic profile
};

// Fillet arc stored for debug visualisation after build_from_road_network.
struct FilletDebugInfo {
	glm::vec3 center, pt_in, pt_out;
	float     radius, arc_angle;
	bool      left_turn;
};

// The course spline built from level waypoint spawners.
// All per-frame AI queries go through this class.
class BikeCourse {
public:
	std::vector<BikeWaypoint>   waypoints;
	std::vector<FilletDebugInfo> debug_fillets;  // populated by build_from_road_network
	float total_length_m      = 0.f;
	bool  is_built            = false;
	bool  is_loop             = false;  // true: last waypoint connects back to first

	// Fillet parameters — change and call build_from_road_network to re-apply.
	bool  fillet_enabled       = true;
	float fillet_min_angle_deg = 10.f;  // junctions with less deflection are not filleted

	// Load "bike_waypoint" spawners from the current level, sort by editor_name (integer).
	// Raycasts each waypoint down to the terrain surface, then builds as a loop.
	void build_from_spawners();

	// Route through the road network graph using route_hints as ordered control points.
	// For each consecutive pair of hints, finds the nearest road nodes and runs A* between
	// them. The resulting edge chain is densely sampled to produce smooth course waypoints.
	// If loop is true, the last hint routes back to the first.
	void build_from_road_network(const RoadNetworkComponent& network,
	                              const std::vector<glm::vec3>& route_hints,
	                              float sample_step_m = 0.5f,
	                              bool  loop = false);

	// Build from raw positions and per-node half-widths.
	// If loop is true, connects the last waypoint back to the first.
	void build(const std::vector<glm::vec3>& positions,
	           const std::vector<float>&     road_half_widths,
	           bool                          loop = false);

	// Project a world-space position onto the nearest point on the course.
	// Returns arc-length (course_dist_m) of that point.
	// out_lateral: signed offset from road centre, +ve = road-right
	// out_segment: index of the waypoint segment that was nearest
	float project(glm::vec3 world_pos,
	              float* out_lateral = nullptr,
	              int*   out_segment = nullptr) const;

	// Interpolated waypoint at a given arc-length (Catmull-Rom for position, lerp for the rest).
	// Wraps when is_loop is true.
	BikeWaypoint sample(float dist_m) const;

	// World-space position ahead_m metres ahead of from_dist_m along the course.
	// Wraps around the loop automatically.
	glm::vec3 lookahead(float from_dist_m, float ahead_m) const;

	// World-space position on the ideal racing line, ahead_m metres ahead of from_dist_m.
	// Offsets the spline centre by the precomputed racing_line_lateral at that point.
	glm::vec3 racing_line_lookahead(float from_dist_m, float ahead_m) const;

	// Samples the path over [from_dist_m, from_dist_m + ahead_m] and returns
	// the minimum turn radius found (metres). Very large value = straight section.
	// Used by AI to compute safe cornering speed.
	float min_turn_radius_ahead(float from_dist_m, float ahead_m) const;

	// Draw the spline in the debug overlay (gradient-coloured, with road-width tick marks).
	void debug_draw() const;

	// Draw fillet arc geometry (centers, tangent points, radius lines).
	// Call from the debug menu after build_from_road_network.
	void debug_draw_fillets() const;

	// Iterative minimum-curvature path optimisation.
	// Pulls the racing line tight inside the road corridor until curvature is minimised.
	// Produces outside-entry / late-apex / outside-exit naturally — no corner detection.
	// strength: fraction of road_half_width available to the racing line (0=centred, 1=full width).
	// num_iters: optimisation iterations — more = tighter/smoother, diminishing returns above ~300.
	static void compute_racing_line(std::vector<BikeWaypoint>& wps, bool loop,
	                                float strength  = 0.82f,
	                                int   num_iters = 200);
};
