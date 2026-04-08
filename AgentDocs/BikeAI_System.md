# Bike AI — Full System Design

## Overview

The AI system is built in six layers. Each layer depends on the one below it. Implement and validate in order.

| Layer | Name | Owns |
|---|---|---|
| 1 | Course & Waypoints | `BikeCourse`, `course_dist_m`, sorted rider list |
| 2 | Bike Control | PID steering, power targeting, `BikeAI::evaluate()` |
| 3 | Drafting Physics | Shared drag reduction, cone query, `draft_factor` |
| 4 | Boid Forces | Separation, alignment, draft-seeking |
| 5 | Lateral Push System | Contested lateral position, assertiveness, counter-push |
| 6 | Strategic AI | State machine, energy management, race decisions |

Layer 1 is fully specified in `BikeAI_CourseAndWaypoints.md`. This document specifies layers 2–6 and the full integration.

---

## Layer 2 — Bike Control

### Design Intent

AI riders use the same `ControlInput` interface as the player (`BikeObject::update_tick(ControlInput)`). The physics simulation is identical — AI bikes experience wind, terrain gradient, stamina drain, gear shifting, and crash physics. What differs is how `ControlInput` is constructed each frame.

AI control has two outputs:
- `steer` — a value in [-1, 1], produced by a PID controller
- `power` — watts, set by the strategy layer (Layer 6) and clamped by stamina

### PID Steering

The AI steers toward a **lookahead point** on the spline, not toward the next waypoint node directly. Steering toward nodes causes oscillation; a lookahead point gives smooth arc-following.

```
lookahead_pos = course.lookahead(my_course_dist, lookahead_dist_m)
```

`lookahead_dist_m` scales with speed: `lookahead_dist_m = speed * 1.5f + 3.f` (in metres). Faster riders look further ahead, which reduces oscillation at speed and tightens response when slow.

The steer error is the signed lateral angle to the lookahead point in the bike's local frame:

```
to_target   = normalize(lookahead_pos - bike_ws_pos)
steer_error = dot(to_target, bike_right)   // signed: +ve = target is to the right
```

PID:
```
steer_P = steer_kp * steer_error
steer_I += steer_error * dt  (clamped)
steer_D = steer_kd * (steer_error - prev_error) / dt

steer_output = clamp(steer_P + steer_ki * steer_I + steer_D, -1, 1)
```

Initial tuning values: `kp=1.8, ki=0.0, kd=0.25`. Ki is 0 by default — integral term is rarely needed for path following and can cause wind-up on long corners. Enable only if systematic lateral drift appears.

### Lateral Correction Blend

The PID steers toward the spline. The lateral push system (Layer 5) sets a `desired_lateral` offset from the spline. These blend:

```
total_steer = path_steer * 0.7 + lateral_correction_steer * 0.3
```

`lateral_correction_steer` = proportional controller: `(desired_lateral - current_lateral) * lateral_kp`. This is not a full PID — proportional only, because lateral position changes slowly and integral wind-up is a real risk in a pack.

### Power Targeting

The AI does not use `BIKE_POWER_LEVELS[]` discrete steps. It sets power continuously in watts, passed directly as `ControlInput::power`. The stamina system (`tick_stamina`) already clamps this to `power_ceiling`, so the AI does not need to guard against it.

The strategy layer sets `target_power_watts`. The control layer smooths it to avoid instantaneous jumps:

```
actual_power_command = damp_dt_independent(target_power_watts, actual_power_command, power_slew_rate, dt)
```

`power_slew_rate = 0.05f` — power ramps over ~1 second. This is realistic (you cannot instantly go from 200W to 800W) and prevents the strategy layer from causing physics instability.

### Simplified AI Physics Considerations

Full terrain raycasts for 50 bikes every frame at 60Hz = 3000 raycasts/frame. Instead:

- AI bikes share the waypoint gradient data: `terrain_gradient` is read from the nearest `BikeWaypoint::gradient` rather than live raycasting
- Gradient is updated at 10Hz (every 6 frames), interpolated between samples
- Crash detection runs for AI but at reduced rate (10Hz). AI "missing a corner" means their PID steers wide, not that they physically slide out — keep crash recovery logic but AI recovers instantly rather than waiting `crash_timer` seconds

### `BikeAI` Struct

```cpp
class BikeAI : public IBikeInput {
public:
    void evaluate(BikeObject* my_bike) final;

    // Waypoint following
    float lookahead_dist_m = 5.f;     // updated each frame from speed

    // PID steering
    float steer_kp = 1.8f, steer_ki = 0.f, steer_kd = 0.25f;
    float steer_integral = 0.f;
    float prev_steer_error = 0.f;

    // Power
    float target_power_watts   = 200.f;   // set by strategy layer
    float actual_power_command = 200.f;   // smoothed toward target
    float power_slew_rate      = 0.05f;

    // Lateral
    float desired_lateral = 0.f;      // set by pack/strategy logic
    float lateral_kp      = 1.2f;

    // Boid (written by pack update, read in evaluate)
    float boid_separation_steer = 0.f;
    float boid_align_power_nudge = 0.f;  // watts to add/subtract for alignment

    // Drafting (written by drafting update)
    float draft_factor = 1.0f;           // already written into BikeObject, mirrored here for debug

    // Strategy state (Layer 6)
    AIRaceState race_state = AIRaceState::Conserve;
    float strategy_timer   = 0.f;        // time until next strategy evaluation

    // Archetype
    RiderArchetype archetype = RiderArchetype::Rouleur;

    // Pack context (written by BikeGameApplication before evaluate)
    int    sorted_idx      = 0;          // this rider's index in riders_sorted
    float  gap_to_ahead_m  = 999.f;      // longitudinal gap to rider immediately ahead
    float  gap_to_behind_m = 999.f;
    BikeObject* rider_ahead  = nullptr;  // null if leading
    BikeObject* rider_behind = nullptr;
};
```

---

## Layer 3 — Drafting Physics

### Design Intent

Drafting is a shared physics effect. Both AI and the player benefit. It is computed in `BikeGameApplication::update()` before any rider's `evaluate()` runs. The result is written into `BikeObject::draft_factor`, which `tick_physics` reads when computing aero drag.

### Draft Cone

A rider is in the draft of a rider ahead if:
- **Longitudinal**: `long_gap = ahead.course_dist_m - my.course_dist_m` is in `[0.3m, 8.0m]`
- **Lateral**: `lat_gap = |ahead.lateral_pos - my.lateral_pos|` is less than `1.2m`

The draft benefit falls off with both gaps:

```
long_factor = 1.0 - clamp((long_gap - 0.3) / 7.7, 0, 1)    // 1.0 at 0.3m, 0.0 at 8.0m
lat_factor  = 1.0 - clamp(lat_gap / 1.2, 0, 1)              // 1.0 on centreline, 0.0 at 1.2m offset

raw_draft_benefit = long_factor * lat_factor * max_draft_benefit   // max_draft_benefit = 0.35
```

`draft_factor = 1.0 - raw_draft_benefit` (so 1.0 = no draft, 0.65 = full draft at ideal position).

### Stacking

If a rider is behind two riders (e.g., in a lead-out train), they get draft from both. Stacking is subadditive:

```
draft_factor = 1.0 - (benefit_from_A + benefit_from_B * 0.5)
draft_factor = clamp(draft_factor, 0.55, 1.0)   // hard floor: never less than 55% CdA
```

Check up to 5 riders ahead (from sorted list) for stacking contributions. Beyond 5 the contribution is negligible.

### Application in tick_physics

In `BikeObject::tick_physics`, the aero drag line becomes:

```cpp
float effective_CdA = base_CdA * draft_factor;
float aero_drag = 0.5f * air_density * effective_CdA * app_speed * fabsf(app_speed);
```

`draft_factor` is written into `BikeObject` each frame by the application update before physics runs. Default is 1.0 (no draft) so the player is unaffected when no riders are ahead.

### Draft Update Loop (BikeGameApplication::update)

```
for each rider at index i in riders_sorted:
    draft_factor = 1.0
    total_benefit = 0.0
    stack_weight = 1.0
    for j in [i-1 downto max(0, i-5)]:
        compute long_gap, lat_gap to riders_sorted[j]
        if in cone:
            total_benefit += compute_benefit(long_gap, lat_gap) * stack_weight
            stack_weight *= 0.5
    riders_sorted[i]->draft_factor = clamp(1.0 - total_benefit, 0.55, 1.0)
```

---

## Layer 4 — Boid Forces

### Design Intent

Boids govern how riders behave as a pack without explicit coordination. All three forces operate on scalars derived from the sorted list — no 3D spatial queries.

### Separation

Prevents lateral overlap between riders at similar course distance.

For each rider, scan neighbors within `±5m long_gap`. If any neighbor has `lat_gap < min_safe_lat` (0.6m):

```
separation_push = (min_safe_lat - lat_gap) / min_safe_lat   // 0..1, stronger when closer
separation_dir  = sign(my.lateral_pos - neighbor.lateral_pos)  // push away from them
separation_steer_contribution = separation_push * separation_strength * separation_dir
```

Accumulate contributions from all nearby neighbors. This is written into `BikeAI::boid_separation_steer` and added to the steer output in `evaluate()`.

Separation is the highest priority boid force — it overrides lateral correction. A rider being squeezed always moves away first.

### Alignment

Riders in a pack unconsciously match speed with their neighbours. This is what makes a peloton: everyone settles at the average pace of those around them.

For each rider, compute the mean speed of all riders within `±20m long_gap` (call it `pack_avg_speed`). Then:

```
speed_error = pack_avg_speed - my.speed
align_power_nudge = speed_error * align_kp   // align_kp ~ 15 W/(m/s)
```

`align_power_nudge` is added to `target_power_watts` (before the slew filter). This nudges power up or down to converge on pack speed. The nudge is bounded: `clamp(align_power_nudge, -60W, +60W)`. Strategy overrides this during attacks.

Alignment is suspended for a rider in `ATTACK` or `SPRINT` state — they are intentionally breaking from the pack.

### Draft Seeking

If a rider is in `CONSERVE` or `FOLLOW_WHEEL` state and their `gap_to_ahead_m > 2.5m`, they apply a mild power bonus to close the gap:

```
if gap_to_ahead_m > 2.5 and gap_to_ahead_m < 15.0:
    close_power_bonus = clamp((gap_to_ahead_m - 2.5) * 8.0, 0, 60)  // up to +60W
    target_power_watts += close_power_bonus
```

This keeps the pack together naturally without needing explicit cohesion logic. If the gap is >15m the rider may switch to `DROPPED` state (strategy layer decision).

---

## Layer 5 — Lateral Push System

### Design Intent

Lateral position in the pack is contested. Riders move to their `desired_lateral` by steering, but other riders physically impede them. A rider being pushed can counter-push back based on their assertiveness. This gives emergent jostling without scripted choreography.

### Per-Rider Lateral State

Added to `BikeObject`:

```cpp
float desired_lateral   = 0.f;   // target offset from road centre (m), set by strategy
float lateral_pos       = 0.f;   // current actual offset (updated each frame from project())
float push_received     = 0.f;   // net lateral push accumulated this frame (reset each tick)
float assertiveness     = 0.7f;  // [0,1] — how hard this rider resists/counter-pushes
```

### Push Resolution (BikeGameApplication::update, after sort, before evaluate)

For each pair (i, j) where riders are in contact:
- `long_gap < 2.5m` AND `lat_gap < 0.9m`

Rider i is moving toward their `desired_lateral`. If rider j is in the way of that motion:

```
push_intent_i = desired_lateral_i - lateral_pos_i       // how far and in what direction i wants to go
obstruction   = dot(sign(push_intent_i), sign(lateral_pos_i - lateral_pos_j)) > 0  // j is in the way
if obstruction:
    push_force = clamp(|push_intent_i|, 0, max_push) * assertiveness_i
    j.push_received += push_force * sign(push_intent_i)   // push j in the direction i wants to go
    // counter-push: j pushes back on i
    i.push_received -= push_force * assertiveness_j * 0.6
```

After all pairs are resolved, apply accumulated `push_received` to each rider's `desired_lateral`:

```
// Push moves the rider's desired position, not their actual position directly.
// Actual position follows via the lateral correction steer term.
desired_lateral += push_received * push_to_desired_scale   // push_to_desired_scale ~ 0.4
desired_lateral = clamp(desired_lateral, -road_half_width + 0.4, road_half_width - 0.4)
push_received = 0.f   // reset for next frame
```

Clamp keeps all riders on the road. The 0.4m margin prevents riding the gutter.

### What Sets desired_lateral?

The strategy layer writes `desired_lateral` each tick. Priority order (highest wins):

1. **Separation push** — escape an immediate collision (override everything)
2. **Sprint positioning** — move to preferred side (wind-sheltered, road camber)
3. **Follow wheel** — lateral target is `rider_ahead.lateral_pos + small_offset` (0.2–0.4m to one side, so you can see ahead)
4. **Gap threading** — scan neighbors at similar course_dist, find the widest available lateral gap, aim for its centre
5. **Conserve** — hold current lateral_pos (no active movement)

Gap threading (used when moving up through the pack):

```
collect lateral_pos of all riders within ±3m long_gap
sort them
find widest gap between adjacent entries (or between edge and road boundary)
if widest gap > min_gap_to_enter (1.0m):
    desired_lateral = centre of that gap
```

### Assertiveness by State

| State | Assertiveness |
|---|---|
| `CONSERVE` | 0.3 — yield, save energy |
| `FOLLOW_WHEEL` | 0.6 — hold your wheel, minor resistance |
| `ATTACK` | 0.9 — push for space actively |
| `SPRINT` | 1.0 — full push, no yield |
| `RECOVER` | 0.2 — spent, no energy to fight |
| `DROPPED` | 0.1 — doesn't matter |

Assertiveness transitions smoothly (via `damp_dt_independent`) when state changes, so a sprinting rider doesn't snap immediately to full assertiveness.

---

## Layer 6 — Strategic AI

### Design Intent

The strategy layer makes high-level race decisions on a slow tick (1.0–2.0s). It reads physiological state (stamina), race state (position, gaps, course ahead), and archetype personality to set:
- `target_power_watts`
- `desired_lateral`
- `race_state`

It does not directly control the bike. All outputs are consumed by lower layers.

### Race States

```cpp
enum class AIRaceState {
    Conserve,       // sit in pack, minimise effort, draft maximum
    FollowWheel,    // track a specific rider's wheel
    Attack,         // surge above FTP, burn W', try to open a gap
    Sprint,         // max power, final 300m
    Recover,        // after attack, drop back below FTP to recover W'
    Dropped,        // fell off the back, can no longer win — soft-pedal or quit
};
```

### Strategy Tick

Runs every `strategy_interval` seconds (1.0–2.0s, jittered ±0.3s per rider to avoid synchronised decisions across 50 riders).

```
evaluate_strategy(BikeObject* bike, BikeAI* ai, BikeCourse* course):
    race_progress = bike->course_dist_m / course->total_length_m
    dist_to_finish = course->total_length_m - bike->course_dist_m
    upcoming_gradient = course->sample(bike->course_dist_m + 300).gradient
    gap_to_leader = riders_sorted[0]->course_dist_m - bike->course_dist_m
    w_prime_frac = bike->stamina.w_prime / bike->rider.w_prime_max
    glycogen = bike->stamina.glycogen

    transition to new state based on rules below
    set target_power_watts and desired_lateral based on new state
```

### State Transition Rules

**→ DROPPED**: gap to the pack rear > 45m AND unable to close (power at ceiling). Once dropped, stay dropped unless gap closes.

**→ SPRINT**: `dist_to_finish < sprint_trigger_dist` where `sprint_trigger_dist` is archetype-dependent (sprinter: 350m, rouleur: 200m, climber: 150m). Sprint requires `w_prime_frac > 0.3` — a spent rider cannot sprint.

**→ ATTACK**: All of the following:
- race_progress > 0.4 (not too early)
- w_prime_frac > 0.55 (enough anaerobic reserve)
- glycogen > 0.35 (not cooked)
- upcoming_gradient > attack_gradient_threshold (archetype-dependent: climbers attack earlier on shallower grades)
- gap_to_leader < 60m (in contention)
- not already been in ATTACK within the last 90s (cool-down prevents repeated futile attacks)

**→ RECOVER**: transitions from ATTACK when `w_prime_frac < 0.25` OR the attack failed to open a gap (gap to nearest chaser is still <10m after 15s of attacking).

**→ FOLLOW_WHEEL**: default state when near the front of the pack and race_progress > 0.3. Sets `rider_to_follow` to the rider directly ahead.

**→ CONSERVE**: default state otherwise. Sit in the pack, take draft, recover W'.

### Power Targets by State

| State | target_power_watts |
|---|---|
| `CONSERVE` | `effective_ftp * 0.75` — aerobic, recovering W' |
| `FOLLOW_WHEEL` | Whatever keeps `gap_to_ahead_m < 2m` — uses draft-seek nudge from Layer 4 |
| `ATTACK` | `effective_ftp * 1.3` clamped to `power_ceiling` — burn W' |
| `SPRINT` | `power_ceiling` — max available |
| `RECOVER` | `effective_ftp * 0.60` — zone 2, W' recovery |
| `DROPPED` | `effective_ftp * 0.80` — ride out the stage |

### Archetypes

Three personality types that modify threshold constants in the strategy rules:

```cpp
enum class RiderArchetype { Climber, Sprinter, Rouleur };

struct ArchetypeParams {
    float attack_gradient_threshold;  // min gradient to trigger attack
    float sprint_trigger_dist;        // metres from finish to begin sprint
    float conserve_power_frac;        // fraction of FTP for CONSERVE state
    float attack_power_frac;          // fraction of FTP for ATTACK state
    float base_assertiveness;         // lateral assertiveness baseline
};

static const ArchetypeParams ARCHETYPE_PARAMS[] = {
    // Climber:   attacks hard on gradients, weak sprint, efficient in saddle
    { .attack_gradient_threshold = 0.025f, .sprint_trigger_dist = 150.f,
      .conserve_power_frac = 0.72f, .attack_power_frac = 1.40f, .base_assertiveness = 0.6f },
    // Sprinter:  conserves ruthlessly, explodes in final 300m, zero interest in hills
    { .attack_gradient_threshold = 999.f,  .sprint_trigger_dist = 350.f,
      .conserve_power_frac = 0.68f, .attack_power_frac = 1.20f, .base_assertiveness = 0.9f },
    // Rouleur:   steady tempo, breakaway specialist, attacks on long flats
    { .attack_gradient_threshold = -0.01f, .sprint_trigger_dist = 200.f,
      .conserve_power_frac = 0.78f, .attack_power_frac = 1.35f, .base_assertiveness = 0.75f },
};
```

Each AI bike is assigned an archetype at spawn. Mix roughly: 30% climbers, 30% sprinters, 40% rouleurs for a typical stage race peloton.

### Strategy Does Not Override Physics

The strategy layer sets targets. The stamina system (`tick_stamina`) enforces the actual ceiling. A rider in `ATTACK` state requesting 1.4× FTP will naturally be capped by `power_ceiling` as W' depletes. The strategy layer does not need to guard against this — the physics handles it.

---

## Data Flow Summary (per frame)

```
BikeGameApplication::update():
│
├── 1. project() all riders → course_dist_m, lateral_pos
├── 2. sort riders_sorted by course_dist_m descending
├── 3. write race_position, gap_to_ahead, gap_to_behind, rider_ahead into each BikeAI
│
├── 4. DRAFTING PASS
│       for each rider: scan up to 5 riders ahead, compute draft_factor → write to BikeObject
│
├── 5. LATERAL PUSH PASS
│       for each contact pair: compute push forces, accumulate push_received
│       apply push_received → update desired_lateral, clamp to road
│
├── 6. BOID PASS (separation steer, alignment power nudge, draft-seek power bonus)
│       write boid_separation_steer, boid_align_power_nudge into BikeAI
│
├── 7. STRATEGY PASS (throttled per rider: runs every 1–2s)
│       for each AI rider: evaluate state machine → set target_power_watts, desired_lateral, race_state
│
└── 8. RIDER UPDATE (all riders including player)
        for each rider: call input->evaluate(bike) → update_tick(ControlInput)
            BikeAI::evaluate():
                slew target_power_watts → actual_power_command
                run PID steer → path_steer
                compute lateral_correction_steer
                blend + add boid_separation_steer
                fill ControlInput, call update_tick()
```

---

## Spawn and Race Setup

```cpp
BikeObject* BikeGameApplication::create_ai(glm::vec3 pos, RiderArchetype archetype) {
    Entity* e = GameplayStatic::spawn_entity();
    e->set_ws_position(pos);
    auto* bo = e->create_component<BikeObject>();
    auto* ai = std::make_unique<BikeAI>();
    ai->archetype = archetype;
    // Assign rider stats from archetype
    bo->rider = make_rider_stats(archetype);
    bo->input = std::move(ai);
    all_riders.push_back(bo);
    return bo;
}
```

Spawn all 50 AI riders staggered along the first 100m of course. Stagger by ~2m intervals so the initial sort is already nearly correct and the first few frames don't have instability.

---

## Implementation Order

| Step | What | Validates |
|---|---|---|
| 1 | `BikeCourse`: `build()`, `project()`, `sample()`, `lookahead()` | Unit test: project round-trips correctly |
| 2 | `BikeWaypointMarker` component, load in `start()` | Debug draw: can see the spline in-game |
| 3 | `course_dist_m` / `lateral_pos` written per rider each frame | Debug: numbers update as player rides |
| 4 | Sort loop, `race_position`, `gap_to_ahead` | Debug: positions update correctly |
| 5 | Single AI bike: PID steering only, `target_power = 250W` | AI navigates the map |
| 6 | Spawn 5 AI bikes, verify no overlap (separation force only) | Pack stays spread, no interpenetration |
| 7 | Drafting pass + `draft_factor` in `tick_physics` | Player feels draft behind AI, debug shows factor |
| 8 | Alignment force | Loose pack coalesces to similar speed |
| 9 | Lateral push system | Riders jostle, don't phase through each other |
| 10 | Strategy layer: CONSERVE + FOLLOW_WHEEL only | Peloton rides together, drafts, holds position |
| 11 | Strategy: ATTACK + RECOVER | Attacks fire on climbs, W' depletes, riders recover |
| 12 | Strategy: SPRINT | Sprinters contest the line |
| 13 | Scale to 50 riders, profile | Confirm acceptable frame budget |
| 14 | Archetype mix, tuning | Races feel varied and realistic |

---

## Non-Goals

- **Voice/commentary**: not in scope.
- **AI crash recovery animation**: AI recovers from crashes instantly (timer skipped). Animation work deferred.
- **Multi-stage persistence**: stamina and race results between stages is game-layer logic, not AI-layer logic. The AI system exposes `StaminaState` for the game layer to save/restore.
- **Pathfinding around obstacles**: course is a road, no obstacles. The spline is the path.
- **Difficulty scaling**: archetype stats and strategy thresholds are the difficulty levers. Explicit "easy/hard" mode is a game-layer concern.
