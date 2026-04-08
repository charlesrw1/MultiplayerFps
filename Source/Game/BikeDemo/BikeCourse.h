#pragma once
#include <vector>
#include <glm/glm.hpp>

// One node along the race course spline.
// All derived fields (forward, right, dist_from_start, gradient) are precomputed at build time.
struct BikeWaypoint {
	glm::vec3 position        = {};
	glm::vec3 forward         = { 0, 0, 1 };  // normalized tangent along the course
	glm::vec3 right           = { 1, 0, 0 };  // road-right, perpendicular to forward in road plane
	float     road_half_width = 4.f;           // half the usable road width (m)
	float     dist_from_start = 0.f;           // arc-length from start to this node (m)
	float     gradient        = 0.f;           // road gradient in radians (+ve = uphill)
};

// The course spline built from level waypoint spawners.
// All per-frame AI queries go through this class.
class BikeCourse {
public:
	std::vector<BikeWaypoint> waypoints;
	float total_length_m = 0.f;
	bool  is_built       = false;

	// Load "bike_waypoint" spawners from the current level, sort by editor_name (integer), build.
	void build_from_spawners();

	// Build from raw positions and per-node half-widths.
	void build(const std::vector<glm::vec3>& positions, const std::vector<float>& road_half_widths);

	// Project a world-space position onto the nearest point on the course.
	// Returns arc-length (course_dist_m) of that point.
	// out_lateral: signed offset from road centre, +ve = road-right
	// out_segment: index of the waypoint segment that was nearest
	// hint_segment: search start hint (use cached segment index for repeated calls)
	float project(glm::vec3 world_pos,
	              float* out_lateral  = nullptr,
	              int*   out_segment  = nullptr,
	              int    hint_segment = 0) const;

	// Interpolated waypoint at a given arc-length (Catmull-Rom for position, lerp for the rest).
	BikeWaypoint sample(float dist_m) const;

	// World-space position ahead_m metres ahead of from_dist_m along the course.
	// Used by AI for the PID steering lookahead point.
	glm::vec3 lookahead(float from_dist_m, float ahead_m) const;

	// Draw the spline in the debug overlay (gradient-coloured, with road-width tick marks).
	void debug_draw() const;
};
