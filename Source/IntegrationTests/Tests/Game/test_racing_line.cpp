// Unit tests for BikeCourse::compute_racing_line (hinge-spring physics)
// Tests operate on synthetic waypoint arrays — no level loading required.
#include "IntegrationTests/TestContext.h"
#include "IntegrationTests/TestRegistry.h"
#include "Game/BikeDemo/BikeCourse.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Build a BikeWaypoint array from a list of positions + uniform half-width.
// Computes forward, right, dist_from_start exactly as BikeCourse::build() does.
static std::vector<BikeWaypoint> make_waypoints(const std::vector<glm::vec3>& positions,
                                                 float hw, bool loop)
{
	const int n = (int)positions.size();
	std::vector<BikeWaypoint> wps(n);
	for (int i = 0; i < n; ++i) {
		BikeWaypoint& wp = wps[i];
		wp.position       = positions[i];
		wp.road_half_width = hw;

		glm::vec3 fwd;
		if (i < n - 1)       fwd = positions[i + 1] - positions[i];
		else if (loop)       fwd = positions[0]     - positions[i];
		else                 fwd = positions[i]     - positions[i - 1];

		const float len = glm::length(fwd);
		fwd = (len > 1e-4f) ? fwd / len : glm::vec3(0, 0, 1);

		wp.forward = fwd;
		glm::vec3 right = glm::cross(WORLD_UP, fwd);
		const float rlen = glm::length(right);
		wp.right = (rlen > 1e-4f) ? right / rlen : glm::vec3(1, 0, 0);

		wp.dist_from_start = (i == 0) ? 0.f
			: wps[i - 1].dist_from_start + glm::distance(positions[i], positions[i - 1]);
	}
	return wps;
}

// Return the index whose racing_line_lateral has the largest absolute value.
static int peak_lateral_idx(const std::vector<BikeWaypoint>& wps)
{
	int best = 0;
	float best_abs = 0.f;
	for (int i = 0; i < (int)wps.size(); ++i) {
		if (std::abs(wps[i].racing_line_lateral) > best_abs) {
			best_abs = std::abs(wps[i].racing_line_lateral);
			best = i;
		}
	}
	return best;
}

// ─────────────────────────────────────────────────────────────
// Test: straight road has no lateral offset
// ─────────────────────────────────────────────────────────────
static TestTask test_straight_road_zero_lateral(TestContext& t) {
	// 60 waypoints going east, 0.5m spacing
	std::vector<glm::vec3> positions;
	for (int i = 0; i < 60; ++i)
		positions.push_back({ i * 0.5f, 0.f, 0.f });

	auto wps = make_waypoints(positions, 4.f, false);
	BikeCourse::compute_racing_line(wps, false);

	for (int i = 0; i < (int)wps.size(); ++i)
		t.check(std::abs(wps[i].racing_line_lateral) < 0.1f,
		        "straight road: lateral near zero at every waypoint");

	co_return;
}
GAME_TEST("racing_line/straight_zero_lateral", 5.f, test_straight_road_zero_lateral);

// ─────────────────────────────────────────────────────────────
// Test: 90° right turn (east → south)
//
//   Road A: 20 pts east, step 0.5 m  (indices 0–19)
//   Junction: index 20
//   Road B: 20 pts south, step 0.5 m (indices 21–40)
//
// For a 90° right turn with hw=4:
//   - Approach (road A): lateral < 0   (outside = north)
//   - Apex region (road B, ~hw=4 m past junction): lateral >> 0  (inside = west)
//   - Exit (road B far): lateral < 0   (outside = east)
// ─────────────────────────────────────────────────────────────
static TestTask test_90deg_right_turn_apex(TestContext& t) {
	const float hw    = 4.f;
	const float step  = 0.5f;

	// Road A: east (+X), 10 m = 20 pts
	std::vector<glm::vec3> positions;
	for (int i = 0; i <= 20; ++i)
		positions.push_back({ i * step, 0.f, 0.f });
	// Road B: south (-Z) from junction, 10 m = 20 pts
	for (int i = 1; i <= 20; ++i)
		positions.push_back({ 20 * step, 0.f, -i * step });

	auto wps = make_waypoints(positions, hw, false);
	BikeCourse::compute_racing_line(wps, false);

	// 1. Apex is the maximum-lateral waypoint — should be on road B, inside the turn.
	//    Expected: ~hw*tan(45°)=4 m past junction → index ~28.
	const int apex_idx = peak_lateral_idx(wps);
	t.check(apex_idx > 20, "90° right apex is on road B (past junction)");
	t.check(apex_idx < 40, "90° right apex is not at the very end of road B");

	// 2. Apex lateral is strongly positive (inside = road-right = west for south-going road).
	const float apex_lat = wps[apex_idx].racing_line_lateral;
	t.check(apex_lat > hw * 0.5f, "90° right apex lateral is strongly inside (> 50% of hw)");

	// 3. Approach on road A is negative (outside = north side of east-going road).
	float worst_approach = 0.f;
	for (int i = 0; i <= 15; ++i)  // well before corner start
		worst_approach = std::min(worst_approach, wps[i].racing_line_lateral);
	t.check(worst_approach < -0.1f, "90° right: approach on road A is to the outside");

	// 4. Exit on far road B is negative (outside = east side of south-going road).
	float worst_exit = 0.f;
	for (int i = 35; i < (int)wps.size(); ++i)
		worst_exit = std::min(worst_exit, wps[i].racing_line_lateral);
	t.check(worst_exit < -0.1f, "90° right: exit on road B is to the outside");

	co_return;
}
GAME_TEST("racing_line/90deg_right_turn_apex", 5.f, test_90deg_right_turn_apex);

// ─────────────────────────────────────────────────────────────
// Test: 90° left turn (east → north)
//
// Mirror of right-turn test — apex lateral should be negative (inside = east for
// north-going road, which is road-right; but for a LEFT turn, sg=-1, so apex < 0).
// ─────────────────────────────────────────────────────────────
static TestTask test_90deg_left_turn_apex(TestContext& t) {
	const float hw   = 4.f;
	const float step = 0.5f;

	// Road A: east (+X)
	std::vector<glm::vec3> positions;
	for (int i = 0; i <= 20; ++i)
		positions.push_back({ i * step, 0.f, 0.f });
	// Road B: north (+Z)
	for (int i = 1; i <= 20; ++i)
		positions.push_back({ 20 * step, 0.f, i * step });

	auto wps = make_waypoints(positions, hw, false);
	BikeCourse::compute_racing_line(wps, false);

	const int apex_idx = peak_lateral_idx(wps);
	t.check(apex_idx > 20, "90° left apex is on road B (past junction)");

	const float apex_lat = wps[apex_idx].racing_line_lateral;
	t.check(apex_lat < -hw * 0.5f, "90° left apex lateral is strongly inside (strongly negative)");

	// Approach (road A) should be positive (outside = south side for east-going road, left turn)
	float worst_approach = 0.f;
	for (int i = 0; i <= 15; ++i)
		worst_approach = std::max(worst_approach, wps[i].racing_line_lateral);
	t.check(worst_approach > 0.1f, "90° left: approach on road A is to the outside");

	// Exit (far road B) should be positive (outside for left turn on north-going road)
	float worst_exit = 0.f;
	for (int i = 35; i < (int)wps.size(); ++i)
		worst_exit = std::max(worst_exit, wps[i].racing_line_lateral);
	t.check(worst_exit > 0.1f, "90° left: exit on road B is to the outside");

	co_return;
}
GAME_TEST("racing_line/90deg_left_turn_apex", 5.f, test_90deg_left_turn_apex);

// ─────────────────────────────────────────────────────────────
// Test: sharp 150° right turn (nearly a hairpin)
//
// The true apex is hw*tan(75°) ≈ 14.9 m past the junction on road B.
// Road B needs to be long enough to contain the apex.
// Apex should still be on road B and strongly inside.
// ─────────────────────────────────────────────────────────────
static TestTask test_150deg_sharp_right_turn(TestContext& t) {
	const float hw   = 4.f;
	const float step = 0.5f;

	// 150° right turn: road A goes east (+X), road B goes at 150° clockwise from east.
	// 150° clockwise from east in XZ plane: direction = (cos(-150°), 0, sin(-150°))
	//   = (-0.866, 0, -0.5)  → going SW-ish
	const glm::vec3 dir_b = glm::normalize(glm::vec3(-0.866f, 0.f, -0.5f));

	std::vector<glm::vec3> positions;
	// Road A: 20 pts east (10 m)
	for (int i = 0; i <= 20; ++i)
		positions.push_back({ i * step, 0.f, 0.f });
	// Road B: 80 pts along dir_b (40 m — long enough for apex at ~15 m)
	const glm::vec3 junction = positions.back();
	for (int i = 1; i <= 80; ++i)
		positions.push_back(junction + dir_b * (i * step));

	auto wps = make_waypoints(positions, hw, false);
	BikeCourse::compute_racing_line(wps, false);

	const int apex_idx = peak_lateral_idx(wps);
	t.check(apex_idx > 20, "150° sharp right: apex is past junction (on road B)");

	const float apex_lat = wps[apex_idx].racing_line_lateral;
	t.check(apex_lat > hw * 0.4f,
	        "150° sharp right: apex lateral is significantly inside");

	co_return;
}
GAME_TEST("racing_line/150deg_sharp_right_apex", 5.f, test_150deg_sharp_right_turn);

// ─────────────────────────────────────────────────────────────
// Test: apex world position is within road bounds
//
// For a 90° right turn, the apex position = waypoint.position + right * lateral.
// This must lie within road_half_width of the road centreline (|lateral| <= hw).
// ─────────────────────────────────────────────────────────────
static TestTask test_apex_within_road(TestContext& t) {
	const float hw   = 4.f;
	const float step = 0.5f;

	std::vector<glm::vec3> positions;
	for (int i = 0; i <= 20; ++i)
		positions.push_back({ i * step, 0.f, 0.f });
	for (int i = 1; i <= 20; ++i)
		positions.push_back({ 20 * step, 0.f, -i * step });

	auto wps = make_waypoints(positions, hw, false);
	BikeCourse::compute_racing_line(wps, false, 0.82f);

	bool all_in_bounds = true;
	for (const auto& wp : wps) {
		if (std::abs(wp.racing_line_lateral) > wp.road_half_width + 0.01f) {
			all_in_bounds = false;
			break;
		}
	}
	t.check(all_in_bounds, "all racing_line_lateral values stay within road_half_width");

	co_return;
}
GAME_TEST("racing_line/apex_within_road_bounds", 5.f, test_apex_within_road);
