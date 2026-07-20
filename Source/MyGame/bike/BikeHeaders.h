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
// ONE layer: speed PID (power) + boids-style magnetism (cohesion/separation/
// draft/line-formation, layered on the racing line's own offset) -> a target
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

	// ---- Magnetism: desired lateral offset (m, road/wheel frame) ----
	bool  enable_magnetism         = true;  // if false, AI just tracks the racing line — no pack behavior at all
	float cohesion_k               = 0.5f;  // pull toward neighbor centroid beyond this range
	float cohesion_trigger_dist_m  = 6.f;   // only cohere if nearest neighbor farther than this
	float separation_k             = 1.2f;  // push away from neighbors closer than separation_dist_m
	float separation_dist_m        = 1.0f;  // m — full-strength push starts inside this radius
	// Draft: ADOPTS the leader's actual lateral_pos (not a proportional nudge
	// toward it) — real drafting means sitting on the wheel directly ahead,
	// sacrificing the racing line if the leader isn't on it. draft_follow_k is
	// a blend fraction (0=ignore leader's line, 1=fully lock onto it), scaled
	// by how close the gap is (glm::clamp(1 - long_gap/draft_dist_m, 0, 1)) so
	// the pull strengthens as you close in rather than snapping on at range.
	float draft_follow_k           = 0.7f;
	float draft_dist_m             = 8.0f;  // m — only draft-pull within this longitudinal range
	float lineformation_k          = 0.35f; // always-on bias toward zero lateral offset from neighbors' track

	// ---- Collision avoidance: decentralized — each rider independently
	// detects an imminent overlap with ANY sensed neighbor (not just
	// nearest_ahead) and resolves it by yielding sideways if there's room, or
	// braking to fall back if there isn't. Both riders in a conflict run this
	// same logic, so whichever has less room/more urgency ends up yielding —
	// no central coordinator, same hemisphere-scan/no-array-index rule as the
	// rest of this file. Separate from (and stacks on top of) the softer
	// always-on separation term above, which alone isn't decisive enough to
	// prevent an actual overlap when someone cuts across laterally. ----
	float collision_long_m    = 2.0f;  // longitudinal gap inside this = an active conflict
	float collision_lat_m     = 0.9f;  // lateral gap inside this = an active conflict (roughly bike width + margin)
	float avoidance_lateral_k = 3.0f;  // strong lateral push when yielding, scaled by conflict severity (0..1)
	float avoidance_brake_k   = 0.7f;  // brake_amount applied when there's no room to yield, scaled by severity

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
	float steer_lookahead_m      = 6.f;   // floor distance (m), keeps a preview even near-stationary
	float steer_lookahead_time_s = 0.9f;  // scales lookahead with speed above the floor

	// ---- Lateral guidance — converts target lateral offset into ci.lateral_shift ----
	// Deliberately simple proportional-only: this just sets the setpoint for
	// BikeObject's own heading PID (bike_heading_gains in BikeObject_Local.h),
	// which is where the actual feedback control (and its damping) lives now —
	// two stacked PIDs here was one tunable loop too many. Command is clamped
	// to [-1,1]; BikeObject::tick_transform maps it onto a heading offset
	// (bike_heading_max_offset_deg) from the track tangent.
	float lateral_shift_kp = 1.5f;  // shift command (pre-clamp) per metre of offset error

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
	float heading_shift_kp = 1.2f;  // shift command (pre-clamp) per radian of heading error, at full corner weight

	// ---- Manual lateral offset blend — how much of BikeObject::manual_lateral_offset
	// (debug-set per rider, see BikeDebugger) actually reaches the steering
	// target. Full weight on straights; blended out toward the racing line as
	// upcoming curvature tightens (reuses the same min_turn_radius_ahead scan as
	// corner braking, so it anticipates on the same horizon) — an offset that
	// made sense on the preceding straight would put the bike on a nonsense
	// line through the corner itself. ----
	float offset_straight_r_m = 50.f;  // min_r above this: full manual offset
	float offset_corner_r_m   = 15.f;  // min_r at/below this: zero manual offset (racing line only)
	float offset_blend_tau_s  = 0.3f;  // low-pass tau smoothing the blend transition

	// ---- Off-track hard clamp ----
	float edge_safety_m = 0.8f;  // margin inside road edge the magnetism offset may never cross
};
extern BikeAIParams g_ai_params;

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
	float dbg_cohesion_offset    = 0.f;
	float dbg_separation_offset  = 0.f;
	float dbg_draft_blend        = 0.f;  // 0..1, how strongly locked onto the draft leader's own lateral_pos
	float dbg_lineform_offset    = 0.f;
	float dbg_target_lat_offset  = 0.f;
	bool  dbg_clamped            = false;
	float dbg_lateral_shift      = 0.f;
	float dbg_heading_error      = 0.f;  // rad, signed angle from bike_direction to the racing line's own tangent
	bool  dbg_avoidance_active   = false;
	float dbg_avoidance_lat_term = 0.f;  // extra target-offset delta from yielding sideways (m)
	float dbg_avoidance_brake    = 0.f;  // extra brake_amount from an unavoidable conflict (0..1)
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
		float lateral_shift = 0.0; // -1..1 — the only lateral control: bends bike_direction toward a lateral target (rate-capped turn, see BikeObject::tick_transform)
		float brake_amount = 0.0;// 0,1
		float power = 0.0;	// input _watts_ requested


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

	Entity*               racing_line_entity = nullptr;
	MeshBuilderComponent* racing_line_mb     = nullptr;
};

// Shared per-rider stats text + gizmo overlay — defined in BikeApplication_Debug.cpp.
// Used by the index-based follow camera and BikeDebugger's click-to-select orbit camera.
void draw_rider_debug_info(BikeObject* bo);
