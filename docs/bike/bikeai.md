# Bike AI plan

Replaces the leader/boid two-state model. One unified controller. The only decision a rider makes each frame is: whose wheel am I on.

## Per-rider state

- `wheel`: BikeObject* of the rider directly ahead I'm following. null = I'm a leader.
- `lat_offset`: float, meters, road-relative offset from the wheel's track. Continuous, not a lane index.
- `long_gap`: target longitudinal spacing behind wheel. Default ~0.5m.
- `tactical_state`: enum (Following, Pulling, Peeling, DriftingBack, Attacking, Bridging, Dropped).

## Target position

Computed every frame, fed to a PD steer + power controller.

```
if wheel != null:
    base  = wheel->get_ws_position()
    fwd   = wheel->bike_direction
    right = course->sample(wheel->course_dist_m).right
else:
    wp    = course->sample(my_course_dist_m)
    base  = wp.racing_line_pos
    fwd   = wp.forward
    right = wp.right
target = base - fwd * long_gap + right * (lat_offset * corner_factor)
```

The offset is in the road frame at the wheel's position so it bends through corners automatically. There is no world-fixed lane grid.

`corner_factor = clamp(min_r_ahead / 30.0, 0.3, 1.0)` — tight corners pull the formation toward single file.

## Lateral offset rule

`lat_offset` is continuous, not chosen from a menu. It is set by clear-air resolution, not lane assignment.

- Default 0 (best draft).
- If another rider already occupies the slot at offset 0 behind the same wheel (within ~1m lat, ~1m long), shift sideways toward more open air.
- Per-rider personality bias adds ~±0.2m preference.
- Low-pass damp `lat_offset` over ~1.5s so riders commit to a side.

Resulting structure (single file, 2-up, 3-up, echelons) is emergent from clear-air + draft, not authored.

## Wheel picking

Each frame, score candidates: same group, ahead of me by 0.3–8m, lateral overlap within ~2m.
Score = w1·(closeness to ideal long_gap) + w2·(lateral alignment to my offset) + w3·draft_factor + stickiness_bonus_for_current_wheel.
If best score below threshold → wheel = null, I am a leader.

This subsumes `is_leader` and `num_leaders_per_group`. Leadership is emergent: solo breakaways, dropped riders, and front pairs all fall out of the same query.

Front pair case: A has no candidate ahead → leader. B finds A → wheel = A. B sees A occupying offset 0 → shifts to ±0.4m. 2-up at the front, no special case.

## Steering

Single PD path. Target the computed `target` position with the lookahead-based steer already in `BikeAI::evaluate`. Drop the boid_desired_dir / boid_steer_override branch.

Keep: edge avoidance, off-track recovery, anticipatory braking.

## Power

If wheel: match `wheel->stamina.actual_power` + P-control on `long_gap` error. The existing update_gap_regulation already does this, but should switch from cone-scan to using the explicit wheel pointer.
If leader: tactical target watts (set by Pulling state or strategic layer).

Drafting (`draft_factor`) unchanged.

## Tactical FSM

Per rider, modifies wheel-pick + lat_offset + power. All transitions are simple thresholds (timers, fatigue, group position).

- Following: default. Wheel-pick + draft.
- Pulling: leader, hold target tempo. Triggered by becoming leader-by-emergence and cooldown elapsed.
- Peeling: 2-3s. Force lat_offset toward ±1.0m, power -50W. After timeout returns to Following — wheel-pick will reacquire from drifting back.
- DriftingBack: power -30% until a stable wheel is found behind the current group position.
- Attacking: wheel forced null, lat_offset pinned, power +30%, fatigue accelerated.
- Bridging: chasing a forward group, max sustainable power.
- Dropped: cannot hold any wheel, solo on racing line.

## Off-course prevention

Three-layer hierarchy: prevent, correct, rescue. Edge safety is a constraint applied to the output of wheel-following, not woven into it.

1. Target clamp. After clear-air resolution, clamp `lat_offset` so `wheel.lateral_pos + lat_offset` stays within `±(road_hw - edge_safety_m)`. The rider is never asked to aim off-road.
2. PD edge avoidance (existing in `BikeAI::evaluate`): arc-prediction + PD on `lateral_pos` vs safe edge, additive while on-track. Catches inertia, wind, mid-corner drift the clamp doesn't predict.
3. Off-track recovery (existing): `|lateral_pos| > road_hw` → override steer with edge_steer + apply edge_brake. Wheel-following bypassed for the duration.

Wheel-picker reinforcement: penalize candidates whose required offset clips the road edge, so flank slots self-thin. `corner_factor` already pulls `lat_offset → 0` in tight corners, keeping riders away from the outside line through the apex.

## Rider-rider avoidance

Same shape: prevent, predict, rescue.

1. Clear-air resolver (steady state). The `lat_offset` rule queries every nearby rider, not just same-wheel followers. For candidate target_pos, if any other rider's target or current position is within ~1m lat × ~1m long, shift `lat_offset` until clear. Smoothed ~1.5s.
2. Predictive avoidance (existing `avoidance_sep_steer`): arc-prediction + 0.5s/1s overlap → additive sep_steer. Fast reflex layer. Catches surges, brake events, line changes.
3. Priority yield + squeeze brake (existing `hard_steer_min/max` + `avoidance_brake`): last-resort. Lower race position is clamped from steering into higher; brake to scrub if boxed.
4. Long-gap regulation (rear-end): wheel-following power controller already P-controls long_gap. If wheel brakes hard, gap shrinks, power cuts, anticipatory braking takes over.

Tactical-state priority modifies the resolver:
- Following → defers (widens offset).
- MovingUp / Attacking → priority bonus, holds line.
- Peeling → others let them through.

Removes the need for the `boid_separation_*` repulsive force — the resolver subsumes it.

## Code to remove

- `coh_lat` mean-position blob in `update_boids`.
- Alignment toward mean neighbor heading.
- `is_leader`, `num_leaders_per_group`.
- Direct-steer override path: `boid_forces_active`, `boid_desired_dir`, `boid_steer_override`, `boid_turn_rate_override`, and the `in_boid_mode` branch in `BikeAI::evaluate`.
- Boid params: `boid_separation_*`, `boid_cohesion_k`, `boid_alignment_k`, `boid_turn_k`, `boid_max_turn_rate`.

## Code to keep / repurpose

- Drafting (`update_drafting`): unchanged.
- Predictive lateral avoidance (`update_boids` avoidance section): becomes the clear-air resolver for `lat_offset`.
- Priority yield + squeeze brake: keep as last-resort safety; should rarely fire if the offset resolver is working.
- Edge avoidance, off-track recovery, anticipatory braking, gap regulation core: keep.

## Implementation order

Each step independently testable.

1. Add `wheel` pointer + wheel-picker. Wire it but do not change behavior yet (compare picked wheel vs current heuristics in debug HUD).
2. Replace the boid-mode steer path with target-pos PD steer using `lat_offset = 0`. Should produce single-file pelotons.
3. Add clear-air resolver for `lat_offset`.
4. Add `corner_factor` compression.
5. Strip removed code listed above.
6. Layer tactical FSM (Pulling / Peeling / DriftingBack first; Attacking / Bridging / Dropped after).
7. Personality bias on `lat_offset`.

## Sign conventions

See [[bike/sign_conventions]]. `right` from BikeWaypoint points world-right (+X for +Z forward), so `lat_offset > 0` = road-right of the wheel.
