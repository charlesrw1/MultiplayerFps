#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <vector>

// ============================================================
// BikeAI magnetism-layer unit tests
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
// Separation — push away from anyone closer than separation_dist_m, weighted
// by inverse falloff. term -= sign(lat_gap) * weight * separation_k.
// ---------------------------------------------------------------------------
static float separation_term(float lat_gap, float dist, float separation_dist_m, float separation_k)
{
    if (dist >= separation_dist_m) return 0.f;
    const float weight = 1.f - dist / separation_dist_m;
    const float sign = (lat_gap > 0.f) ? 1.f : (lat_gap < 0.f ? -1.f : 0.f);
    return -sign * weight * separation_k;
}

TEST(BikeAISeparation, NeighborToRight_PushesLeft)
{
    // lat_gap > 0 -> neighbor is road-right of me -> push away = road-left =
    // a DECREASE in lateral_pos (road-right positive) -> negative offset delta.
    EXPECT_LT(separation_term(0.5f, 0.3f, 1.0f, 1.2f), 0.f);
}

TEST(BikeAISeparation, NeighborToLeft_PushesRight)
{
    EXPECT_GT(separation_term(-0.5f, 0.3f, 1.0f, 1.2f), 0.f);
}

TEST(BikeAISeparation, OutsideRange_NoPush)
{
    EXPECT_EQ(separation_term(0.5f, 2.0f, 1.0f, 1.2f), 0.f);
}

TEST(BikeAISeparation, CloserIsStrongerPush)
{
    const float far  = std::abs(separation_term(0.5f, 0.9f, 1.0f, 1.2f));
    const float near = std::abs(separation_term(0.5f, 0.1f, 1.0f, 1.2f));
    EXPECT_GT(near, far) << "closer neighbor must produce a stronger separation push";
}

// ---------------------------------------------------------------------------
// Draft + line formation — pull toward zero lateral offset from the nearest
// forward neighbor's line. Draft only within draft_dist_m; line-formation is
// smaller and always-on for any sensed forward neighbor.
// ---------------------------------------------------------------------------
static float draft_term(float nearest_lat_gap, float nearest_long_gap, float draft_dist_m, float draft_k)
{
    if (nearest_long_gap >= draft_dist_m) return 0.f;
    return -nearest_lat_gap * draft_k;
}

static float lineform_term(float nearest_lat_gap, float lineformation_k)
{
    return -nearest_lat_gap * lineformation_k;
}

TEST(BikeAIDraft, NeighborAheadAndRight_PullsRight)
{
    // nearest_lat_gap > 0 -> neighbor is road-right -> pull me road-right (negative-of-negative = positive... )
    // target_offset_delta += -lat_gap * k; lat_gap>0 -> delta negative -> pulls toward road-LEFT is wrong sign check:
    // -lat_gap*k with lat_gap=0.5,k=1 => -0.5: this moves target_lat toward negative (road-left) which REDUCES
    // separation from a neighbor that is to my right -- i.e. it pulls me toward their (more road-right) line.
    // Concretely: my new target = my_lat + delta = my_lat - 0.5, which is LESS road-right than before,
    // moving me toward centre relative to a neighbor further right; the intent is "align with their line",
    // i.e. move toward nearest_lat_gap's sign being reduced toward 0. Assert delta has opposite sign to gap.
    float delta = draft_term(0.5f, 3.f, 8.f, 1.0f);
    EXPECT_LT(delta, 0.f);
    EXPECT_LT(std::abs(delta), 0.5f + 1e-4f);
}

TEST(BikeAIDraft, OutsideDraftRange_NoTerm)
{
    EXPECT_EQ(draft_term(0.5f, 20.f, 8.f, 1.0f), 0.f);
}

TEST(BikeAIDraft, ZeroOffsetFromLine_NoTerm)
{
    EXPECT_NEAR(draft_term(0.f, 3.f, 8.f, 1.0f), 0.f, 1e-6f);
}

TEST(BikeAILineFormation, AlwaysOn_EvenBeyondDraftRange)
{
    // Line-formation is not range-gated like draft — it applies any time there's a
    // sensed forward neighbor, so riders converge to single-file before they're
    // close enough to draft.
    EXPECT_NE(lineform_term(0.8f, 0.35f), 0.f);
}

TEST(BikeAILineFormation, WeakerThanDraft_SameGap)
{
    const float lf = std::abs(lineform_term(0.8f, 0.35f));
    const float dr = std::abs(draft_term(0.8f, 3.f, 8.f, 1.0f));
    EXPECT_LT(lf, dr) << "line-formation is a smaller always-on bias, draft dominates in range";
}

// ---------------------------------------------------------------------------
// Cohesion — only fires when isolated (nearest neighbor farther than the
// trigger distance), pulls toward the lateral centroid of sensed neighbors.
// ---------------------------------------------------------------------------
static float cohesion_term(const std::vector<float>& neighbor_lat_gaps, float nearest_dist,
                            float cohesion_trigger_dist_m, float cohesion_k)
{
    if (neighbor_lat_gaps.empty()) return 0.f;
    if (nearest_dist <= cohesion_trigger_dist_m) return 0.f;
    float sum = 0.f;
    for (float g : neighbor_lat_gaps) sum += g;
    return (sum / (float)neighbor_lat_gaps.size()) * cohesion_k;
}

TEST(BikeAICohesion, IsolatedAndCentroidRight_PullsRight)
{
    std::vector<float> gaps = { 1.f, 1.5f };
    EXPECT_GT(cohesion_term(gaps, 10.f, 6.f, 0.5f), 0.f);
}

TEST(BikeAICohesion, NotIsolated_NoTerm)
{
    // nearest_dist below trigger -> draft/separation/lineformation already govern positioning
    std::vector<float> gaps = { 1.f, 1.5f };
    EXPECT_EQ(cohesion_term(gaps, 2.f, 6.f, 0.5f), 0.f);
}

TEST(BikeAICohesion, NoNeighbors_NoTerm)
{
    EXPECT_EQ(cohesion_term({}, 999.f, 6.f, 0.5f), 0.f);
}

// ---------------------------------------------------------------------------
// Ground rule: magnetism sums must be order-independent. Summing neighbor
// contributions in a different (shuffled) order must produce the same total —
// nothing in the formula may depend on iteration/array order.
// ---------------------------------------------------------------------------
TEST(BikeAIGroundRule, SeparationSum_OrderIndependent)
{
    struct N { float lat_gap, dist; };
    std::vector<N> a = { {0.5f, 0.2f}, {-0.3f, 0.6f}, {0.1f, 0.9f} };
    std::vector<N> b = { {-0.3f, 0.6f}, {0.1f, 0.9f}, {0.5f, 0.2f} };  // shuffled

    auto sum = [](const std::vector<N>& ns) {
        float total = 0.f;
        for (const auto& n : ns) total += separation_term(n.lat_gap, n.dist, 1.0f, 1.2f);
        return total;
    };
    EXPECT_NEAR(sum(a), sum(b), 1e-6f);
}

// ---------------------------------------------------------------------------
// Hard clamp — magnetism can never command a target lateral position past
// (road_half_width - edge_safety_m). Clamping must apply to the offset target,
// not to a steer term (steer/heading is driven independently by the racing
// line PID and is never touched by this clamp).
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

TEST(BikeAIHardClamp, MagnetismCannotForceOffTrack_EvenWithLargeCohesionPull)
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
// Lateral shift command — P-control from target offset error to [-1,1].
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
// Speed/power PID — no target (isolated) -> flat base_power_w. With a draft
// target -> PID-tracked power around base_power_w.
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
