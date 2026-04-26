# Bike AI Improvements — TODO

## 1. Braking Anticipation
Current: brakes reactively when `speed > v_max` at the current corner.

- [ ] Add a separate forward scan (e.g. 3–5 seconds at current speed) to find the tightest upcoming corner radius ahead.
- [ ] Compute braking distance: `d_brake = (v² - v_max²) / (2 · decel)` where decel ≈ brake_force/mass.
- [ ] Begin braking when the corner is within `d_brake` metres, not when already in it.
- [ ] Make scan distance a tunable `AI_BRAKE_SCAN_M` constant.

## 2. Steering Anticipation (Damped Steering Compensation)
Current: AI feeds raw steer based on current lookahead. The committed_steer has asymmetric inertia (fast build ~0.002, slow release), so the AI lags corners.

- [ ] Add a second, further lookahead point used only for anticipation (e.g. `lookahead_dist * anticipation_scale`).
- [ ] Compute required steer for that future point.
- [ ] Blend the anticipation steer into the output: `steer_out = steer_k * angle_now + anticipation_k * angle_future`.
- [ ] Tune `anticipation_scale` and `anticipation_k` so the AI pre-loads the turn before the committed_steer rise time.
- [ ] Feed anticipated steer into braking too: if future steer demand is high, start braking earlier.

## 3. Off-Line Recovery
Current: lookahead gradually pulls AI back, but drifting far off line causes the target to pull through obstacles or overshoot.

- [ ] Compute cross-track error: signed lateral distance from bike position to nearest racing line point.
- [ ] Add a recovery steer term: `recovery_steer = cross_track_err * recovery_k`.
- [ ] Blend: large cross-track error → weight toward nearest-point steer; small error → pure lookahead.
- [ ] Cap recovery steer so it doesn't fight boid separation completely.

## 4. Track Boundary Avoidance
Current: none. AI can run off track if boid or braking forces push it wide.

- [ ] Compute lateral distance to each track edge (can use course width data or raycasts).
- [ ] Apply a steer impulse proportional to `1 / dist_to_edge` when within `EDGE_WARN_M` (~2 m).
- [ ] Make this a **hard override** — runs after boid steer, can clamp final steer output.
- [ ] Expose `EDGE_WARN_M` and edge avoidance gain as tunable constants.

## 5. Close-Range Collision Avoidance
Current: boid separation handles inter-rider spacing softly. Very close gaps (<0.5 m) have no hard response.

- [ ] Add a close-range avoidance term that activates below `COLLISION_HARD_DIST` (~0.5 m lateral).
- [ ] Steer impulse: inversely proportional to lateral gap, direction away from nearest rider.
- [ ] This stacks on top of boid separation but is scaled much higher so it wins at close range.
- [ ] Ensure it does not fight boundary avoidance (prioritize whichever is stronger).

## 6. Wind Behaviors
Current: wind system exists (`g_wind`) but AI does not use it.

### 6a. Seek Draft
- [ ] For each nearby rider within draft cone (~1 m lateral, ~5 m behind), compute draft benefit.
- [ ] Add a steer and power term that nudges AI into the cone of the rider-ahead with best draft angle vs. wind.
- [ ] Draft seek should be a soft additive term (boid-style), not a hard override.

### 6b. Hide from Wind / Seek Shelter
- [ ] When not racing hard (power below threshold), AI steers toward the sheltered side of the pack relative to wind direction.
- [ ] Compute wind-relative pack centroid and sheltered offset.
- [ ] Apply as a cohesion-style steer term weighted by `wind_speed`.

## 7. Group Position Seeking
Current: gap-following PD targets only the single rider directly ahead.

### 7a. Seek Group Front
- [ ] When AI is in an aggressive mode/state, increase `target_power_watts` when gap to group leader exceeds threshold.
- [ ] Could reuse existing gap PD but target the front-of-pack rider instead of rider_ahead.

### 7b. Seek Group Back
- [ ] When AI is in a recovery mode, reduce power and let gap to group grow.
- [ ] Useful for realistic dropped-rider behavior and peloton dynamics.

## 8. Parameter Tuning via CMA-ES
Current: constants are hand-tuned. CMA-ES (Covariance Matrix Adaptation Evolution Strategy) is an evolutionary optimizer that maintains a covariance matrix over the parameter space, taking larger steps in directions that improve fitness and smaller steps in directions that don't. Converges in ~100–300 fitness evaluations for a ~15-parameter problem — far fewer than a vanilla genetic algorithm.

### Genome (~15 floats)
`steer_k`, `lookahead_dist_base`, `lookahead_dist_per_ms`, `corner_speed_k`, `anticipation_scale`, `anticipation_k`, `recovery_k`, `edge_warn_m`, boid weights (sep, coh, aln, long_sep), `AI_GAP_KP`, `AI_GAP_KD`, `corner_look_m`

### Infrastructure
- [ ] Add a headless sim mode: run a full race on a fixed course with no rendering, return when all riders finish.
- [ ] Run all population members simultaneously in one sim (mute inter-rider boids during tuning to isolate individual param effects, or keep them on for pack behavior tuning).
- [ ] Score function (lower = better): `race_time + w1*track_deviation_rms + w2*collision_count + w3*steer_oscillation`. Start with just `race_time`, add penalty terms as needed.
- [ ] Expose all genome params via a struct so CMA-ES can write a candidate and the AI can read it, without touching the global constants.

### CMA-ES Implementation
- [ ] Implement or import a minimal CMA-ES (~200 lines). Key loop: sample N candidates from current distribution → evaluate fitness for each → update mean and covariance matrix → repeat.
- [ ] Sigma (initial step size): ~10–20% of each param's expected range.
- [ ] Population size: `4 + floor(3 * ln(n_params))` ≈ 12 for 15 params (standard formula).
- [ ] Stopping criteria: fitness stagnation over 20+ generations, or max 500 generations.
- [ ] Run tuning on multiple courses simultaneously and sum scores to avoid overfitting to one track.
- [ ] Save best genome to a config file (vars.txt or a dedicated `bike_ai_params.cfg`) so it loads at runtime.

---

## Priority Order
1. **Braking anticipation** — biggest visible improvement, self-contained.
2. **Steering anticipation** — fixes corner entry lag, pairs well with braking.
3. **Track boundary avoidance** — prevents embarrassing off-track excursions.
4. **Off-line recovery** — polishes racing line behavior after boid interactions.
5. **Close-range collision avoidance** — safety net for dense pack racing.
6. **Wind behaviors** — adds tactical depth, requires wind system integration.
7. **Group position seeking** — enriches race strategy, builds on existing paceline states.
8. **Parameter tuning infrastructure** — enables all of the above to be tuned systematically.
