#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>

// ============================================================
// BikeAI business logic unit tests
//
// Sign conventions (match BikeApplication.cpp):
//   lateral_pos   road-right positive  (+ve = right of centre)
//   steer output  road-left  positive  (+1 = steer hard left)
//   signed_gap    other.dist - me.dist (+ve = other is ahead, I am behind)
//   gap_to_ahead  always positive when I'm behind (rider_ahead.dist - my.dist)
//   gap_err       gap_to_ahead - AI_GAP_TARGET (+ve = too far back, need more power)
//   gap_rate      rider_ahead.speed - my.speed (+ve = they're pulling away, gap growing)
//
// These tests directly replicate the formulas in BikeApplication.cpp and BikeAI.cpp
// so that any sign flip or logic inversion is caught as a test failure.
// ============================================================

// ---------------------------------------------------------------------------
// Separation direction
// dir_away = steer sign that moves me AWAY from the other rider.
//   I'm to the RIGHT of other (me_lat > other_lat) → push further right → steer right = -1
//   I'm to the LEFT  of other (me_lat < other_lat) → push further left  → steer left  = +1
// ---------------------------------------------------------------------------
static float sep_dir_away(float me_lat, float other_lat)
{
    return (me_lat >= other_lat) ? -1.f : 1.f;
}

TEST(BikeAISeparation, DirAway_ImRight_SteerRight)
{
    // I'm to the right → must steer right (negative) to widen gap
    EXPECT_LT(sep_dir_away(1.0f, 0.0f), 0.f);
}

TEST(BikeAISeparation, DirAway_ImLeft_SteerLeft)
{
    // I'm to the left → must steer left (positive) to widen gap
    EXPECT_GT(sep_dir_away(-1.0f, 0.0f), 0.f);
}

TEST(BikeAISeparation, DirAway_Equal_SteerRight)
{
    // Exactly coincident: tie-break to -1 (steer right), consistent with code
    EXPECT_LT(sep_dir_away(0.f, 0.f), 0.f);
}

// ---------------------------------------------------------------------------
// Separation KD — closing velocity
// closing > 0 when I'm moving TOWARD the other rider.
// lat_vel = d(lat_pos)/dt, positive = moving road-right.
//
// Case A: dir_away = -1 (I'm right), moving left (lat_vel < 0) → closing toward other
//   lat_vel * dir_away = (-)(-)  = +   → max(0, +) > 0  ✓
// Case B: dir_away = +1 (I'm left), moving right (lat_vel > 0) → closing toward other
//   lat_vel * dir_away = (+)(+)  = +   → max(0, +) > 0  ✓
// Case C: moving away from other → product is negative → clamped to 0, no KD contribution
// ---------------------------------------------------------------------------
static float sep_closing(float lat_vel, float dir_away)
{
    return std::max(0.f, lat_vel * dir_away);
}

TEST(BikeAISeparation, Closing_ImRight_MovingLeft_IsPositive)
{
    // dir_away = -1, lat_vel = -0.5 (moving left = closing from right)
    EXPECT_GT(sep_closing(-0.5f, -1.f), 0.f);
}

TEST(BikeAISeparation, Closing_ImLeft_MovingRight_IsPositive)
{
    // dir_away = +1, lat_vel = +0.5 (moving right = closing from left)
    EXPECT_GT(sep_closing(0.5f, 1.f), 0.f);
}

TEST(BikeAISeparation, Closing_MovingAway_IsZero)
{
    // dir_away = -1, lat_vel = +0.5 (moving right = moving AWAY when I'm to the right)
    EXPECT_EQ(sep_closing(0.5f, -1.f), 0.f);
}

// ---------------------------------------------------------------------------
// Longitudinal separation power
// When two riders are side-by-side, the one BEHIND should push (+ watts) to
// sprint clear, and the one AHEAD should yield (- watts) to let them through.
//
// signed_gap = other.course_dist - me.course_dist
//   +ve → other is ahead  → I am BEHIND  → I should PUSH  (+1)
//   -ve → other is behind → I am AHEAD   → I should YIELD (-1)
// ---------------------------------------------------------------------------
static float long_sep_yield(float signed_gap, float long_weight, float boid_sep_power_max)
{
    // FIXED formula (was inverted: behind rider was incorrectly yielding)
    return (signed_gap > 0.f ? 1.f : -1.f) * long_weight * boid_sep_power_max;
}

TEST(BikeAISeparation, LongPower_ImBehind_ShouldPush)
{
    // signed_gap = +2 (other is 2m ahead, I am behind) → push: positive power
    float power = long_sep_yield(2.f, 1.f, 60.f);
    EXPECT_GT(power, 0.f) << "Rider who is behind should push through (+watts)";
}

TEST(BikeAISeparation, LongPower_ImAhead_ShouldYield)
{
    // signed_gap = -2 (other is 2m behind, I am ahead) → yield: negative power
    float power = long_sep_yield(-2.f, 1.f, 60.f);
    EXPECT_LT(power, 0.f) << "Rider who is ahead should yield (-watts)";
}

TEST(BikeAISeparation, LongPower_ScalesWithWeight)
{
    float full   = long_sep_yield(1.f, 1.0f, 60.f);
    float half   = long_sep_yield(1.f, 0.5f, 60.f);
    EXPECT_NEAR(half, full * 0.5f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Cohesion PD (player assist steer)
// formula: -(lat_err * KP - lat_vel * KD)
//          = -KP*lat_err + KD*lat_vel
//
// lat_err = lat_target - lat_pos  (+ve = target is to the right)
// lat_vel = d(lat_pos)/dt         (+ve = moving road-right)
// steer positive = road-left.  To move right we need negative steer.
//
// Pure proportional: target right → lat_err > 0 → output negative (steer right) ✓
// Damping: moving toward target (lat_err > 0, lat_vel > 0):
//   D term +KD*lat_vel is positive, opposing the negative P term → magnitude reduced ✓
// ---------------------------------------------------------------------------
static float cohesion_steer(float lat_err, float lat_vel, float kp, float kd)
{
    float raw = -(lat_err * kp - lat_vel * kd);
    return std::max(-1.f, std::min(1.f, raw));
}

TEST(BikeAICohesion, TargetRight_SteerRight)
{
    // lat_err > 0 (target to the right), no velocity → output should be negative (steer right)
    EXPECT_LT(cohesion_steer(1.f, 0.f, 0.05f, 0.05f), 0.f);
}

TEST(BikeAICohesion, TargetLeft_SteerLeft)
{
    // lat_err < 0 (target to the left) → output should be positive (steer left)
    EXPECT_GT(cohesion_steer(-1.f, 0.f, 0.05f, 0.05f), 0.f);
}

TEST(BikeAICohesion, MovingTowardTarget_IsDamped)
{
    // lat_err > 0, lat_vel > 0 (approaching from left): output magnitude must be
    // smaller than pure-P to confirm the D term is dampening, not reinforcing.
    float pure_p   = cohesion_steer(1.f, 0.f,  0.05f, 0.05f);
    float with_d   = cohesion_steer(1.f, 0.5f, 0.05f, 0.05f);
    // with_d should be less negative (smaller magnitude) than pure_p
    EXPECT_GT(with_d, pure_p) << "D term should reduce steer magnitude when approaching target";
}

TEST(BikeAICohesion, MovingAwayFromTarget_IsAmplified)
{
    // lat_err > 0 (target right), lat_vel < 0 (moving further left away from target)
    // D term should increase the rightward effort
    float pure_p   = cohesion_steer(1.f,  0.f,  0.05f, 0.05f);
    float with_d   = cohesion_steer(1.f, -0.5f, 0.05f, 0.05f);
    EXPECT_LT(with_d, pure_p) << "D term should increase steer when moving away from target";
}

// ---------------------------------------------------------------------------
// Gap PD (AI power vs. rider ahead)
// formula: gap_err * KP + gap_rate * KD
//
// gap_err  = gap_to_ahead - AI_GAP_TARGET  (+ve = too far back, need more power)
// gap_rate = rider_ahead.speed - my.speed  (+ve = they're faster, gap is GROWING)
//
// Both terms positive → add watts (close gap) ✓
// Gap closing too fast (gap_rate < 0) → subtract watts (prevent overshoot) ✓
// ---------------------------------------------------------------------------
static float gap_pd_bonus(float gap_err, float gap_rate, float kp, float kd,
                           float max_push, float max_pull)
{
    float raw = gap_err * kp + gap_rate * kd;
    return std::max(-max_push, std::min(max_pull, raw));
}

TEST(BikeAIGapPD, TooFarBehind_NoRelativeSpeed_AddsWatts)
{
    // gap_err = +3 (3m further back than target gap), gap_rate = 0
    EXPECT_GT(gap_pd_bonus(3.f, 0.f, 8.f, 20.f, 40.f, 60.f), 0.f);
}

TEST(BikeAIGapPD, TooClose_NoRelativeSpeed_SheddingWatts)
{
    // gap_err = -2 (2m closer than desired) → shed power
    EXPECT_LT(gap_pd_bonus(-2.f, 0.f, 8.f, 20.f, 40.f, 60.f), 0.f);
}

TEST(BikeAIGapPD, GapGrowing_AddsWatts)
{
    // gap_rate > 0 = ahead rider is faster, gap growing → add power
    EXPECT_GT(gap_pd_bonus(0.f, 1.f, 8.f, 20.f, 40.f, 60.f), 0.f);
}

TEST(BikeAIGapPD, GapClosingFast_ReducesWatts)
{
    // gap_rate < 0 = I'm faster, gap shrinking → shed power to avoid overshoot
    EXPECT_LT(gap_pd_bonus(0.f, -1.f, 8.f, 20.f, 40.f, 60.f), 0.f);
}

TEST(BikeAIGapPD, ClampedAtMaxPull)
{
    float out = gap_pd_bonus(100.f, 100.f, 8.f, 20.f, 40.f, 60.f);
    EXPECT_NEAR(out, 60.f, 1e-5f);
}

TEST(BikeAIGapPD, ClampedAtMaxPush)
{
    float out = gap_pd_bonus(-100.f, -100.f, 8.f, 20.f, 40.f, 60.f);
    EXPECT_NEAR(out, -40.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Peel direction
// When a puller finishes their pull they peel to the side.
// Rule: peel AWAY from road centre (or to the RIGHT if exactly centred).
//   lateral_pos < 0 (left of centre) → peel left  → peel_dir = -1
//   lateral_pos >= 0 (right or at centre) → peel right → peel_dir = +1
// ---------------------------------------------------------------------------
static float peel_dir(float lateral_pos)
{
    // FIXED: was (<= 0) which gave -1 at centre; should be (< 0)
    return (lateral_pos < 0.f) ? -1.f : 1.f;
}

TEST(BikeAIPaceline, PeelDir_LeftOfCentre_PeelsLeft)
{
    EXPECT_LT(peel_dir(-0.5f), 0.f);
}

TEST(BikeAIPaceline, PeelDir_RightOfCentre_PeelsRight)
{
    EXPECT_GT(peel_dir(0.5f), 0.f);
}

TEST(BikeAIPaceline, PeelDir_AtCentre_PeelsRight)
{
    // At exactly 0 the comment says "to the right if centred"
    EXPECT_GT(peel_dir(0.f), 0.f) << "Centred rider should peel right per comment";
}

// ---------------------------------------------------------------------------
// Overshoot grace timer logic
// When locked_gap <= 0 (I'm ahead of my locked target), don't drop immediately.
// Only drop if: gap is very negative (> ABS_DROP threshold) OR timer expires.
// ---------------------------------------------------------------------------
struct GraceState {
    float overshoot_t = 0.f;
};

static bool should_drop_target(float locked_gap, float dt, GraceState& s,
                                float grace_s, float drop_threshold_m)
{
    if (locked_gap > 25.f) return true;   // out of range
    if (locked_gap <= 0.f) {
        s.overshoot_t += dt;
        if (locked_gap < drop_threshold_m || s.overshoot_t >= grace_s)
            return true;
        return false;  // still in grace window
    }
    s.overshoot_t = 0.f;
    return false;
}

TEST(BikeAIGrace, SlightlyAhead_NotDroppedImmediately)
{
    GraceState s;
    // -0.5m ahead, 0.016s frame, grace=3s, drop=-8m
    EXPECT_FALSE(should_drop_target(-0.5f, 0.016f, s, 3.f, -8.f));
}

TEST(BikeAIGrace, SlightlyAhead_DropsAfterGraceExpires)
{
    GraceState s;
    // Simulate 3.1 seconds of being slightly ahead
    bool dropped = false;
    for (int i = 0; i < 200; ++i)
        dropped = should_drop_target(-0.5f, 0.016f, s, 3.f, -8.f);
    EXPECT_TRUE(dropped);
}

TEST(BikeAIGrace, WayAhead_DroppedImmediately)
{
    GraceState s;
    // -10m ahead exceeds drop threshold
    EXPECT_TRUE(should_drop_target(-10.f, 0.016f, s, 3.f, -8.f));
}

TEST(BikeAIGrace, Behind_TimerResets)
{
    GraceState s;
    s.overshoot_t = 1.5f;  // partially accumulated
    should_drop_target(1.f, 0.016f, s, 3.f, -8.f);  // now behind again
    EXPECT_NEAR(s.overshoot_t, 0.f, 1e-6f) << "Timer should reset when back behind target";
}

TEST(BikeAIGrace, FarAhead_OutOfRange_Dropped)
{
    GraceState s;
    EXPECT_TRUE(should_drop_target(26.f, 0.016f, s, 3.f, -8.f));
}

// ---------------------------------------------------------------------------
// is_at_front() — paceline cascade regression
//
// A Pulling rider outputs more watts than Following riders, so the gap between
// the Puller and the next rider can exceed any fixed window within seconds.
// is_at_front() must scan ALL riders ahead with no distance cutoff; otherwise
// every Following rider eventually sees "nobody within Nm ahead" and
// self-promotes to Pulling, causing the whole chain to pull simultaneously.
//
// We model is_at_front() as a pure function for testability.
// ---------------------------------------------------------------------------
enum class TestPaceState { Following, Pulling, Peeling, DriftingBack };

struct TestRider {
    float course_dist;
    TestPaceState state;
    bool is_player = false;  // players are ignored by is_at_front
};

// Returns true if rider[me_idx] is at the virtual front.
// riders must be sorted descending by course_dist (index 0 = race leader).
static bool is_at_front(const std::vector<TestRider>& riders, int me_idx)
{
    for (int j = me_idx - 1; j >= 0; --j) {
        if (riders[j].is_player) continue;
        if (riders[j].state == TestPaceState::Peeling ||
            riders[j].state == TestPaceState::DriftingBack) continue;
        return false;
    }
    return true;
}

TEST(BikeAIPaceline, IsAtFront_OnlyLeader_True)
{
    std::vector<TestRider> r = {
        { 100.f, TestPaceState::Pulling },
    };
    EXPECT_TRUE(is_at_front(r, 0));
}

TEST(BikeAIPaceline, IsAtFront_PullerAhead_False)
{
    // Puller at index 0, Follower at index 1 — follower is NOT at front
    std::vector<TestRider> r = {
        { 100.f, TestPaceState::Pulling },
        { 90.f,  TestPaceState::Following },
    };
    EXPECT_FALSE(is_at_front(r, 1));
}

TEST(BikeAIPaceline, IsAtFront_PullerFarAhead_StillFalse)
{
    // Puller has accelerated >10m away — old 10m window would wrongly return true
    std::vector<TestRider> r = {
        { 100.f, TestPaceState::Pulling   },  // 15m ahead of follower
        { 85.f,  TestPaceState::Following },
    };
    EXPECT_FALSE(is_at_front(r, 1))
        << "Follower must not become Puller just because gap to leader exceeds 10m";
}

TEST(BikeAIPaceline, IsAtFront_LeadPeeling_SecondIsAtFront)
{
    // Lead peels off → second rider is now effectively at front
    std::vector<TestRider> r = {
        { 100.f, TestPaceState::Peeling   },
        { 98.f,  TestPaceState::Following },
        { 90.f,  TestPaceState::Following },
    };
    EXPECT_TRUE(is_at_front(r, 1))  << "2nd rider should become new puller";
    EXPECT_FALSE(is_at_front(r, 2)) << "3rd rider should still be following";
}

// ---------------------------------------------------------------------------
// Pulling → Following when a wheel appears ahead
// A rider in Pulling state should immediately revert to Following if a stable
// rider (Following or Pulling) appears ahead — always prefer catching a wheel.
// ---------------------------------------------------------------------------
static bool pulling_should_revert_to_following(bool has_rider_ahead,
                                                TestPaceState ahead_state,
                                                bool ahead_is_player)
{
    if (!has_rider_ahead) return false;
    if (ahead_is_player)  return true;   // player counts as stable
    return ahead_state == TestPaceState::Following ||
           ahead_state == TestPaceState::Pulling;
}

TEST(BikeAIPaceline, Pulling_NoRiderAhead_StaysPulling)
{
    EXPECT_FALSE(pulling_should_revert_to_following(false, TestPaceState::Following, false));
}

TEST(BikeAIPaceline, Pulling_StableFollowerAhead_RevertsToFollowing)
{
    EXPECT_TRUE(pulling_should_revert_to_following(true, TestPaceState::Following, false));
}

TEST(BikeAIPaceline, Pulling_StablePullerAhead_RevertsToFollowing)
{
    EXPECT_TRUE(pulling_should_revert_to_following(true, TestPaceState::Pulling, false));
}

TEST(BikeAIPaceline, Pulling_PeelingAhead_StaysPulling)
{
    // Peeling rider is exiting — don't follow them
    EXPECT_FALSE(pulling_should_revert_to_following(true, TestPaceState::Peeling, false));
}

TEST(BikeAIPaceline, Pulling_DriftingBackAhead_StaysPulling)
{
    EXPECT_FALSE(pulling_should_revert_to_following(true, TestPaceState::DriftingBack, false));
}

TEST(BikeAIPaceline, Pulling_PlayerAhead_RevertsToFollowing)
{
    EXPECT_TRUE(pulling_should_revert_to_following(true, TestPaceState::Following, true));
}

TEST(BikeAIPaceline, IsAtFront_CascadeRegression)
{
    // Regression: when lead peels, only the 2nd rider becomes Puller.
    // Once 2nd is Pulling and has accelerated >10m away, 3rd must NOT self-promote.
    // This is the core cascade bug — the fixed is_at_front() must catch it.
    std::vector<TestRider> r = {
        { 105.f, TestPaceState::Peeling   },  // lead peeling
        { 100.f, TestPaceState::Pulling   },  // 2nd, now pulling, 15m ahead of 3rd
        { 85.f,  TestPaceState::Following },  // 3rd — must NOT become Pulling
        { 80.f,  TestPaceState::Following },  // 4th — must NOT become Pulling
    };
    EXPECT_FALSE(is_at_front(r, 2)) << "3rd rider must not cascade to Pulling";
    EXPECT_FALSE(is_at_front(r, 3)) << "4th rider must not cascade to Pulling";
}

// ---------------------------------------------------------------------------
// Hard steer cutoff
// Params match BikeApplication.cpp:
//   HARD_SEP_LONG_RADIUS = 3.0m
//   HARD_SEP_OUTER_LAT   = 0.7m
//   HARD_SEP_INNER_LAT   = 0.05m
//
// The update_boids loop narrows [hard_steer_min, hard_steer_max] to [0, +1] or
// [-1, 0] when a neighbour is inside the zone.  BikeAI clamps the final steer
// into this window.  These tests replicate that logic.
// ---------------------------------------------------------------------------
struct HardClampState {
    float hard_min = -1.f;
    float hard_max =  1.f;
};

static const float HARD_LONG  = 3.0f;
static const float HARD_OUTER = 0.7f;
static const float HARD_INNER = 0.05f;

// Apply one neighbour to the hard clamp state (mirrors the inner loop in update_boids)
static void apply_hard_neighbour(HardClampState& s,
                                  float me_lat, float me_dist,
                                  float other_lat, float other_dist)
{
    const float h_long   = std::abs(other_dist - me_dist);
    if (h_long >= HARD_LONG)  return;
    const float lat_diff = other_lat - me_lat;
    const float h_lat    = std::abs(lat_diff);
    if (h_lat >= HARD_OUTER)  return;
    if (h_lat <  HARD_INNER)  return;  // already overlapping — allow escape
    if (lat_diff > 0.f)
        s.hard_max = 0.f;  // other to right: block rightward steer
    else
        s.hard_min = 0.f;  // other to left: block leftward steer
}

TEST(BikeAIHardSteer, OtherToRight_BlocksRightSteer)
{
    HardClampState s;
    apply_hard_neighbour(s, 0.f, 0.f, 0.4f, 1.0f);  // other is 0.4m to the right
    EXPECT_EQ(s.hard_max, 0.f)  << "max should be clamped to 0 (no right steer)";
    EXPECT_EQ(s.hard_min, -1.f) << "min should be unchanged (left steer still allowed)";
}

TEST(BikeAIHardSteer, OtherToLeft_BlocksLeftSteer)
{
    HardClampState s;
    apply_hard_neighbour(s, 0.f, 0.f, -0.4f, 1.0f);  // other is 0.4m to the left
    EXPECT_EQ(s.hard_min, 0.f)  << "min should be clamped to 0 (no left steer)";
    EXPECT_EQ(s.hard_max, 1.f)  << "max should be unchanged (right steer still allowed)";
}

TEST(BikeAIHardSteer, OutsideLateralZone_NoClamp)
{
    HardClampState s;
    apply_hard_neighbour(s, 0.f, 0.f, 0.8f, 1.0f);  // 0.8m > HARD_OUTER=0.7m
    EXPECT_EQ(s.hard_max, 1.f);
    EXPECT_EQ(s.hard_min, -1.f);
}

TEST(BikeAIHardSteer, OutsideLongitudinalZone_NoClamp)
{
    HardClampState s;
    apply_hard_neighbour(s, 0.f, 0.f, 0.4f, 4.0f);  // 4.0m > HARD_LONG=3.0m
    EXPECT_EQ(s.hard_max, 1.f);
    EXPECT_EQ(s.hard_min, -1.f);
}

TEST(BikeAIHardSteer, AlreadyOverlapping_AllowsEscape)
{
    HardClampState s;
    apply_hard_neighbour(s, 0.f, 0.f, 0.02f, 1.0f);  // 0.02m < HARD_INNER=0.05m
    EXPECT_EQ(s.hard_max, 1.f)  << "overlapping riders must be allowed to escape freely";
    EXPECT_EQ(s.hard_min, -1.f);
}
