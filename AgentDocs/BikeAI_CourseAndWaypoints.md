# Bike AI — Course & Waypoint System Design

## Purpose

This document covers the foundational layer of the bike AI: the course spline, waypoint nodes, and the `course_dist_m` scalar. Everything else in the AI stack (pack dynamics, drafting, lateral positioning, strategy) derives from this layer.

---

## Core Concept: Course Distance Scalar

Every rider — player and AI — has a single float: `course_dist_m`. This is their signed arc-length distance along the course spline from the start line.

This scalar is the race's source of truth:
- **Position in the race**: who is ahead, who is behind, and by how many meters
- **Gap calculation**: `gap_m = leader.course_dist_m - follower.course_dist_m`
- **Neighbor lookups**: sort all riders by `course_dist_m`, then use index arithmetic instead of 3D spatial queries
- **Stage completion**: `progress = course_dist_m / total_course_length_m`
- **Strategic inputs**: "am I in the final 5km?", "is there a climb in the next 500m?"

All per-frame AI decisions are cheaper and more correct when expressed in terms of course distance rather than 3D world-space distance.

---

## Waypoint Node

The course is defined as an ordered array of `BikeWaypoint` nodes. The spline is a Catmull-Rom (or piecewise linear fallback) through these nodes.

```cpp
struct BikeWaypoint {
    glm::vec3 position;          // world-space centre of the road at this node
    glm::vec3 forward;           // precomputed tangent direction (normalized)
    glm::vec3 right;             // precomputed road-right direction (normalized, in road plane)
    float     road_half_width;   // half-width of usable road at this node (m)
    float     dist_from_start;   // precomputed arc-length from start to this node (m)
    float     gradient;          // road gradient in radians (+ve = uphill)
    float     recommended_speed; // advisory speed through this section (m/s), 0 = no limit
};
```

`forward`, `right`, `dist_from_start`, and `gradient` are all **precomputed at load time** — never recomputed per frame.

---

## Course Spline

```cpp
class BikeCourse {
public:
    std::vector<BikeWaypoint> waypoints;
    float total_length_m = 0.f;

    // Build from raw positions (call once at load time)
    void build(std::span<glm::vec3> positions, std::span<float> road_half_widths);

    // Project a world-space position onto the spline.
    // Returns the arc-length (course_dist_m) of the closest point.
    // out_lateral: signed lateral offset from road centre (+ve = road-right)
    // out_segment: index of the nearest waypoint segment
    float project(glm::vec3 world_pos, float* out_lateral = nullptr, int* out_segment = nullptr) const;

    // Sample the spline at a given arc-length.
    // Returns interpolated BikeWaypoint (position, forward, right, gradient).
    BikeWaypoint sample(float dist_m) const;

    // Return the lookahead point on the spline, dist_m ahead of a given arc-length.
    glm::vec3 lookahead(float from_dist_m, float ahead_m) const;

    // Nearest waypoint index for a given arc-length.
    int nearest_waypoint(float dist_m) const;
};
```

### build()

Iterates all waypoint positions, computes segment lengths, accumulates `dist_from_start`, computes `total_length_m`. Computes `forward` as the normalised direction to the next node. `right` = cross(forward, world_up), then projected into the road plane.

### project()

Binary search on `dist_from_start` to find the nearest segment, then project the world position onto that segment line. Returns arc-length. Used once per rider per frame to update `course_dist_m`.

This is O(log N) where N = waypoint count. For a typical stage with one waypoint every 10m over 20km, N ≈ 2000. Binary search over 2000 elements is negligible.

### sample()

Binary search → lerp between two `BikeWaypoint` structs. Used for AI lookahead queries and drafting cone calculations.

### lookahead()

`sample(from_dist_m + ahead_m)` — just a distance-offset sample. AI uses this to steer toward a point ahead on the spline rather than directly at the next waypoint node, which eliminates zigzag oscillation.

---

## Per-Rider Course State

Added to `BikeObject`:

```cpp
// In BikeObject:
float course_dist_m  = 0.f;   // arc-length from start, updated every frame
float lateral_pos    = 0.f;   // signed offset from road centre (m), +ve = road-right
int   course_segment = 0;     // index of nearest waypoint segment (cached for perf)
```

Updated in `BikeObject::update()` (or in `BikeGameApplication::update()` before any AI runs):

```cpp
course_dist_m = g_course->project(get_ws_position(), &lateral_pos, &course_segment);
```

---

## Race State (BikeGameApplication)

The application owns the sorted rider list. It is rebuilt each frame:

```cpp
class BikeGameApplication : public Application {
public:
    BikeCourse course;

    std::vector<BikeObject*> all_riders;       // all riders, unsorted
    std::vector<BikeObject*> riders_sorted;    // sorted front-to-back by course_dist_m (descending)

    void update() final;
    BikeObject* create_player(glm::vec3 pos);
    BikeObject* create_ai(glm::vec3 pos, RiderArchetype archetype);
};
```

Each frame in `update()`:

```cpp
// 1. Update course_dist_m for all riders
for (auto* r : all_riders)
    r->course_dist_m = course.project(r->get_ws_position(), &r->lateral_pos, &r->course_segment);

// 2. Sort descending (front of race = index 0)
std::sort(riders_sorted.begin(), riders_sorted.end(),
    [](const BikeObject* a, const BikeObject* b) {
        return a->course_dist_m > b->course_dist_m;
    });

// 3. Write race position (1-indexed) into each rider
for (int i = 0; i < (int)riders_sorted.size(); ++i)
    riders_sorted[i]->race_position = i + 1;
```

Sorting 50 riders is O(50 log 50) ≈ 300 comparisons — negligible.

---

## Neighbor Lookup

Because riders are sorted by `course_dist_m`, the k riders immediately ahead of rider at index `i` are at indices `i-1, i-2, ...` and immediately behind at `i+1, i+2, ...`.

```cpp
// Get up to k riders immediately ahead of riders_sorted[my_idx]
void get_riders_ahead(int my_idx, int k, std::vector<BikeObject*>& out) {
    for (int j = my_idx - 1; j >= 0 && (my_idx - j) <= k; --j)
        out.push_back(riders_sorted[j]);
}
```

No spatial hash, no 3D distance queries. The lateral proximity check (for boids, pushing, drafting) is just:

```cpp
float lat_gap = fabsf(neighbor->lateral_pos - my->lateral_pos);
float long_gap = neighbor->course_dist_m - my->course_dist_m;
```

Both are cheap scalar comparisons.

---

## Waypoint Authoring

Waypoints are placed in the level editor as entities with a `BikeWaypointMarker` component. They are ordered by a sequential `waypoint_index` property. `BikeCourse::build()` reads them out in order at level load.

```cpp
class BikeWaypointMarker : public Component {
public:
    CLASS_BODY(BikeWaypointMarker);
    int   waypoint_index   = 0;
    float road_half_width  = 4.f;    // metres
    float recommended_speed = 0.f;  // 0 = no advisory
};
```

At load time in `BikeGameApplication::start()`:

```cpp
// Collect all BikeWaypointMarker components, sort by waypoint_index, pass to course.build()
```

This means the course is entirely data-driven from the level — no hardcoded positions.

---

## Debug Visualisation

A debug menu entry draws the course and rider state:

- **Spline**: draw line segments between waypoint positions
- **Road width**: draw two parallel lines offset by `±road_half_width` along `right`
- **Gradient colouring**: green = flat, yellow = 3–6%, red = >6%, blue = downhill
- **Rider positions**: dot on the spline at each `course_dist_m`, labeled with race position
- **Lookahead point**: for selected AI, show the 3D world point they are currently steering toward

---

## Integration Points for Later Layers

| Layer | Uses from this document |
|---|---|
| AI steering (PID) | `course.lookahead()` — steers toward a point ahead on the spline |
| Drafting | `long_gap`, `lat_gap` from sorted rider list |
| Lateral push system | `lateral_pos`, `road_half_width` from nearest waypoint |
| Strategy | `course_dist_m / total_length_m` for race phase; `course.sample()` to read upcoming gradient |
| Boid alignment | Neighbors from sorted list within ±30m `course_dist_m` |
| Debug UI | `race_position`, `course_dist_m`, gap to leader |

---

## Implementation Order

1. `BikeWaypoint` struct and `BikeCourse` class (header + stub .cpp)
2. `BikeCourse::build()` — arc-length precomputation
3. `BikeCourse::project()` — binary search projection
4. `BikeCourse::sample()` and `BikeCourse::lookahead()`
5. Add `course_dist_m`, `lateral_pos`, `race_position` to `BikeObject`
6. Integrate sort loop into `BikeGameApplication::update()`
7. `BikeWaypointMarker` component + level load integration
8. Debug visualisation
9. Place test waypoints in `bike_test_map.tmap`, verify projection is correct

---

## Non-Goals (out of scope for this layer)

- **Path smoothing / Catmull-Rom interpolation**: piecewise linear is sufficient for the first pass. Can upgrade `sample()` later without changing the interface.
- **Multi-path / junctions**: stage races are a single road. No branching.
- **Altitude-accurate gradient**: `gradient` is derived from the Y-delta between waypoints. It does not need to match terrain mesh normals exactly — the physics already does terrain raycasting separately.
