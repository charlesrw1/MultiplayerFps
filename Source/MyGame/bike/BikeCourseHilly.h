// BikeCourseHilly.h
// "Hilly" hardcoded course: a curvy road laid over procedural, perlin-noise
// terrain (bumpy, not mountainous), with road gradient clamped to
// max_grade_pct. See BikeCourseHilly.cpp for the generator and
// BikeCourseTerrainMesh.cpp for the visual terrain mesh built from the same
// height function.
#pragma once
#include "BikeCourse.h"

// Debug-tunable terrain/road parameters for the Hilly course. Mirrors the
// BikeAIParams pattern: a plain struct + single global instance, edited
// directly by ImGui drag widgets (BikeApplication_Debug.cpp), no reflection.
struct BikeHillyParams {
	// ---- Perlin fBm heightfield (bike_hilly_height) ----
	unsigned seed        = 1337;
	int   octaves        = 4;      // more octaves = more small-scale bumpiness on top of the base rolls
	float base_freq      = 0.02f;  // 1/metre -- lower = broader rolling hills
	float lacunarity     = 2.0f;   // frequency multiplier per octave
	float gain           = 0.5f;   // amplitude multiplier per octave
	float amplitude_m    = 16.f;   // metres -- raw (pre grade-limiting) peak-to-peak is roughly +-amplitude_m

	// ---- Terrain mesh (build_terrain_mesh) ----
	float terrain_size_m       = 150.f;  // full width/length of the terrain square, centered on the course
	float terrain_grid_step_m  = 3.f;    // metres between grid vertices -- lower = smoother/heavier mesh

	// ---- Road elevation grading (build_hilly_circuit) ----
	// Raw terrain height sampled per-waypoint is grade-limited by iterative
	// forward/backward clamping (see BikeCourseHilly.cpp) so the ROAD itself
	// never exceeds this even where the underlying terrain is steeper --
	// "bumpy hills you can actually ride over", not a slope-following slalom.
	float max_grade_pct   = 8.f;    // percent, e.g. 8 = 8% grade (~4.6 deg)
	int   grade_smooth_passes = 12; // forward+backward clamp passes -- more = closer to true max_grade_pct at the seam

	// ---- Terrain/road conforming (bike_hilly_terrain_height) ----
	// The road is grade-limited to max_grade_pct but the raw noise terrain
	// often isn't -- without this, the road ribbon floats above dips or is
	// half-buried in bumps wherever the natural terrain slope exceeds the
	// road's own. This is the lateral distance (beyond the road's own half
	// width) over which the terrain is smoothly pulled down/up to meet the
	// road surface exactly at its edge, like a real cut/embankment.
	float road_blend_dist_m = 6.f;
};
extern BikeHillyParams g_hilly_params;

// Raw terrain height (perlin fBm only, no road conforming) at a world XZ
// position, per the current g_hilly_params. Used by build_hilly_circuit to
// derive the road's own (grade-limited) elevation profile -- NOT what the
// terrain mesh or the bike's physics should sample directly; see
// bike_hilly_terrain_height for that.
float bike_hilly_height(float x, float z);

// Terrain height a renderer/physics query should actually use: bike_hilly_height,
// but pulled toward the nearest point on the Hilly road's own (grade-limited)
// elevation within road_blend_dist_m of the road edge, so the terrain always
// meets the road with no floating/buried gap. Falls back to bike_hilly_height
// unchanged if the Hilly course isn't currently built (g_bike_app null, wrong
// course_variant, or course not yet built). Single source of truth: used by
// both BikeCourseTerrainMesh.cpp's mesh sampling and BikeObject's tick_transform
// terrain query.
float bike_hilly_terrain_height(float x, float z);

// Builds the Hilly course: a curvy closed-loop road (BikeTurtle path, broad
// sweeping bends) with elevation sampled from bike_hilly_height and grade-
// limited to g_hilly_params.max_grade_pct, then runs the standard racing-line
// simulation. Fills course.waypoints/total_length_m/is_loop/is_built, same
// contract as build_hardcoded_circuit's other variants.
void build_hilly_circuit(BikeCourse& course);
