#pragma once
#include "Game/EntityPtr.h"
#include "Game/Entity.h"
#include "BikeCourse.h"
#include "BikeDebugger.h"
#include <array>
#include "Framework/MulticastDelegate.h"
#include "Framework/MeshBuilder.h"
#include "Sound/SoundPublic.h"
#include "Render/MaterialPublic.h"
#include "Render/DynamicModelPtr.h"

class CharacterController;
class Texture;
class MeshComponent;
class MeshBuilderComponent;

// Discrete power levels available to the player (watts)
static constexpr int BIKE_POWER_LEVELS[] = {50, 100, 150, 200, 225, 250, 275, 300, 325, 350, 400, 450, 500, 600, 800, 1000 };
static constexpr int BIKE_NUM_POWER_LEVELS = (int)(sizeof(BIKE_POWER_LEVELS) / sizeof(BIKE_POWER_LEVELS[0]));


// selects the gear ratio
class GearSelector {
public:
	viewMulticastDelegate<> get_on_fire() { return viewMulticastDelegate<>(on_fire); }
	MulticastDelegate<> on_fire;

	int current_low_gear = 0;
	int current_high_gear = 0;
	std::array<int, 2> front_cogs = { 50,34 };
	std::array<int, 11> back_cogs = { 32,28,25,23,18,16,15,14,13,12,11 };
};

class BikeWorld {
public:
	// wind system
	// weather system
	// road surface

	// manages current race track. has racing line AI can use
};

// feeds into bikecharacter
class BikeObject;
class IBikeInput {
public:
	virtual ~IBikeInput() {}
	virtual void evaluate(BikeObject* my_bike) {}
};
#include "Game/Components/CameraComponent.h"
// Forward declare so BikeAI can hold a pointer to the course.
class BikeCourse;

// ============================================================
// Camera state — per-frame state, owned by BikePlayer
// ============================================================
struct BikeCameraState {
	glm::vec3 camera_pos{};
	glm::vec3 smooth_aim_pos{};
	bool  camera_initialized = false;
	float camera_yaw         = 0.f;
	float camera_pitch       = 0.f;
	float gradient_pitch     = 0.f;
	float brake_pitch        = 0.f;
	float lead_yaw           = 0.f;
	float fov_smoothed       = 65.f;
	float camera_roll        = 0.f;
	bool  reverse_view       = false;
	float reverse_yaw        = 0.f;
	float cadence_bob_phase  = 0.f;
	// Road bump springs
	float bump_pitch_disp    = 0.f;
	float bump_pitch_vel     = 0.f;
	float bump_vert_disp     = 0.f;
	float bump_vert_vel      = 0.f;
	glm::vec3 shake_offset{};
	float shake_magnitude    = 0.f;
};

// ============================================================
// SpeedlinesFx — screen-space radial speed lines (owned by BikePlayer)
// ============================================================
class BikeSpeedlinesFx {
public:
	BikeSpeedlinesFx();
	~BikeSpeedlinesFx();
	void update(float speed, float fov_deg,
	            glm::vec3 final_pos, glm::vec3 cam_right, glm::vec3 cam_up, glm::vec3 cam_fwd,
	            float dt);

	// Tuning — exposed for debug menu via bp->speedlines
	bool  lines_enabled    = true;
	int   lines_count      = 32;
	float lines_speed_min  = 10.f;
	float lines_speed_full = 22.f;
	float lines_inner      = 0.55f;
	float lines_outer      = 1.0f;
	float lines_max_len    = 0.22f;
	float lines_width      = 0.004f;
	float lines_phase_rate = 1.4f;
	float lines_alpha_peak = 200.f;

private:
	static constexpr int MAX_SPEEDLINES = 48;
	float sl_phases[MAX_SPEEDLINES] = {};
	float sl_angles[MAX_SPEEDLINES] = {};
	bool  sl_initialized = false;
	MeshBuilder speedlines_mb{};
	handle<Particle_Object> speedlines_handle{};
};

// ============================================================
// WindSystem — world wind state + visual streak FX
// ============================================================
class BikeObject;
class WindSystem {
public:
	void init();      // register particle obj — call from BikePlayer ctor
	void shutdown();  // remove particle obj  — call from BikePlayer dtor
	void update(BikeObject* bike, glm::vec3 camera_pos);

	// World state (read by BikeCamera, BikeApplication, BikeObject)
	glm::vec3 wind_direction    = glm::vec3(1.f, 0.f, 0.f);
	float     wind_speed        = 4.f;
	float     wind_gust_factor  = 0.f;
	float     wind_elapsed_time = 0.f;
	float     ambient_temp      = 35.f;
	float     sun_exposure      = 0.9f;
	float     gust_speed_amp    = 1.5f;

	struct WindLine {
		glm::vec3 pos;
		float lifetime, max_life, wave_phase, wave_speed;
		float radius, len, width, alpha;
	};
	static constexpr int WIND_LINE_COUNT = 20;

private:
	WindLine    wind_lines[WIND_LINE_COUNT] = {};
	bool        wind_initialized = false;
	MeshBuilder wind_mb{};
	handle<Particle_Object> wind_handle{};
};

extern WindSystem g_wind;

// ============================================================
// BikeAIParams — global tuning knobs shared by all AI riders.
// Edit via the debug menu; never loop through riders to set these.
//
// ONE layer: speed PID (power) + two pack-behavior terms — cohesion (draft)
// and avoidance, layered on the racing line's own offset — -> a target
// lateral position -> lateral_shift, which BikeObject::tick_transform turns
// into a heading correction (worldspace-authoritative steering, not a rail
// translation). See [[bike/bikeai]].
// ============================================================
struct BikeAIParams {
	// ---- Corner braking (lookahead safety scan, not a magnetism term) ----
	float corner_look_m  = 20.f;
	float corner_speed_k = 1.4f;

	// ---- Speed/power PID (drives ci.power toward a target speed) ----
	float speed_kp      = 60.f;   // W per (m/s) of speed error
	float speed_ki      = 5.f;
	float speed_kd      = 10.f;
	float base_power_w  = 250.f;  // constant cruise power when no neighbors sensed
	float min_power_w   = 50.f;
	float max_power_w   = 1000.f;

	// ---- Hemisphere neighbor sense ----
	float sense_radius_m      = 15.f;  // m — ignore riders beyond this
	float sense_half_angle_deg = 100.f; // deg — forward cone half-angle

	// ---- Cohesion (draft): pulls a rider in behind whoever is sensed ahead,
	// and bunches the whole sensed group closer together laterally. Two
	// independent sub-terms — see BikeAI.cpp section 2. Smooth/gradual by
	// design: real feedback lives in the shared speed PID and lateral_shift_kp
	// below, not a fast override — that's the avoidance term's job. ----
	bool  enable_cohesion         = true;   // if false, this whole term is skipped (avoidance is independent, see below)
	// "Behind": lateral pull into the nearest ahead-neighbor's wheel track
	// (drive their lat_gap toward 0) plus a following-gap speed match, once
	// within cohesion_follow_dist_m longitudinally. This is the "draft" part.
	float cohesion_behind_k       = 0.9f;   // lateral pull per metre of the leader's lat_gap
	float cohesion_follow_dist_m  = 3.0f;   // m — only pulls/speed-matches within this longitudinal range ahead
	float cohesion_gap_m          = 3.0f;   // m — target following gap once locked on (speed PID setpoint)
	float cohesion_gap_kp         = 0.9f;   // (m/s) speed correction per metre of gap error
	// "Closer": always-on lateral magnetism toward the lateral centroid of ALL
	// sensed neighbors (ahead or behind) — keeps the group from spreading out
	// sideways even when nobody is close enough ahead to draft off of.
	float cohesion_closer_k       = 0.3f;   // pull toward neighbor lateral centroid, per metre of offset

	// ---- Avoidance: decentralized — every rider independently detects an
	// imminent overlap with ANY sensed neighbor (not just the one ahead) and
	// yields. NOT symmetric by default: the TRAILING rider (the one catching
	// up from behind) is the one who yields, matching a real pack — the rider
	// out front has no reason to react to someone approaching from behind them.
	// Only once the gap is small enough to call it side-by-side
	// (avoidance_side_by_side_m below) rather than a clean following
	// situation do BOTH riders yield, since neither one is unambiguously "in
	// front." Both riders in a conflict run this same role logic
	// independently — no central coordinator, same hemisphere-scan/
	// no-array-index rule as cohesion. Computed and applied entirely in
	// WORLDSPACE, never course-space (course_dist_m/lateral_pos):
	// each rider projects a neighbor's offset onto its OWN bike_direction/right
	// (a real 3D box-overlap test, not a road-tangent-relative one, so it stays
	// correct through corners where the two frames diverge). Also touches
	// LATERAL POSITION directly (ControlInput::avoidance_lateral_vel, a
	// worldspace velocity vector), never heading/bike_direction — routing it
	// through the heading PID's momentum/accel-cap chain (like cohesion's
	// lateral_shift_kp path does) meant the turn kept building past what was
	// needed and then had to unwind the same way once clear, producing a
	// spring/overshoot "yoyo" against cohesion pulling back in. A direct
	// velocity, proportional only to this tick's conflict severity, has no
	// memory to unwind. See BikeAI.cpp section 3 / BikeObject::tick_transform. ----
	bool  enable_avoidance        = true;   // independent of enable_cohesion — can run with cohesion off
	// Drop-dead box half-extents, matched to the rider prefab's own bounding
	// box (prefab space: Z front/back, X left/right) — this is the box no
	// other rider's own box should ever actually overlap, not an arbitrary
	// tuning value. Prefab box is Z:[-0.9,0.9] X:[-0.25,0.25]. A conflict is a
	// box-overlap test (Minkowski sum): since every rider shares this box, two
	// riders' boxes touch when the worldspace gap on an axis drops below TWICE
	// the half-extent — see BikeAI.cpp section 3.
	float avoidance_box_half_long_m   = 0.65f;   // half-extent, Z (front/back)
	float avoidance_box_half_lat_m    = 0.25f;  // half-extent, X (left/right)
	// Soft reaction zone: severity ramps 0..1 linearly across this extra
	// distance OUTSIDE the hard box-overlap boundary (severity=1 once boxes
	// actually touch), so the response builds in smoothly instead of snapping
	// to full force the instant the hard boundary is crossed.
	float avoidance_soft_margin_long_m = 0.7f;
	float avoidance_soft_margin_lat_m  = 0.5f;
	// Below this worldspace longitudinal gap (full distance, not a half-extent),
	// neither rider reads as clearly ahead/behind — treat it as side-by-side
	// and have BOTH yield. Above it, only the trailing rider (conflicting
	// neighbor is ahead of them) responds.
	float avoidance_side_by_side_m     = 0.6f;
	float avoidance_lateral_speed_mps  = 3.5f;  // direct worldspace lateral slide speed at severity=1 (m/s)
	float avoidance_brake_k            = 0.5f;  // brake_amount at severity=1, applied directly (bypasses speed PID)

	// ---- Steer target lookahead — where along the course the target lateral
	// offset is sampled from (pure-pursuit style preview, not the bike's own
	// current position). effective_m = max(steer_lookahead_m, speed * steer_lookahead_time_s).
	//
	// Must anticipate corners on roughly the same horizon as the braking scan
	// below (corner_look_m, ~20m+), not a much shorter one — a short lookahead
	// here brakes for the corner in good time but doesn't start turning-in
	// until nearly on top of it, so the target has already moved deep into the
	// curve by the time steering reacts: reads as turning in late and cutting
	// across toward the apex instead of tracking it smoothly.
	float steer_lookahead_m      = 2.f;   // floor distance (m), keeps a preview even near-stationary
	float steer_lookahead_time_s = 0.0f;  // scales lookahead with speed above the floor

	// ---- Lateral guidance — converts target lateral offset into ci.lateral_shift ----
	// Deliberately simple proportional-only: this just sets (part of) the
	// setpoint for BikeObject's own heading PID (bike_heading_gains in
	// BikeObject_Local.h), which is where the actual feedback control (and
	// its damping) lives now — two stacked PIDs here was one tunable loop too
	// many. Command is clamped to [-1,1] independently of heading_shift_kp's
	// term below (see ci.heading_shift); BikeObject::tick_transform sums both
	// terms' angles and maps the total onto a heading offset
	// (bike_heading_max_offset_deg) from the track tangent.
	float lateral_shift_kp = 0.35f;  // shift command (pre-clamp) per metre of offset error

	// ---- Heading guidance — lateral_shift_kp alone only ever points the bike
	// at the ROAD's own tangent (wp.forward), offset by a lateral-error angle;
	// it never accounts for the racing line's own heading, which diverges from
	// the road tangent through a corner (that's the whole point of an apex
	// line — it cuts across the road at an angle). Without this, the bike
	// tracks lateral position correctly but can still enter/exit a corner
	// pointing the wrong way relative to the line. heading_shift_kp adds a
	// term proportional to the angle between bike_direction and the racing
	// line's own tangent (finite-differenced from racing_line_pos near the
	// lookahead point), gated by the same corner-detection blend as the
	// manual offset above but inverted — negligible on a straight (where the
	// road tangent and racing line tangent are ~identical anyway), full
	// strength mid-corner. ----
	float heading_shift_kp = 3.8f;  // shift command (pre-clamp) per radian of heading error, at full corner weight

	// ---- Manual lateral offset blend — how much of BikeObject::manual_lateral_offset
	// (debug-set per rider, see BikeDebugger) actually reaches the steering
	// target. Full weight on straights; blended out toward the racing line as
	// upcoming curvature tightens (reuses the same min_turn_radius_ahead scan as
	// corner braking, so it anticipates on the same horizon) — an offset that
	// made sense on the preceding straight would put the bike on a nonsense
	// line through the corner itself. ----
	float offset_straight_r_m = 20.f;  // min_r above this: full manual offset
	float offset_corner_r_m   = 6.f;  // min_r at/below this: zero manual offset (racing line only)
	float offset_blend_tau_s  = 0.05f;  // low-pass tau smoothing the blend transition

	// ---- Off-track hard clamp ----
	float edge_safety_m = 0.8f;  // margin inside road edge the cohesion offset may never cross

	// ---- "Move to front" debug behavior (BikeObject::ai_behavior_state) —
	// bypasses the speed PID entirely and just commands this much power while
	// active; see BikeAI::evaluate section 5. ----
	float move_to_front_power_w = 500.f;

	// ---- "Stay at front" debug behavior, once at the front (suppress_front_draft
	// in BikeAI::evaluate section 2) — a continuous, proportional lateral
	// separation between riders holding the front together, DISTINCT from
	// avoidance's binary box-overlap trigger: avoidance's own hard/soft zone is
	// deliberately much narrower than the spacing below (its trigger_lat_m is
	// ~2*avoidance_box_half_lat_m + avoidance_soft_margin_lat_m, well under 1m
	// by default), so as long as this term holds riders apart at spacing_m,
	// avoidance should stay dormant between them — no fighting between two
	// separate "push apart" forces with different equilibria. ----
	float front_abreast_spacing_m     = 1.4f;  // m — desired lateral gap between riders holding the front together
	float front_abreast_separation_k  = 0.8f;  // lateral push per metre of encroachment inside spacing_m

	// Pace target for the same behavior — see BikeAI::evaluate section 2/5.
	// Same shape as cohesion_gap_kp/cohesion_gap_m (draft follow-gap match)
	// but on POSITION along the course rather than a following gap, and
	// symmetric (applies against every other StayingAtFront groupmate, not
	// just whoever's "ahead" of me): being even slightly ahead of a groupmate
	// lowers my target speed below theirs, and vice versa, so matching raw
	// speed alone is never enough to stop settling into an even line —
	// existing position error keeps correcting until the gap itself is zero.
	float front_abreast_gap_kp        = 0.4f;   // (m/s) target-speed correction per metre I'm ahead of a groupmate
	float front_abreast_gap_cap_ms    = 4.0f;   // clamp on the above, per neighbor

	// How close (course_dist_m) a StayingAtFront rider has to get to the
	// group's actual rank-0 leader before it's considered "at the front" and
	// switches from sprinting to pace-holding. Deliberately NOT pos_in_group_norm
	// itself (a RANK divided by group size — rank 1 of 5 riders is already
	// 0.25, however physically close it actually is to the leader): using rank
	// directly here meant a trailing StayingAtFront rider could never register
	// as "at front" in a group bigger than 2-3, so it kept sprinting at full
	// power indefinitely, overtook, and then the rider it just passed started
	// sprinting in turn — a permanent back-and-forth. See BikeAI::evaluate
	// section 2's near_group_front lambda.
	float front_abreast_join_dist_m   = 3.0f;
};
extern BikeAIParams g_ai_params;

// Per-neighbor breakdown of this tick's cohesion/avoidance, rebuilt fresh
// every BikeAI::evaluate() call (same lifetime as the hemisphere scan itself
// — riders drift in/out of sense range tick to tick). Debug-only: lets
// BikeDebugger draw exactly which sensed riders contributed what, and how
// much, rather than just the summed totals in BikeAI::dbg_*.
struct BikeAIDebugNeighbor {
	BikeObject* rider                     = nullptr;
	float       dist                      = 0.f;
	float       long_gap                  = 0.f;  // +ve = ahead of me
	float       lat_gap                   = 0.f;  // +ve = road-right of me
	bool        is_cohesion_behind_leader = false;  // nearest-ahead neighbor cohesion is pulling behind / speed-matching
	bool        is_cohesion_closer_member = false;  // included in this tick's "closer" lateral centroid average
	bool        is_avoidance_conflict     = false;  // this neighbor is within the avoidance trigger zone (may or may not be MY responsibility to react — see below)
	bool        is_avoidance_responsible  = false;  // valid iff is_avoidance_conflict: true if I'M the one who should yield (trailing rider, or side-by-side — see BikeAIParams::avoidance_side_by_side_m). False = they're ahead of me by a clear margin, so THEY yield, not me.
	float       avoidance_severity        = 0.f;
};

class BikeAI : public IBikeInput {
public:
	void evaluate(BikeObject* my_bike) final;

	// Set once after construction — points to the application-owned course.
	BikeCourse* course = nullptr;

	// Braking scan constants
	static constexpr int   BRAKE_SCAN_STEPS  = 8;
	static constexpr float BRAKE_SCAN_STEP_M = 10.f;

	// ---- PID controller state (speed only — lateral guidance is proportional-only) ----
	float speed_integral    = 0.f;
	float speed_prev_error  = 0.f;

	// Separate PID state for the "stay at front" abreast speed-match (targets
	// the average speed of other front-abreast riders — see BikeAI::evaluate
	// section 5). Kept independent of speed_integral/speed_prev_error above so
	// switching between "drafting behind a leader" (cohesion's follow-gap
	// match) and "holding pace abreast at the front" — different speed
	// regimes — never carries stale integral windup from one into the other.
	float front_abreast_speed_integral   = 0.f;
	float front_abreast_speed_prev_error = 0.f;

	// Low-passed weight (0..1) of BikeObject::manual_lateral_offset actually
	// applied this tick — 1 on a straight, blended toward 0 as upcoming
	// curvature tightens. See BikeAIParams::offset_straight_r_m/offset_corner_r_m.
	float offset_blend = 1.f;

	// ---- Debug ----
	glm::vec3 dbg_lookahead_pt{};
	float dbg_steer_final        = 0.f;
	float dbg_power_final        = 0.f;
	float dbg_brake_amount       = 0.f;
	float dbg_brake_dist_m       = 0.f;
	float dbg_brake_corner_r     = 0.f;
	float dbg_min_r              = 0.f;
	float dbg_v_max              = 0.f;
	float dbg_target_speed       = 0.f;
	int   dbg_num_neighbors      = 0;
	float dbg_cohesion_behind_lat = 0.f;  // lateral term from the "behind" sub-term (m)
	float dbg_cohesion_closer_lat = 0.f;  // lateral term from the "closer" sub-term (m)
	float dbg_cohesion_gap_m      = 0.f;  // actual longitudinal gap to the behind-leader, valid iff dbg_cohesion_behind_lat's leader exists
	float dbg_target_lat_offset  = 0.f;
	bool  dbg_clamped            = false;
	float dbg_lateral_shift      = 0.f;  // ci.lateral_shift this tick — position-error term only
	float dbg_heading_shift      = 0.f;  // ci.heading_shift this tick — heading-error term only, see dbg_heading_error
	float dbg_heading_error      = 0.f;  // rad, signed angle from bike_direction to the racing line's own tangent
	bool  dbg_avoidance_active   = false;
	float dbg_avoidance_lateral_vel = 0.f;  // signed speed along my own right vector (m/s) — dot(ci.avoidance_lateral_vel, my_right)
	float dbg_avoidance_brake    = 0.f;  // extra brake_amount from an unavoidable conflict (0..1)
	glm::vec3 dbg_cohesion_centroid_pt{};  // world-space point at the "closer" centroid lateral offset, valid iff dbg_cohesion_closer_lat != 0
	std::vector<BikeAIDebugNeighbor> dbg_neighbors;  // rebuilt every evaluate() — see BikeAIDebugNeighbor
};

class BikePlayer : public IBikeInput {
public:
	BikePlayer();
	~BikePlayer();
	void evaluate(BikeObject* my_bike) final;
	void update_camera(BikeObject* bike, float steer, float brake_amount);
	void draw_power_meter(float current_watts, int power_idx, bool coasting, bool speed_hold, float speed_hold_watts, float actual_watts, float power_ceiling);

	CameraComponent* cc = nullptr;
	BikeCameraState  cam;
	BikeSpeedlinesFx speedlines;

	float current_power      = 0.f;
	int   power_level_idx    = 4;
	bool  is_coasting        = false;
	float power_hold_timer   = 0.f;
	float power_repeat_timer = 0.f;

	bool  speed_hold_active = false;
	float speed_hold_target = 0.f;
	float speed_hold_power  = 0.f;

	SoundPlayer* freewheel_player = nullptr;
	SoundPlayer* wind_player      = nullptr;
	SoundPlayer* pedal_player     = nullptr;

	float dbg_steer_final         = 0.f;
};
class AnimatorObject;
class BikeAnimDriver {
public:
	// handles animation
	AnimatorObject* ao = nullptr;
};

// Debug goal states (BikeObject::ai_behavior_state), distinct from the
// persistent per-rider toggles above (ride_2nd_wheel_enabled, ai_override_*).
// MovingToFront is one-shot ("do X until done" — BikeAI::evaluate flips it
// back to Default on its own once reached). StayingAtFront is persistent
// ("always do X" — never auto-cancels): sprints to the front exactly like
// MovingToFront while behind it, then holds there once reached, WITHOUT
// re-cancelling to Default and WITHOUT drafting behind whoever else is at
// the front — see BikeAI::evaluate sections 2/5 — so several StayingAtFront
// riders naturally settle side-by-side instead of queuing single-file.
enum class BikeAIBehaviorState : uint8_t {
	Default,
	MovingToFront,   // sprint (BikeAIParams::move_to_front_power_w) until this rider leads its own group, then auto-reverts to Default
	StayingAtFront,  // sprint to the front like above, then HOLD there indefinitely — no cohesion "behind" draft-lock once at the front, so multiple riders can sit abreast
};

// physics of the bike. handles:
// slope gradient
// pedal strikes
// sliding out, rain
// wind
class BikeObject : public Component {
public:
	CLASS_BODY(BikeObject);

	BikeAnimDriver anim;
	std::unique_ptr<IBikeInput> input;

	// Set once at spawn (BikeGameApplication::create_player/create_ai) — the
	// course tick_transform's rail movement samples from. Not read via
	// g_bike_app, since that global isn't set until BikeGameApplication::update()
	// runs, which is after entity ticks on the frame a rider is spawned.
	BikeCourse* course = nullptr;

	void start() final;
	void update() final;

	// input:
	struct ControlInput {
		float aero_coeff = 0.0;	// determied by stance
		float steer = 0.0;// l,r — cosmetic only (fork angle / lean); never rotates bike_direction, see tick_steer
		float lateral_shift = 0.0; // -1..1 — offset from the road's own tangent, driven by lateral POSITION error (rate-capped turn, see BikeObject::tick_transform)
		float heading_shift = 0.0; // -1..1 — a second, independent offset from the road's own tangent, driven directly by HEADING error (e.g. matching the racing line's own tangent through a corner). Summed with lateral_shift's angle rather than blended/clamped together, so neither term dilutes the other.
		float brake_amount = 0.0;// 0,1
		float power = 0.0;	// input _watts_ requested
		glm::vec3 avoidance_lateral_vel = glm::vec3(0.f); // m/s, worldspace vector (already includes direction, e.g. along the yielding bike's own right vector) — a direct positional slide, deliberately NOT routed through bike_direction/heading (see BikeObject::tick_transform). Set by BikeAI's avoidance term only; BikePlayer never sets it.


		bool is_coasting() const { return power == 0.0; }
	};
	ControlInput update_tick(ControlInput input);

	// Sub-tick functions (called in order by update_tick)
	void tick_physics(ControlInput& ci, float dt);
	void tick_steer(const ControlInput& ci, float dt);
	void tick_gears(float dt);
	void tick_transform(const ControlInput& ci, float dt);
	float get_wind_along_bike() const;
	glm::vec3 get_wind_along_bike_vector() const;


	// Cross-tick communication (written by one sub-tick, read by another)
	float     steer_input_raw      = 0.f;       // resolved raw stick input
	glm::vec3 terrain_forward_dir  = {0,0,1};   // terrain-aligned forward from last raycast

	glm::vec3 bike_direction = glm::vec3(0.f, 0, 1.f);  // actual steered heading (worldspace-authoritative) — used for sensing/wind/probe placement, never smoothed
	float heading_turn_rate  = 0.f;  // rad/s, persists tick to tick — the bike's actual angular momentum while steering (see BikeObject::tick_transform)
	float heading_error_integral = 0.f;  // rad*s, accumulated heading-PID error (see bike_heading_gains, BikeObject::tick_transform)
	glm::vec3 visual_heading = glm::vec3(0.f, 0, 1.f);  // low-passed toward actual velocity direction (forward + lateral); drives render orientation only, see tick_transform
	float speed = 0.f;
	float speed_smoothed = 0.f; // low-pass filtered speed, used for gear cadence checks
	float cadence = 0.f;	// cadence at gear
	float current_roll = 0.0;
	float current_steer    = 0.f;  // low-pass-smoothed steer
	float terrain_gradient    = 0.f;   // radians, + = uphill, - = downhill
	float prev_gradient       = 0.f;   // last frame gradient, for bump detection
	float bump_impulse        = 0.f;   // magnitude of bump this frame (speed-scaled) — consumed by BikeCamera shake
	float crack_impulse       = 0.f;   // set by app when bike crosses a crack decal — consumed by BikeCamera shake
	float crack_cooldown      = 0.f;   // seconds until crack can retrigger

	float gear_shift_cooldown = 0.f;   // seconds remaining until next shift is allowed
	bool  just_shifted        = false; // set true for one tick when a shift occurs
	GearSelector gear;

	float surface_traction = 1.0f;  // [0,1] — road grip: 1=dry tarmac, 0.6=wet, 0.3=gravel; scales max braking decel and corner speed limit

	// Course state — DERIVED each tick from the authoritative worldspace position
	// (course->project, in BikeObject::tick_transform). Used for AI targeting,
	// braking lookahead, and curvature/lean; never fed back to move position.
	float course_dist_m  = 0.f;   // arc-length from course start (m)
	float lateral_pos    = 0.f;   // signed offset from road centre, +ve = road-right (m)
	int   course_segment = 0;     // nearest waypoint segment index (cached)
	int   race_position  = 0;     // 1-indexed finishing position in sorted rider list

	// Debug-set per-rider bias (BikeDebugger's Selected Rider panel), signed
	// offset from road centre same as lateral_pos, +ve = road-right. AI-only:
	// blended into the steering target on straights, blended out toward the
	// racing line through corners (see BikeAI::evaluate / BikeAIParams).
	float manual_lateral_offset = 0.f;

	// Debug-only per-rider AI overrides (BikeDebugger's Selected Rider panel).
	// Unlike manual_lateral_offset (a bias that still blends/fades through
	// corners) these are hard overrides — force one AI's behavior directly so
	// you can watch how OTHER AI react to it (draft/separation/avoidance all
	// key off real sensed lateral_pos/speed, so an overridden rider is a
	// legitimate stimulus for the rest of the pack), without touching global
	// BikeAIParams which would affect every rider at once. AI-only; no effect
	// on a player-controlled bike. See BikeAI::evaluate.
	bool  ai_override_speed_enabled   = false;
	float ai_override_target_speed_ms = 8.f;
	bool  ai_override_lateral_enabled = false;
	float ai_override_lateral_pos_m   = 0.f;  // hard target lateral offset — bypasses racing line/magnetism/draft entirely

	// Debug-only per-rider behavior mode (BikeDebugger's Selected Rider panel),
	// AI-only. Unlike the hard overrides above, this doesn't pin a fixed value —
	// it changes WHICH rider cohesion's "behind" sub-term targets: instead of
	// whichever hemisphere-sensed neighbor happens to be nearest ahead, it locks
	// onto the current LEADER of this rider's own group (group_id/
	// pos_in_group_norm below, real position not array index), and targets them
	// regardless of distance/hemisphere — so this rider chases back onto the
	// leader's wheel even from well outside normal draft range. See
	// BikeAI::evaluate section 2.
	bool  ride_2nd_wheel_enabled = false;

	// Debug-only per-rider goal state (BikeDebugger's Selected Rider panel),
	// AI-only — see BikeAIBehaviorState for MovingToFront (one-shot) vs.
	// StayingAtFront (persistent) semantics. Toggling the corresponding debug
	// button again while active cancels it back to Default early. Only
	// power/speed (and, for StayingAtFront, cohesion's draft targeting) change
	// — steering/lateral guidance and avoidance are untouched, so the AI still
	// holds its normal racing line and won't physically overlap another rider.
	// See BikeAI::evaluate sections 2/5.
	BikeAIBehaviorState ai_behavior_state = BikeAIBehaviorState::Default;

	// Group context (written by BikeGameApplication::update_groups each frame)
	int   group_id           = 0;
	float pos_in_group_norm  = 0.f;  // 0=front of group, 1=back
	float group_rank_norm    = 0.f;  // 0=leading group, 1=last group
	float group_size_norm    = 0.f;  // group_size / total_riders

	// Drafting (written by BikeGameApplication::update_drafting before physics runs)
	// 1.0 = no draft (open air), 0.65 = full draft at ideal position
	float draft_factor = 1.0f;

	// Lateral velocity — written each tick by BikeObject::tick_transform
	float lateral_vel = 0.f;  // m/s, positive = moving road-right

	// Steering debug — written each tick by BikeObject::tick_transform. Applies
	// to player and AI alike (it's the physical steering model, not AI-specific).
	float dbg_steer_cmd               = 0.f;  // ci.lateral_shift this tick, [-1,1]
	float dbg_desired_heading_offset_deg = 0.f;  // commanded heading offset from track tangent, pre rate-cap
	float dbg_heading_offset_deg      = 0.f;  // actual signed angle between bike_direction and track tangent, +ve = right
	float dbg_turn_rate_dps           = 0.f;  // actual heading turn rate applied this tick (deg/s)

	EntityPtr fork_entity;
	float fork_flick_smoothed = 0.f;  // lightly-smoothed lateral_vel driving the fork "flick" (tick_transform)

	// Pedal visual (crank + shoe pivots) — see BikeObject::tick_transform.
	// Rest rotations are captured once in start() so the animation composes
	// on top of whatever the artist authored, rather than overwriting it.
	EntityPtr crank_entity;
	EntityPtr right_shoe_entity;
	EntityPtr left_shoe_entity;
	glm::quat crank_rest_rot{ 1.f, 0.f, 0.f, 0.f };
	glm::quat right_shoe_rest_rot{ 1.f, 0.f, 0.f, 0.f };
	glm::quat left_shoe_rest_rot{ 1.f, 0.f, 0.f, 0.f };
	float right_shoe_phase_offset = 0.f;  // this shoe pivot's baseline angle around the crank circle, from its authored local position (start())
	float left_shoe_phase_offset  = 0.f;
	float crank_phase = 0.f;  // rad, advances with cadence; frozen while coasting so freewheeling doesn't spin the legs

	// Head-look (corner anticipation) — see BikeObject::tick_transform.
	EntityPtr head_entity;
	glm::quat head_rest_rot{ 1.f, 0.f, 0.f, 0.f };
	float head_look_smoothed = 0.f;  // rad, damped toward the target yaw each tick

	// Jersey color material — owns the dynamic material applied to the prefab's
	// "rider_body" mesh in start(); must outlive that mesh's material_override.
	DynamicMatUniquePtr jersey_mat;

	glm::vec3 prev_front_wheel_pos{};
	glm::vec3 prev_rear_wheel_pos{};
	bool wheel_history_initialized = false;
};


#include "Framework/Hashset.h"
class BikeGameApplication : public Application {
public:
	CLASS_BODY(BikeGameApplication);

	~BikeGameApplication() override;

	void start() final;
	void update() final;
	void on_imgui() final;

	BikeObject* create_player(glm::vec3 pos);
	BikeObject* create_ai(glm::vec3 pos);

	BikeCourse    course;
	BikeDebugger  debugger;

	// Which code-generated circuit rebuild_course()/start() build. Set from the
	// debug menu's course dropdown, applied on the next rebuild_course() call.
	BikeHardcodedCourseKind course_variant = BikeHardcodedCourseKind::ClassicLoop;

	// Build/refresh the visible road mesh (a flat ribbon strip along course.waypoints,
	// road_half_width wide) and display it via a MeshComponent on a dedicated entity.
	// Call after any full course rebuild (build_hardcoded_circuit/build_from_spawners).
	void build_road_mesh();

	// Builds/refreshes the procedural grass terrain mesh for the Hilly course
	// (a terrain_size_m x terrain_size_m grid sampled from bike_hilly_height,
	// groundgrass_01 material). Hides/frees it (set_model(nullptr) + reset)
	// when course_variant isn't Hilly -- call after every rebuild_course().
	void build_terrain_mesh();

	// Show/hide the ideal racing line as a MeshBuilder line strip (orange), rebuilt
	// from course.waypoints[*].racing_line_pos. Bound to BikeDebugger's checkbox.
	void set_draw_racing_line(bool show);
	bool draw_racing_line_debug = false;

	// All riders (player + AI), populated by create_player / create_ai
	std::vector<BikeObject*> all_riders;
	// Sorted front-to-back by course_dist_m each frame (index 0 = race leader)
	std::vector<BikeObject*> riders_sorted;


	int  num_ai                 = 5;

	// Crack decal instances collected at map load
	struct CrackDecalInstance {
		glm::vec3 pos;
		float     trigger_radius;  // derived from decal WS scale * type radius_mult
		int       type_idx;
	};
	std::vector<CrackDecalInstance> crack_instances;

	void rebuild_course();  // re-runs course build with current fillet params (call from debug menu)
	void respawn_ai();      // destroy existing AI riders and re-spawn num_ai of them

	// Destroys every rider (player included) and re-spawns them at the current
	// course's start line, same layout as start(). Called by rebuild_course()
	// so switching course type (e.g. into/out of Hilly) never leaves riders
	// stranded mid-air over a track that no longer exists under them, or still
	// projected onto stale course_dist_m/lateral_pos from the old course.
	void respawn_all_riders();

private:
	void collect_crack_decals();
	void sort_riders();
	void update_groups();
	void update_drafting();
	void update_crack_triggers();
	void debug_draw_course() const;

	DynamicModelUniquePtr road_mesh;
	Entity*        road_mesh_entity    = nullptr;
	MeshComponent* road_mesh_component = nullptr;

	DynamicModelUniquePtr terrain_mesh;
	Entity*        terrain_mesh_entity    = nullptr;
	MeshComponent* terrain_mesh_component = nullptr;

	Entity*               racing_line_entity = nullptr;
	MeshBuilderComponent* racing_line_mb     = nullptr;
};

// Shared per-rider stats text + gizmo overlay — defined in BikeApplication_Debug.cpp.
// Used by the index-based follow camera and BikeDebugger's click-to-select orbit camera.
void draw_rider_debug_info(BikeObject* bo);

// Opt-in avoidance diagnostic overlay (drop-dead box, optionally the outer
// soft-reaction box too, + vectors to yielded-to neighbors + the actual
// commanded slide) — defined in BikeApplication_Debug.cpp.
// See BikeDebugger::draw_avoidance_box / draw_avoidance_soft_box.
void draw_rider_avoidance_box(BikeObject* bo, bool draw_soft_box);
