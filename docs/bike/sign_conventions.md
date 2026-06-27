# Sign Convention

(DO NOT get these wrong)

**World axes:** +Y up, +Z forward (default `bike_direction`), +X world-right.

**`bike_right = cross(bike_direction, up)`** points LEFT in world space (+Z bike → cross(+Z,+Y) = -X). Misnamed. Do not treat as "right".

**Steer sign:** positive steer = turn LEFT. Consistent with `bike_right` pointing left — racing-line follower steers toward `bike_right` and it works.

**`lateral_pos`:** signed metres from road centre. +ve = road-right (+X for a +Z road). `BikeWaypoint::right` = actual world right (+X for +Z road).

**Corrective steer to return to road centre:**
- `lateral_pos > 0` (road-right) → need to turn left → **positive** steer = `+sign(lateral_pos)`.
- Never `-sign(lateral_pos)` — turns the wrong way.



