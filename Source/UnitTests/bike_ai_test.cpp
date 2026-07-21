#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================
// BikeAI cohesion/avoidance unit tests
//
// These pure-function replicas mirror the formulas in BikeAI.cpp exactly
// (same variable names/signs) so a sign flip or logic inversion there is
// caught as a test failure here, without needing to bootstrap the engine.
//
// Sign conventions (match BikeAI.cpp / BikeApplication.cpp):
//   lateral_pos   road-right positive (+ve = right of centre)
//   lat_gap       other.lateral_pos - me.lateral_pos (+ve = other is road-right of me)
//   long_gap      other.course_dist_m - me.course_dist_m (+ve = other is ahead)
//   steer output  road-left positive (+1 = steer hard left)
// ============================================================

// ---------------------------------------------------------------------------
// Hemisphere sense — forward cone test. Real geometry only (dot product on
// the actual world-space direction), never array-index adjacency.
// ---------------------------------------------------------------------------
static bool in_forward_hemisphere(float fwd_dot, float half_angle_deg)
{
    return fwd_dot > std::cos(half_angle_deg * 3.14159265f / 180.f);
}

TEST(BikeAISense, DirectlyAhead_IsSensed)
{
    EXPECT_TRUE(in_forward_hemisphere(1.f, 100.f));  // dot=1 -> directly ahead
}

TEST(BikeAISense, DirectlyBehind_NotSensed_100deg)
{
    EXPECT_FALSE(in_forward_hemisphere(-1.f, 100.f));  // dot=-1 -> directly behind
}

TEST(BikeAISense, WideningAngle_IncludesMore)
{
    // fwd_dot = 0 (directly beside) — outside a 100deg half-angle cone (cos(100)= -0.17),
    // 0 > -0.17 so it's inside; but outside a 45deg cone (cos(45)=0.707).
    EXPECT_TRUE(in_forward_hemisphere(0.f, 100.f));
    EXPECT_FALSE(in_forward_hemisphere(0.f, 45.f));
}

// ---------------------------------------------------------------------------
// Cohesion "behind" — lateral pull into the nearest ahead-neighbor's wheel
// track (their lat_gap -> 0), plus a following-gap speed match, gated by
// cohesion_follow_dist_m longitudinally. This is the "draft" sub-term.
// ---------------------------------------------------------------------------
static float cohesion_behind_lat(float nearest_long_gap, float nearest_lat_gap,
                                  float cohesion_follow_dist_m, float cohesion_behind_k)
{
    if (nearest_long_gap >= cohesion_follow_dist_m) return 0.f;
    return nearest_lat_gap * cohesion_behind_k;
}

static float cohesion_gap_speed_target(float leader_speed, float nearest_long_gap,
                                        float cohesion_gap_m, float cohesion_gap_kp)
{
    const float gap_err = nearest_long_gap - cohesion_gap_m;
    return leader_speed + std::max(-3.f, std::min(3.f, gap_err * cohesion_gap_kp));
}

TEST(BikeAICohesionBehind, LeaderToRight_PullsRight)
{
    // nearest_lat_gap > 0 -> leader is road-right of me -> pull toward them
    // (an INCREASE in lateral_pos, road-right positive) -> positive term.
    EXPECT_GT(cohesion_behind_lat(2.f, 0.5f, 8.f, 0.5f), 0.f);
}

TEST(BikeAICohesionBehind, LeaderToLeft_PullsLeft)
{
    EXPECT_LT(cohesion_behind_lat(2.f, -0.5f, 8.f, 0.5f), 0.f);
}

TEST(BikeAICohesionBehind, OutsideFollowRange_NoTerm)
{
    EXPECT_EQ(cohesion_behind_lat(20.f, 0.5f, 8.f, 0.5f), 0.f);
}

TEST(BikeAICohesionBehind, AtFollowDistEdge_NoTerm)
{
    EXPECT_EQ(cohesion_behind_lat(8.f, 0.5f, 8.f, 0.5f), 0.f);
}

TEST(BikeAICohesionGapSpeed, GapTooLarge_SpeedsUp)
{
    // long_gap(6) > target gap(3) -> falling behind -> speed up beyond leader's own speed.
    EXPECT_GT(cohesion_gap_speed_target(8.f, 6.f, 3.f, 0.5f), 8.f);
}

TEST(BikeAICohesionGapSpeed, GapTooSmall_SlowsDown)
{
    // long_gap(1) < target gap(3) -> too close -> back off below leader's speed.
    EXPECT_LT(cohesion_gap_speed_target(8.f, 1.f, 3.f, 0.5f), 8.f);
}

TEST(BikeAICohesionGapSpeed, AtTargetGap_MatchesLeaderSpeed)
{
    EXPECT_NEAR(cohesion_gap_speed_target(8.f, 3.f, 3.f, 0.5f), 8.f, 1e-6f);
}

TEST(BikeAICohesionGapSpeed, CorrectionClampedToPlusMinus3)
{
    EXPECT_NEAR(cohesion_gap_speed_target(8.f, 1000.f, 3.f, 0.5f), 11.f, 1e-6f);
    EXPECT_NEAR(cohesion_gap_speed_target(8.f, -1000.f, 3.f, 0.5f), 5.f, 1e-6f);
}

// ---------------------------------------------------------------------------
// Cohesion "closer" — always-on lateral magnetism toward the lateral
// centroid of ALL sensed neighbors (ahead or behind), not gated by
// isolation or follow range.
// ---------------------------------------------------------------------------
static float cohesion_closer_lat(const std::vector<float>& neighbor_lat_gaps, float cohesion_closer_k)
{
    if (neighbor_lat_gaps.empty()) return 0.f;
    float sum = 0.f;
    for (float g : neighbor_lat_gaps) sum += g;
    return (sum / (float)neighbor_lat_gaps.size()) * cohesion_closer_k;
}

TEST(BikeAICohesionCloser, CentroidRight_PullsRight)
{
    std::vector<float> gaps = { 1.f, 1.5f };
    EXPECT_GT(cohesion_closer_lat(gaps, 0.3f), 0.f);
}

TEST(BikeAICohesionCloser, AlwaysOn_EvenWithCloseNeighbor)
{
    // Unlike the old isolation-gated cohesion, "closer" applies regardless of
    // how near the nearest neighbor is.
    std::vector<float> gaps = { 0.2f };
    EXPECT_NE(cohesion_closer_lat(gaps, 0.3f), 0.f);
}

TEST(BikeAICohesionCloser, NoNeighbors_NoTerm)
{
    EXPECT_EQ(cohesion_closer_lat({}, 0.3f), 0.f);
}

// ---------------------------------------------------------------------------
// Avoidance — decentralized, worldspace box-overlap resolution. Every rider
// shares the same prefab box (half-extents on each axis); two riders'
// boxes physically touch once the gap on an axis drops below TWICE the
// half-extent ("hard" boundary — the true drop-dead, must-never-overlap
// threshold). A soft margin OUTSIDE that hard boundary ramps severity 0->1
// linearly, so the reaction builds in instead of snapping to full force the
// instant the hard boundary is crossed; severity is pinned at 1 for any gap
// at/inside the hard boundary itself (never partial once actually
// overlapping). avoid_direction picks which way to yield (away from the
// conflicting neighbor, in the yielding rider's OWN right-vector sense, not
// road-relative); room_available checks the resulting position doesn't
// leave the road.
// ---------------------------------------------------------------------------
static float axis_severity(float gap_abs, float hard, float trigger)
{
    if (gap_abs >= trigger) return 0.f;
    if (gap_abs <= hard)    return 1.f;
    return (trigger - gap_abs) / (trigger - hard);
}

static float conflict_severity(float long_gap, float lat_gap,
                                float box_half_long, float box_half_lat,
                                float soft_margin_long, float soft_margin_lat)
{
    const float hard_long = 2.f * box_half_long;
    const float hard_lat  = 2.f * box_half_lat;
    const float trigger_long = hard_long + soft_margin_long;
    const float trigger_lat  = hard_lat  + soft_margin_lat;
    if (std::abs(long_gap) >= trigger_long) return 0.f;
    if (std::abs(lat_gap)  >= trigger_lat)  return 0.f;
    return axis_severity(std::abs(long_gap), hard_long, trigger_long)
         * axis_severity(std::abs(lat_gap),  hard_lat,  trigger_lat);
}

static float avoid_direction(float conflict_lat_gap)
{
    return (conflict_lat_gap >= 0.f) ? -1.f : 1.f;
}

static bool room_available(float candidate_lat, float road_limit)
{
    return std::abs(candidate_lat) <= road_limit;
}

// Direct worldspace lateral slide velocity (m/s), proportional to severity
// only — no integrator/momentum, so it has nothing to unwind once the
// conflict clears (that's what fixed the avoidance/cohesion "yoyo": routing
// this through the heading PID instead built up turn momentum that had to
// be unwound the same way, overshooting on both ends).
static float avoidance_lateral_vel_signed(float avoid_dir, float avoidance_lateral_speed_mps, float severity)
{
    return avoid_dir * avoidance_lateral_speed_mps * severity;
}

// Defaults matching BikeAIParams: box_half_long=0.9, box_half_lat=0.25,
// soft_margin_long=1.5, soft_margin_lat=0.5 -> hard_long=1.8, hard_lat=0.5,
// trigger_long=3.3, trigger_lat=1.0.

TEST(BikeAIAvoidance, FarLongitudinally_NoConflict)
{
    EXPECT_EQ(conflict_severity(5.f, 0.f, 0.9f, 0.25f, 1.5f, 0.5f), 0.f);
}

TEST(BikeAIAvoidance, FarLaterally_NoConflict)
{
    EXPECT_EQ(conflict_severity(0.5f, 2.f, 0.9f, 0.25f, 1.5f, 0.5f), 0.f);
}

TEST(BikeAIAvoidance, SamePoint_MaxSeverity)
{
    EXPECT_NEAR(conflict_severity(0.f, 0.f, 0.9f, 0.25f, 1.5f, 0.5f), 1.f, 1e-6f);
}

TEST(BikeAIAvoidance, AtHardBoxBoundary_StillMaxSeverity)
{
    // Right at the hard "boxes just touch" boundary — must still be full
    // severity, not a partial ramp value; the soft ramp is only OUTSIDE this.
    EXPECT_NEAR(conflict_severity(1.8f, 0.5f, 0.9f, 0.25f, 1.5f, 0.5f), 1.f, 1e-6f);
}

TEST(BikeAIAvoidance, HalfwayIntoBothSoftMargins_QuarterSeverity)
{
    // Halfway through each axis's soft margin (0.5 severity each) multiply,
    // not add: 0.5 * 0.5 = 0.25. long: hard=1.8, margin=1.5 -> gap=2.55.
    // lat: hard=0.5, margin=0.5 -> gap=0.75.
    EXPECT_NEAR(conflict_severity(2.55f, 0.75f, 0.9f, 0.25f, 1.5f, 0.5f), 0.25f, 1e-6f);
}

TEST(BikeAIAvoidance, CloseOnOneAxisOnly_NeverAsUrgentAsBoth)
{
    // Both axes deep inside the hard box (severity=1 each) vs. only one axis
    // inside the hard box while the other is partway through its soft margin.
    const float both          = conflict_severity(0.2f, 0.1f, 0.9f, 0.25f, 1.5f, 0.5f);
    const float one_axis_only = conflict_severity(0.2f, 0.8f, 0.9f, 0.25f, 1.5f, 0.5f);
    EXPECT_GT(both, one_axis_only);
}

TEST(BikeAIAvoidance, NeighborToRight_AvoidLeft)
{
    EXPECT_LT(avoid_direction(0.5f), 0.f);
}

TEST(BikeAIAvoidance, NeighborToLeft_AvoidRight)
{
    EXPECT_GT(avoid_direction(-0.5f), 0.f);
}

TEST(BikeAIAvoidance, WithinRoad_RoomAvailable)
{
    EXPECT_TRUE(room_available(1.5f, 3.2f));
}

TEST(BikeAIAvoidance, PastEdge_NoRoom)
{
    EXPECT_FALSE(room_available(3.5f, 3.2f));
}

TEST(BikeAIAvoidance, DeepConflict_FullSlideSpeed)
{
    // severity=1 -> the full configured slide speed.
    EXPECT_NEAR(avoidance_lateral_vel_signed(1.f, 3.5f, 1.f), 3.5f, 1e-6f);
}

TEST(BikeAIAvoidance, ShallowConflict_ScaledDownBySeverity)
{
    const float shallow = std::abs(avoidance_lateral_vel_signed(1.f, 3.5f, 0.1f));
    const float deep    = std::abs(avoidance_lateral_vel_signed(1.f, 3.5f, 1.f));
    EXPECT_LT(shallow, deep);
}

TEST(BikeAIAvoidance, ClearedConflict_ZeroVelocity_NoMomentumToUnwind)
{
    // severity=0 (conflict fully cleared) must give exactly zero, not a
    // decaying residual — there is no integrator/momentum state here.
    EXPECT_EQ(avoidance_lateral_vel_signed(1.f, 3.5f, 0.f), 0.f);
}

// ---------------------------------------------------------------------------
// Ground rule: cohesion/avoidance sums must be order-independent. Summing
// neighbor contributions in a different (shuffled) order must produce the
// same total — nothing in the formula may depend on iteration/array order.
// ---------------------------------------------------------------------------
TEST(BikeAIGroundRule, CohesionCloserSum_OrderIndependent)
{
    std::vector<float> a = { 0.5f, -0.3f, 0.1f };
    std::vector<float> b = { -0.3f, 0.1f, 0.5f };  // shuffled
    EXPECT_NEAR(cohesion_closer_lat(a, 0.3f), cohesion_closer_lat(b, 0.3f), 1e-6f);
}

// ---------------------------------------------------------------------------
// Hard clamp — cohesion can never command a target lateral position past
// (road_half_width - edge_safety_m). Clamping must apply to the offset
// target, not to a steer term (steer/heading is driven independently by the
// racing line PID and is never touched by this clamp). Avoidance is a
// separate, direct lateral_shift command and is NOT subject to this clamp.
// ---------------------------------------------------------------------------
static float clamp_target_lateral(float target_lat_raw, float road_half_width, float edge_safety_m)
{
    const float limit = std::max(road_half_width - edge_safety_m, 0.1f);
    return std::max(-limit, std::min(limit, target_lat_raw));
}

TEST(BikeAIHardClamp, WithinBounds_Unclamped)
{
    EXPECT_NEAR(clamp_target_lateral(1.f, 4.f, 0.8f), 1.f, 1e-6f);
}

TEST(BikeAIHardClamp, PastRightEdge_ClampedToLimit)
{
    // road_hw=4, edge_safety=0.8 -> limit=3.2; requesting 5.0 must clamp to 3.2
    EXPECT_NEAR(clamp_target_lateral(5.f, 4.f, 0.8f), 3.2f, 1e-6f);
}

TEST(BikeAIHardClamp, PastLeftEdge_ClampedToLimit)
{
    EXPECT_NEAR(clamp_target_lateral(-5.f, 4.f, 0.8f), -3.2f, 1e-6f);
}

TEST(BikeAIHardClamp, CohesionCannotForceOffTrack_EvenWithLargeClosePull)
{
    // A rider near the edge (lateral_pos = 3.0, road_hw=4, edge_safety=0.8, limit=3.2) with a
    // strong cohesion pull toward an off-track neighbor centroid must still clamp inside the limit.
    const float my_lateral_pos = 3.0f;
    const float huge_pull_delta = 10.f;  // way more than needed to cross the edge
    const float target_raw = my_lateral_pos + huge_pull_delta;
    const float clamped = clamp_target_lateral(target_raw, 4.f, 0.8f);
    EXPECT_LE(clamped, 3.2f + 1e-6f);
}

// ---------------------------------------------------------------------------
// Lateral shift command — P-control from target offset error to [-1,1]
// (cohesion/racing-line path only). Avoidance is NOT part of this at all —
// it never touches ci.lateral_shift or heading, see BikeAIAvoidance above.
// ---------------------------------------------------------------------------
static float lateral_shift_command(float target_lat, float current_lat, float kp)
{
    const float raw = (target_lat - current_lat) * kp;
    return std::max(-1.f, std::min(1.f, raw));
}

TEST(BikeAILateralShift, TargetRight_ShiftPositive)
{
    EXPECT_GT(lateral_shift_command(1.f, 0.f, 1.5f), 0.f);
}

TEST(BikeAILateralShift, TargetLeft_ShiftNegative)
{
    EXPECT_LT(lateral_shift_command(-1.f, 0.f, 1.5f), 0.f);
}

TEST(BikeAILateralShift, LargeError_ClampedToUnit)
{
    EXPECT_NEAR(lateral_shift_command(100.f, 0.f, 1.5f), 1.f, 1e-6f);
    EXPECT_NEAR(lateral_shift_command(-100.f, 0.f, 1.5f), -1.f, 1e-6f);
}

TEST(BikeAILateralShift, OnTarget_NoShift)
{
    EXPECT_NEAR(lateral_shift_command(2.f, 2.f, 1.5f), 0.f, 1e-6f);
}

// ---------------------------------------------------------------------------
// Steering PID (heading only — racing line tracking, never pack position).
// ---------------------------------------------------------------------------
static float steer_pid(float error, float integral, float prev_error, float dt,
                        float kp, float ki, float kd)
{
    const float deriv = (error - prev_error) / dt;
    const float raw = kp * error + ki * integral + kd * deriv;
    return std::max(-1.f, std::min(1.f, raw));
}

TEST(BikeAISteerPID, PositiveError_PositiveSteer)
{
    EXPECT_GT(steer_pid(0.2f, 0.f, 0.f, 0.016f, 2.f, 0.f, 0.f), 0.f);
}

TEST(BikeAISteerPID, ZeroErrorNoHistory_ZeroSteer)
{
    EXPECT_NEAR(steer_pid(0.f, 0.f, 0.f, 0.016f, 2.f, 0.1f, 0.15f), 0.f, 1e-6f);
}

TEST(BikeAISteerPID, LargeError_ClampedToUnit)
{
    EXPECT_NEAR(steer_pid(50.f, 0.f, 0.f, 0.016f, 2.f, 0.f, 0.f), 1.f, 1e-6f);
}

// ---------------------------------------------------------------------------
// Speed/power PID — no cohesion follow target (isolated) -> flat
// base_power_w. With a follow target -> PID-tracked power around base_power_w.
// ---------------------------------------------------------------------------
static float speed_power(bool have_target, float speed_error, float integral, float deriv,
                          float base_power_w, float kp, float ki, float kd,
                          float min_power_w, float max_power_w)
{
    if (!have_target) return base_power_w;
    const float pid_out = kp * speed_error + ki * integral + kd * deriv;
    const float raw = base_power_w + pid_out;
    return std::max(min_power_w, std::min(max_power_w, raw));
}

TEST(BikeAISpeedPower, NoNeighbor_ConstantCruisePower)
{
    EXPECT_NEAR(speed_power(false, 5.f, 5.f, 5.f, 250.f, 60.f, 5.f, 10.f, 50.f, 1000.f), 250.f, 1e-6f);
}

TEST(BikeAISpeedPower, BehindTarget_AddsPower)
{
    EXPECT_GT(speed_power(true, 2.f, 0.f, 0.f, 250.f, 60.f, 5.f, 10.f, 50.f, 1000.f), 250.f);
}

TEST(BikeAISpeedPower, AheadOfTarget_SubtractsPower)
{
    EXPECT_LT(speed_power(true, -2.f, 0.f, 0.f, 250.f, 60.f, 5.f, 10.f, 50.f, 1000.f), 250.f);
}

TEST(BikeAISpeedPower, ClampedToMin)
{
    EXPECT_NEAR(speed_power(true, -100.f, 0.f, 0.f, 250.f, 60.f, 5.f, 10.f, 50.f, 1000.f), 50.f, 1e-6f);
}
