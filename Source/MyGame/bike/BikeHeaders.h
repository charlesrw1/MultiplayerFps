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

class CharacterController;
class Texture;

// ============================================================
// Rider stats — fixed per-archetype attributes
// ============================================================
struct RiderStats {
	float base_ftp     = 280.f;    // W — functional threshold power
	float w_prime_max  = 20000.f;  // J — anaerobic work capacity
	float sprint_watts = 1100.f;   // W — hard ceiling on peak power
	float hr_rest      = 55.f;     // bpm
	float hr_max       = 185.f;    // bpm
};

// ============================================================
// Stamina state — runtime physiology
// ============================================================
struct StaminaState {
	float glycogen     = 1.f;      // [0,1] — 1.0 = fresh, depletes irreversibly
	float w_prime      = 20000.f;  // J     — recoverable anaerobic reservoir
	float lactate      = 0.f;      // J     — O2-debt accumulator; raises HR floor, decays ~5min
	float heat_stress  = 0.f;      // [0,1] — thermoregulatory load; driven by temp, sun, effort, cooled by airspeed
	float hr_current   = 55.f;     // bpm   — lagging HR
	float hr_drift     = 0.f;      // bpm   — cardiac drift from glycogen depletion (long-term)
	float hr_pulse_phase = 0.f;    // rad   — oscillates at HR rate, drives UI pulsing

	// Derived (recomputed each tick)
	float effective_ftp   = 280.f;
	float power_ceiling   = 1100.f;
	float actual_power    = 0.f;

	const char* legs_descriptor() const {
		if (glycogen > 0.85f) return "Fresh";
		if (glycogen > 0.70f) return "Good";
		if (glycogen > 0.55f) return "Fading";
		if (glycogen > 0.40f) return "Struggling";
		return "Cooked";
	}

	// 0..3 bars of W' remaining
	int w_prime_bars(float w_prime_max_) const {
		const float frac = w_prime / w_prime_max_;
		if (frac > 0.66f) return 3;
		if (frac > 0.33f) return 2;
		if (frac > 0.10f) return 1;
		return 0;
	}

	// HR zone 0..4
	int hr_zone(float hr_rest_, float hr_max_) const {
		const float frac = (hr_current + hr_drift - hr_rest_) / (hr_max_ - hr_rest_);
		if (frac < 0.60f) return 0;
		if (frac < 0.70f) return 1;
		if (frac < 0.80f) return 2;
		if (frac < 0.90f) return 3;
		return 4;
	}
};

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
// Strategic context (distilled intent from rule-based strategic layer)
// ============================================================
struct BikeStrategicState {
	float desired_effort_fraction = 1.0f;  // ×FTP: 0.5=easy, 1.0=tempo, 1.2=attack
	float target_lateral          = 0.f;   // preferred lateral position for echelon/peel
	int   tactical_objective      = 0;     // 0=Hold, 1=Follow, 2=Pull, 3=Peel, 4=DriftBack, 5=Attack, 6=Recover, 7=Sprint
};

// ============================================================
// Paceline rotation state machine
// ============================================================
enum class PacelineState {
	Following,      // normal: follow the rider ahead, take draft
	Pulling,        // at front of the AI group, doing work
	Peeling,        // done pulling: steer hard to one side to move out of the line
	DriftingBack,   // running at reduced power, drifting to the back of the group
};

static const char* paceline_state_name(PacelineState s) {
	switch (s) {
	case PacelineState::Following:    return "Following";
	case PacelineState::Pulling:      return "Pulling";
	case PacelineState::Peeling:      return "Peeling";
	case PacelineState::DriftingBack: return "DriftBack";
	}
	return "?";
}

// ============================================================
// BikeAIParams — global tuning knobs shared by all AI riders.
// Edit via the debug menu; never loop through riders to set these.
// ============================================================
struct BikeAIParams {
	// Waypoint following
	float lookahead_dist_base   = 0.8f;
	float lookahead_dist_per_ms = 0.4f;
	float steer_k               = 2.0f;

	// Corner scanning
	float corner_look_m  = 20.f;
	float corner_speed_k = 1.4f;

	// Steering anticipation
	float anticipation_dist_scale = 2.0f;
	float anticipation_k          = 1.0f;

	// Track boundary avoidance (PD controller on lateral position)
	float edge_predict_t  = 1.0f;  // seconds ahead to project arc
	float edge_safety_m   = 0.8f;  // danger zone margin inside road edge (m)
	float edge_steer_k    = 1.0f;  // P gain: steer per metre beyond safe zone
	float edge_vel_damp   = 0.4f;  // D gain: reduce correction as lateral_vel approaches centre

	// Off-track recovery: braking + committed steer when past the road edge
	float edge_off_brake_k   = 0.5f;  // brake fraction per metre past road edge
	float edge_off_brake_max = 0.6f;  // maximum brake fraction during off-track recovery

	// Wheel picker — see [[bike/bikeai#Wheel picking]]
	// long_max is wider than the canonical 8m to allow chains to re-form after a
	// gap opens (gap-regulation P-control then closes the gap via power). lat_max
	// must be ≥ 2*clear_air_max_offset so two riders in opposite lanes can still
	// see each other as candidates.
	float wheel_long_min     = 0.3f;  // min longitudinal gap for a candidate (m)
	float wheel_long_max     = 15.0f; // max longitudinal gap (m)
	float wheel_lat_max      = 3.0f;  // lateral overlap window (m)
	float wheel_long_gap     = 1.5f;  // ideal wheel-to-wheel target gap (m)
	float wheel_w_long       = 1.0f;  // score weight: closeness to ideal long_gap
	float wheel_w_lat        = 1.5f;  // score weight: lateral alignment to my offset
	float wheel_w_draft      = 0.8f;  // score weight: draft potential (1 - draft_factor)
	float wheel_stickiness   = 1.0f;  // score bonus for keeping current wheel
	float wheel_score_thresh = 0.0f;  // if best score below this, no wheel (leader)

	// Corner factor — pulls lat_offset → 0 in tight corners
	float corner_factor_r_full = 30.f;  // radius (m) at which corner_factor = 1
	float corner_factor_min    = 0.3f;  // floor on corner_factor (very tight corners)

	// Follower near-field lateral correction (PD-based steering converges slowly
	// because lookahead is far; this term injects road-frame lat_err directly
	// into steer for tight wheel-hugging). Only fires for followers.
	float follower_lat_k       = 0.5f;  // steer per metre of lateral error from wheel's track
	float follower_lat_d_k     = 0.2f;  // steer per (m/s) of lateral velocity (damping)

	// Clear-air resolver — see [[bike/bikeai#Lateral offset rule]]
	float clear_air_lat_window  = 1.0f;   // m — lateral half-window of overlap penalty
	float clear_air_long_window = 1.0f;   // m — longitudinal half-window of overlap penalty
	float clear_air_center_bias = 0.05f;  // score penalty per m of |offset - bias| (prefers draft)
	float clear_air_damp_tau    = 1.5f;   // s  — low-pass time constant on lat_offset
	float clear_air_max_offset  = 0.7f;   // m  — search range each side of bias
	                                      //       (must be ≤ wheel_lat_max/2 or chains break)
	float clear_air_step        = 0.35f;  // m  — candidate spacing

	// Paceline FSM — see [[bike/bikeai#Tactical FSM]]
	float pull_cooldown_s    = 8.f;    // min seconds between pulls (per rider)
	float pull_duration_s    = 30.f;   // max time spent at front before peeling
	float peel_duration_s    = 2.5f;   // time spent steering to the side
	float drift_duration_s   = 6.f;    // max drift-back duration (cap)
	float peel_offset_m      = 1.0f;   // |lat_offset| while peeling
	float peel_power_delta_w = -50.f;  // ΔW vs Following while peeling
	float drift_power_frac   = 0.70f;  // multiplier on Following power while drifting back
	float pull_power_frac    = 1.00f;  // multiplier on Following power while pulling
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

	// ---- Per-rider state ----
	float target_power_watts   = 250.f;
	float actual_power_command = 150.f;
	static constexpr float POWER_SLEW = 0.05f;


	// ---- Wheel-following state (set by BikeGameApplication::update_wheel_picking each frame) ----
	// wheel == null  →  I'm a leader; target = racing-line lookahead.
	// wheel != null  →  follow this rider's road frame at offset (long_gap, lat_offset).
	BikeObject* wheel           = nullptr;
	float       lat_offset      = 0.f;  // m — smoothed road-relative offset from the wheel's track
	float       lat_offset_bias = 0.f;  // m — per-rider personality preference (~±0.2)
	float       long_gap        = 1.5f; // m — target longitudinal spacing behind wheel

	// ---- Paceline FSM (set by BikeGameApplication::update_paceline each frame) ----
	PacelineState paceline_state    = PacelineState::Following;
	float         paceline_timer_s  = 0.f;     // seconds spent in current state
	float         pull_cooldown_s   = 0.f;     // seconds remaining before another pull is allowed
	float         peel_side_sign    = 1.f;     // +1 = peel road-right, -1 = road-left

	// ---- Double echelon lane assignment ----
	float preferred_lateral = 0.f;
	float lane_strength     = 0.f;

	// ---- Hard steer clamp — set by BikeGameApplication::update_boids each frame ----
	float hard_steer_min = -1.f;
	float hard_steer_max =  1.f;

	// ---- Debug ----
	glm::vec3 dbg_lookahead_pt{};
	float dbg_steer_pre_boids    = 0.f;
	float dbg_steer_pre_hard     = 0.f;
	float dbg_steer_final        = 0.f;
	float dbg_power_base         = 0.f;
	float dbg_power_final        = 0.f;
	float dbg_brake_amount       = 0.f;
	float dbg_brake_dist_m       = 0.f;
	float dbg_brake_corner_r     = 0.f;
	float dbg_min_r              = 0.f;
	float dbg_v_max              = 0.f;
	float dbg_lookahead_dist     = 0.f;
	float dbg_steer_near         = 0.f;
	float dbg_steer_far          = 0.f;
	float dbg_edge_steer         = 0.f;
	float dbg_edge_brake         = 0.f;
	float dbg_pred_lateral       = 0.f;
	bool  dbg_off_track          = false;
	float dbg_avoid_steer        = 0.f;
	float dbg_avoid_brake        = 0.f;
	bool  dbg_has_wheel          = false;
	float dbg_wheel_score        = 0.f;
	float dbg_corner_factor      = 1.f;
	float dbg_lat_offset_target  = 0.f;
};

class BikePlayer : public IBikeInput {
public:
	BikePlayer();
	~BikePlayer();
	void evaluate(BikeObject* my_bike) final;
	void update_camera(BikeObject* bike, float steer, float brake_amount);
	void draw_power_meter(float current_watts, int power_idx, bool coasting, bool speed_hold, float speed_hold_watts, float actual_watts, float power_ceiling);
	void draw_stamina_ui(const StaminaState& s, const RiderStats& r);

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

	Texture* heart_icon_tex = nullptr;
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
	RiderStats rider;
	StaminaState stamina;

	void start() final;
	void update() final;

	// input:
	struct ControlInput {
		float aero_coeff = 0.0;	// determied by stance
		float steer = 0.0;// l,r
		float brake_amount = 0.0;// 0,1
		float power = 0.0;	// input _watts_ requested


		bool is_coasting() const { return power == 0.0; }
	};
	ControlInput update_tick(ControlInput input);

	// Sub-tick functions (called in order by update_tick)
	void tick_stamina(ControlInput& ci, float dt);
	void tick_physics(ControlInput& ci, float dt);
	void tick_steer(const ControlInput& ci, float dt);
	void tick_gears(float dt);
	void tick_transform(const ControlInput& ci, float dt);
	float get_wind_along_bike() const;
	glm::vec3 get_wind_along_bike_vector() const;


	// Cross-tick communication (written by one sub-tick, read by another)
	float     steer_input_raw      = 0.f;       // resolved raw stick input (0 when crashed)
	glm::vec3 terrain_forward_dir  = {0,0,1};   // terrain-aligned forward from last raycast

	glm::vec3 bike_direction = glm::vec3(0.f, 0, 1.f);
	float speed = 0.f;
	float speed_smoothed = 0.f; // low-pass filtered speed, used for gear cadence checks
	float turn_rate = 0.f;   // rad/s, written by update_tick() from physics each frame
	float cadence = 0.f;	// cadence at gear
	float current_roll = 0.0;
	float current_steer    = 0.f;  // inertia-smoothed steer, persists briefly after input release
	float steer_committed  = 0.f;  // current_steer after inertia but before wobble/wind/bump — written by update_tick()
	float prev_steer_input = 0.f; // raw steer last frame, for stick velocity calculation
	float terrain_gradient    = 0.f;   // radians, + = uphill, - = downhill
	float prev_gradient       = 0.f;   // last frame gradient, for bump detection
	float bump_impulse        = 0.f;   // magnitude of bump this frame (speed-scaled)
	float crack_impulse       = 0.f;   // set by app when bike crosses a crack decal
	float crack_cooldown      = 0.f;   // seconds until crack can retrigger

	// Cosmetic crack pitch spring (visual only, does not affect physics)
	float crack_pitch_disp    = 0.f;   // current angular displacement (radians)
	float crack_pitch_vel     = 0.f;   // angular velocity (rad/s)

	// Bump steer spring — handlebar kickback from road irregularities
	float bump_steer_disp     = 0.f;   // lateral steer displacement [-1,1] fraction
	float bump_steer_vel      = 0.f;   // rate of change
	float gear_shift_cooldown = 0.f;   // seconds remaining until next shift is allowed
	bool  just_shifted        = false; // set true for one tick when a shift occurs
	GearSelector gear;

	float wobble_steer = 0.f;
	float wobble_vel   = 0.f;
	float wobble_timer = 0.f;
	float wind_steer          = 0.f;  // crosswind handlebar perturbation
	float wind_vel            = 0.f;  // rate of change of wind_steer
	float crosswind_gust_timer = 0.f; // countdown to next gust event
	float stroke_phase       = 0.f;  // pedal cycle phase (0..2π per revolution)
	float stroke_speed_smooth = 0.f; // heavily smoothed speed for phase advancement (breaks feedback loop)

	float surface_traction  = 1.0f;   // [0,1] — road grip: 1=dry tarmac, 0.6=wet, 0.3=gravel
	float rear_skid         = 0.f;    // [0,1] — rear wheel lock from heavy braking (recoverable)
	bool  is_crashed        = false;  // true when corner overspeed exceeded the grip limit
	float crash_timer       = 0.f;    // seconds until rider can resume after a crash
	float corner_warn_timer = 0.f;    // seconds the corner has been over the grip limit

	// Course state (updated each frame by BikeGameApplication before input runs)
	float course_dist_m  = 0.f;   // arc-length from course start (m)
	float lateral_pos    = 0.f;   // signed offset from road centre, +ve = road-right (m)
	int   course_segment = 0;     // nearest waypoint segment index (cached)
	int   race_position  = 0;     // 1-indexed finishing position in sorted rider list

	// Group context (written by BikeGameApplication::update_groups each frame)
	int   group_id           = 0;
	float pos_in_group_norm  = 0.f;  // 0=front of group, 1=back
	float group_rank_norm    = 0.f;  // 0=leading group, 1=last group
	float group_size_norm    = 0.f;  // group_size / total_riders

	// Strategic context (set by strategic layer or paceline logic)
	BikeStrategicState strategic_state;

	// Drafting (written by BikeGameApplication::update_drafting before physics runs)
	// 1.0 = no draft (open air), 0.65 = full draft at ideal position
	float draft_factor = 1.0f;

	// Lateral position history and velocity (written at end of update_boids)
	float prev_lateral_pos = 0.f;
	float lateral_vel      = 0.f;  // m/s, positive = moving road-right

	// Pack outputs (written by BikeGameApplication::update_boids, read by input handlers)
	float boid_long_sep_power    = 0.f;  // W shed when side-by-side (slightly behind yields)

	// Collision avoidance outputs (written by BikeGameApplication::update_boids, read by BikeAI::evaluate)
	float avoidance_sep_steer = 0.f;  // predictive soft lateral push away from nearby riders
	float avoidance_brake     = 0.f;  // [0,1] brake pressure when squeezed with no lateral escape

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

	void start() final;
	void update() final;
	void on_imgui() final;

	BikeObject* create_player(glm::vec3 pos);
	BikeObject* create_ai(glm::vec3 pos);

	BikeCourse    course;
	BikeDebugger  debugger;

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
	void update_course_positions();
	void sort_riders();
	void update_groups();
	void update_drafting();
	void update_wheel_picking();
	void update_clear_air();
	void update_paceline();
	void update_avoidance();
	void update_gap_regulation();
	void update_crack_triggers();
	void debug_draw_course() const;
};

// Shared per-rider stats text + gizmo overlay — defined in BikeApplication_Debug.cpp.
// Used by the index-based follow camera and BikeDebugger's click-to-select orbit camera.
void draw_rider_debug_info(BikeObject* bo);
