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

	// Road network build parameters — change and call build_from_road_network to re-apply.
	float sample_step_m        = 4.0f;  // dense-sample spacing along road edges (m)

	// Fillet parameters — change and call build_from_road_network to re-apply.
	bool  fillet_enabled       = true;
	float fillet_min_angle_deg = 10.f;  // junctions with less deflection are not filleted

	// Racing line physics parameters — change and call rebuild_racing_line() to re-apply
	// without a full course rebuild, or they are picked up automatically on build_from_road_network.
	float rl_k         = 0.50f;    // hinge spring stiffness
	float rl_mass      = 2.0f;    // waypoint mass
	float rl_dt        = 1.f/60.f; // time step per iteration
	int   rl_num_iters = 5000;     // simulation steps — more = better convergence

	// Re-run the racing line simulation on the current waypoints using the stored rl_* params.
	void rebuild_racing_line();

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

	// Project a world-space position onto the nearest point on the course.
	// Returns arc-length (course_dist_m) of that point.
	// out_lateral:  signed offset from road centre, +ve = road-right
	// out_segment:  index of the waypoint segment that was nearest
	// prev_dist_m:  arc-length from the previous frame (pass course_dist_m).
	//   When >= 0, restricts the search to [prev-10m, prev+50m] in arc-length,
	//   preventing a distant part of the loop from stealing the projection when
	//   it happens to be nearby in world-space (loop-aliasing bug).
	//   Falls back to global search only if the rider is >30m from every
	//   segment in the window (off-track / crash recovery).
	float project(glm::vec3 world_pos,
	              float*     out_lateral = nullptr,
	              int*       out_segment = nullptr,
	              float      prev_dist_m = -1.f) const;

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

	// Hinge-spring physics simulation that slides each waypoint laterally to find
	// the ideal racing line (outside-entry / late-apex / outside-exit).
	// k:         spring stiffness of each hinge
	// mass:      mass of each waypoint
	// dt:        time step per iteration (lerp factor = dt/100)
	// num_iters: simulation steps — more = better convergence
	static void compute_racing_line(std::vector<BikeWaypoint>& wps, bool loop,
	                                float k         = 2.0f,
	                                float mass      = 0.50f,
	                                float dt        = 1.0f/60.f,
	                                int   num_iters = 5000);

};
