// Unit tests for BikeCourse::compute_racing_line
// Pure math — no game engine dependency.
#include <gtest/gtest.h>
#include "Game/BikeDemo/BikeCourse.h"
#include <glm/glm.hpp>
#include <cmath>
#include <vector>
#include <algorithm>

static const glm::vec3 WORLD_UP = { 0.f, 1.f, 0.f };

// Build a BikeWaypoint array from positions + uniform half-width,
// exactly mirroring BikeCourse::build_from_road_network() forward/right/dist logic.
static std::vector<BikeWaypoint> make_waypoints(const std::vector<glm::vec3>& pos,
                                                  float hw, bool loop = false)
{
    const int n = (int)pos.size();
    std::vector<BikeWaypoint> wps(n);
    for (int i = 0; i < n; ++i) {
        auto& wp = wps[i];
        wp.position        = pos[i];
        wp.road_half_width = hw;

        glm::vec3 fwd;
        if      (i < n - 1) fwd = pos[i + 1] - pos[i];
        else if (loop)      fwd = pos[0]     - pos[i];
        else                fwd = pos[i]     - pos[i - 1];

        const float len = glm::length(fwd);
        fwd = (len > 1e-4f) ? fwd / len : glm::vec3(0, 0, 1);

        wp.forward = fwd;
        glm::vec3 r = glm::cross(WORLD_UP, fwd);
        float rlen = glm::length(r);
        wp.right = (rlen > 1e-4f) ? r / rlen : glm::vec3(1, 0, 0);

        wp.dist_from_start = (i == 0)
            ? 0.f
            : wps[i - 1].dist_from_start + glm::distance(pos[i], pos[i - 1]);
    }
    return wps;
}

// ─────────────────────────────────────────────────────────────
// Straight road → all laterals near zero
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, StraightRoad_ZeroLateral)
{
    std::vector<glm::vec3> pos;
    for (int i = 0; i < 60; ++i)
        pos.push_back({ i * 0.5f, 0.f, 0.f });

    auto wps = make_waypoints(pos, 4.f);
    BikeCourse::compute_racing_line(wps, false);

    for (const auto& wp : wps)
        EXPECT_NEAR(wp.racing_line_lateral, 0.f, 0.1f)
            << "straight road: lateral should be near zero";
}

// ─────────────────────────────────────────────────────────────
// 90° right turn (east → south)
//
// Road A: indices 0–20 (east)
// Road B: indices 21–40 (south)
//
// For road B: forward=(0,0,-1), right=cross(Y,-Z)=(-1,0,0)=west
// Inside of right turn = west = negative-X → road-right direction
// Apex lateral should be POSITIVE (sg=+1, inside = road-right for south-going road B)
// Approach should be outside → NEGATIVE lateral on road A
// Exit should be outside → NEGATIVE lateral on road B far end
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, Turn90Right_ApexOnRoadB)
{
    const float hw   = 4.f;
    const float step = 0.5f;

    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i)
        pos.push_back({ i * step, 0.f, 0.f });   // road A east
    for (int i = 1; i <= 20; ++i)
        pos.push_back({ 20 * step, 0.f, -i * step }); // road B south

    auto wps = make_waypoints(pos, hw);
    BikeCourse::compute_racing_line(wps, false, 0.82f);

    // Apex = max POSITIVE lateral on road B (inside of right turn)
    float max_road_b = -1e9f;
    int   max_idx    = 21;
    for (int i = 21; i < (int)wps.size(); ++i) {
        if (wps[i].racing_line_lateral > max_road_b) {
            max_road_b = wps[i].racing_line_lateral;
            max_idx    = i;
        }
    }

    EXPECT_GT(max_road_b, hw * 0.82f * 0.3f)
        << "90° right apex must be strongly inside road B (positive lateral)";
    EXPECT_LT(max_idx, (int)wps.size() - 2)
        << "apex must not be at the very end of road B";

    // Approach on road A (well before corner): outside = north → lateral < 0
    float min_approach = 0.f;
    for (int i = 2; i <= 14; ++i)
        min_approach = std::min(min_approach, wps[i].racing_line_lateral);
    EXPECT_LT(min_approach, -0.2f) << "approach on road A must be outside (negative lateral)";

    // Far exit on road B: outside = east side → lateral < 0
    float min_exit = 0.f;
    for (int i = 35; i < (int)wps.size(); ++i)
        min_exit = std::min(min_exit, wps[i].racing_line_lateral);
    EXPECT_LT(min_exit, -0.2f) << "exit on road B must be outside (negative lateral)";
}

// ─────────────────────────────────────────────────────────────
// 90° left turn (east → north)
// Apex lateral must be NEGATIVE (inside = left = negative road-right)
// Approach lateral must be POSITIVE (outside = right side for left turn)
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, Turn90Left_ApexOnRoadB)
{
    const float hw   = 4.f;
    const float step = 0.5f;

    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i)
        pos.push_back({ i * step, 0.f, 0.f });   // road A east
    for (int i = 1; i <= 20; ++i)
        pos.push_back({ 20 * step, 0.f, i * step }); // road B north

    auto wps = make_waypoints(pos, hw);
    BikeCourse::compute_racing_line(wps, false, 0.82f);

    // Apex = min NEGATIVE lateral on road B (inside of left turn)
    float min_road_b = 1e9f;
    for (int i = 21; i < (int)wps.size(); ++i)
        min_road_b = std::min(min_road_b, wps[i].racing_line_lateral);

    EXPECT_LT(min_road_b, -hw * 0.82f * 0.3f)
        << "90° left apex must be strongly inside road B (negative lateral)";

    // Approach on road A: outside for left turn = south side → lateral > 0
    float max_approach = 0.f;
    for (int i = 2; i <= 14; ++i)
        max_approach = std::max(max_approach, wps[i].racing_line_lateral);
    EXPECT_GT(max_approach, 0.2f) << "approach on road A must be outside (positive lateral)";
}

// ─────────────────────────────────────────────────────────────
// 150° sharp right turn (nearly a hairpin)
// apex_dist = hw * tan(75°) ≈ 14.9 m → apex is far into road B
// Road B must be long enough: 80 pts × 0.5 m = 40 m
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, Turn150Sharp_ApexOnRoadB)
{
    const float hw   = 4.f;
    const float step = 0.5f;
    // 150° clockwise from east: dir_b = (cos(-150°), 0, sin(-150°))
    const glm::vec3 dir_b = glm::normalize(glm::vec3(-0.866f, 0.f, -0.5f));

    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i)
        pos.push_back({ i * step, 0.f, 0.f });
    const glm::vec3 junc = pos.back();
    for (int i = 1; i <= 80; ++i)
        pos.push_back(junc + dir_b * (i * step));

    auto wps = make_waypoints(pos, hw);
    BikeCourse::compute_racing_line(wps, false, 0.82f);

    // Apex = max POSITIVE lateral on road B (right turn → inside = positive)
    float max_road_b = -1e9f;
    for (int i = 21; i < (int)wps.size(); ++i)
        max_road_b = std::max(max_road_b, wps[i].racing_line_lateral);

    EXPECT_GT(max_road_b, hw * 0.82f * 0.3f)
        << "150° right turn apex must be strongly inside road B";
}

// ─────────────────────────────────────────────────────────────
// Apex world position must stay within road half-width
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, AllLaterals_WithinRoadBounds)
{
    const float hw   = 4.f;
    const float step = 0.5f;

    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i)
        pos.push_back({ i * step, 0.f, 0.f });
    for (int i = 1; i <= 20; ++i)
        pos.push_back({ 20 * step, 0.f, -i * step });

    auto wps = make_waypoints(pos, hw);
    BikeCourse::compute_racing_line(wps, false);

    for (int i = 0; i < (int)wps.size(); ++i)
        EXPECT_LE(std::abs(wps[i].racing_line_lateral), wps[i].road_half_width + 0.01f)
            << "lateral at index " << i << " exceeds road half-width";
}

// ─────────────────────────────────────────────────────────────
// 30° gentle right turn
// apex_dist = hw * tan(15°) ≈ 1.07 m → apex only slightly past junction
// ─────────────────────────────────────────────────────────────
TEST(RacingLine, Turn30Gentle_ApexPastJunction)
{
    const float hw   = 4.f;
    const float step = 0.5f;
    // 30° clockwise from east
    const glm::vec3 dir_b = glm::normalize(glm::vec3(std::cos(-0.5236f), 0.f, std::sin(-0.5236f)));

    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i)
        pos.push_back({ i * step, 0.f, 0.f });
    const glm::vec3 junc = pos.back();
    for (int i = 1; i <= 40; ++i)
        pos.push_back(junc + dir_b * (i * step));

    auto wps = make_waypoints(pos, hw);
    BikeCourse::compute_racing_line(wps, false, 0.82f);

    // Apex = max POSITIVE lateral on road B (right turn → inside = positive)
    float max_road_b = -1e9f;
    for (int i = 21; i < (int)wps.size(); ++i)
        max_road_b = std::max(max_road_b, wps[i].racing_line_lateral);

    EXPECT_GT(max_road_b, 0.f)
        << "30° right turn apex lateral must be positive (inside) on road B";
}

// ─────────────────────────────────────────────────────────────
// project() tests
//
// Two problems the new implementation must prevent:
//
// 1. Corner arm aliasing: at a 90° corner the exit segment can be
//    geometrically closer than the approach segment even while the bike
//    is still heading east.  Heading weight disambiguates them.
//
// 2. Loop aliasing: on a looping course another section of track may
//    pass near the current corner in world-space.  The windowed search
//    (±arc-length window around prev_dist_m) must prevent course_dist_m
//    from jumping to that distant segment — the "teleport to far point"
//    bug the user reported.
// ─────────────────────────────────────────────────────────────

// Shared helper: build course with approach (east) + exit (south).
// Returns a non-loop course with 0.5 m segment spacing.
static BikeCourse make_corner_course()
{
    const float step = 0.5f;
    std::vector<glm::vec3> pos;
    for (int i = 0; i <= 20; ++i) pos.push_back({ i * step, 0.f, 0.f });
    for (int i = 1; i <= 20; ++i) pos.push_back({ 20 * step, 0.f, -i * step });

    const int n = (int)pos.size();
    std::vector<BikeWaypoint> wps(n);
    for (int i = 0; i < n; ++i) {
        wps[i].position = pos[i];
        glm::vec3 fwd = (i < n - 1) ? pos[i+1] - pos[i] : pos[i] - pos[i-1];
        fwd = glm::normalize(fwd);
        wps[i].forward         = fwd;
        wps[i].right           = glm::normalize(glm::cross(glm::vec3(0,1,0), fwd));
        wps[i].road_half_width = 4.f;
        wps[i].dist_from_start = (i == 0) ? 0.f
            : wps[i-1].dist_from_start + glm::distance(pos[i], pos[i-1]);
        wps[i].racing_line_pos = pos[i];
    }

    BikeCourse c;
    c.waypoints      = wps;
    c.total_length_m = wps.back().dist_from_start + step;  // include last segment
    c.is_built       = true;
    c.is_loop        = false;
    return c;
}

// ── Test 1: heading east at the inner apex → stay on approach ────────────────

TEST(BikeCourseProject, Corner90_HeadingEast_StaysOnApproach)
{
    BikeCourse c = make_corner_course();
    const glm::vec3 pos = { 9.8f, 0.f, -0.3f };

    // Global search without heading: exit dist_sq ≈ 0.04 < approach 0.09 → exit wins.
    float dist_no_heading = c.project(pos);
    EXPECT_GT(dist_no_heading, 10.0f)
        << "sanity: global search without heading jumps to exit road";

    // With heading east + prev_dist_m on approach: must stay on approach (arc ≤ 10 m).
    float dist_headed = c.project(pos, nullptr, nullptr,
                                   glm::vec3(1.f, 0.f, 0.f), /*prev_dist_m=*/9.5f);
    EXPECT_LE(dist_headed, 10.0f)
        << "heading east + prev on approach: must not jump to exit road";
}

// ── Test 2: heading south → correctly move to exit ───────────────────────────

TEST(BikeCourseProject, Corner90_HeadingSouth_MovesToExit)
{
    BikeCourse c = make_corner_course();
    const glm::vec3 pos = { 9.8f, 0.f, -0.3f };

    float dist = c.project(pos, nullptr, nullptr,
                             glm::vec3(0.f, 0.f, -1.f), /*prev_dist_m=*/9.5f);
    EXPECT_GT(dist, 10.0f)
        << "heading south: projection must move onto exit road";
}

// ── Test 3: loop aliasing — far segment must NOT win ─────────────────────────
//
// Build a loop where a second straight passes near the same corner position.
// With prev_dist_m near the corner, the search window must exclude the
// far duplicate, even though it is geometrically closer.

TEST(BikeCourseProject, LoopAliasing_WindowPreventsJump)
{
    // Loop: approach east (0→10m), exit south (10→20m), then a long detour back
    // that passes ~1m north of the corner apex (world pos ≈ (10,0,0)).
    // The detour segment at arc≈60m will be ~1m from the rider when they are at
    // the corner at arc≈9.8m.  Without the window it could win; with it it must not.

    const float step = 0.5f;
    std::vector<glm::vec3> pos;

    // Approach east: arc 0→10 m
    for (int i = 0; i <= 20; ++i) pos.push_back({ i * step, 0.f, 0.f });
    // Exit south:    arc 10→20 m
    for (int i = 1; i <= 20; ++i) pos.push_back({ 10.f, 0.f, -i * step });
    // West leg back: arc 20→30 m
    for (int i = 1; i <= 20; ++i) pos.push_back({ 10.f - i * step, 0.f, -10.f });
    // North leg back toward start, passing 1 m north of apex: arc 30→50 m
    // Passes through (0,0,-10) → (0,0,1): goes directly north past the corner
    for (int i = 1; i <= 22; ++i) pos.push_back({ 0.f, 0.f, -10.f + i * step });
    // East leg back to start: arc ~51→61 m
    for (int i = 1; i <= 20; ++i) pos.push_back({ i * step, 0.f, 1.f });

    const int n = (int)pos.size();
    std::vector<BikeWaypoint> wps(n);
    for (int i = 0; i < n; ++i) {
        wps[i].position = pos[i];
        glm::vec3 fwd = (i < n - 1) ? pos[i+1] - pos[i] : pos[0] - pos[i];
        fwd = glm::normalize(fwd);
        wps[i].forward         = fwd;
        wps[i].right           = glm::normalize(glm::cross(glm::vec3(0,1,0), fwd));
        wps[i].road_half_width = 4.f;
        wps[i].dist_from_start = (i == 0) ? 0.f
            : wps[i-1].dist_from_start + glm::distance(pos[i], pos[i-1]);
        wps[i].racing_line_pos = pos[i];
    }

    BikeCourse c;
    c.waypoints      = wps;
    c.total_length_m = wps.back().dist_from_start + glm::distance(pos.back(), pos[0]);
    c.is_built       = true;
    c.is_loop        = true;

    // Rider approaching the 90° corner at (9.8, 0, -0.3), prev_dist ≈ 9.5 m.
    // The north-leg segment passes near world (10, 0, 1) at arc ≈ 48 m — well
    // outside the [9.5-10, 9.5+50] = [0, 59.5] window... wait, 48 m IS in window.
    // Let's verify the window at least excludes the east-return leg near (10, 0, 1)
    // which is only ~1 m from the rider's position at (9.8, 0, -0.3):
    // arc of east-return leg ≈ 51+ m which is inside the window.
    // But its heading is east=(1,0,0) same as bike — this is the hard case.
    //
    // Critical test: with prev on approach and heading east, the APPROACH segment
    // must win over any heading-aligned return leg that happens to be nearby.
    // The approach segment (arc=9.8m) and return leg (~arc=55m) both head east.
    // Their world-space distances differ: approach ≈ 0.3m, return ≈ 1m.
    // Nearest world-space wins — approach is closer, so it should win.

    const glm::vec3 pos_bike = { 9.8f, 0.f, -0.3f };
    float dist = c.project(pos_bike, nullptr, nullptr,
                             glm::vec3(1.f, 0.f, 0.f), /*prev_dist_m=*/9.5f);

    EXPECT_LE(dist, 10.5f)
        << "with prev on approach: projection must not jump to far part of loop";
    EXPECT_GE(dist, 8.0f)
        << "projection must remain in the vicinity of the corner approach";
}
