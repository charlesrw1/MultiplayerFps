#pragma once
#include "Game/EntityPtr.h"
#include "Game/Entity.h"
#include "BikeCourse.h"
#include <array>
#include "Framework/MulticastDelegate.h"
#include "Framework/MeshBuilder.h"
#include "Sound/SoundPublic.h"

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

class BikeAI : public IBikeInput {
public:
	void evaluate(BikeObject* my_bike) final;

	// Set once after construction — points to the application-owned course.
	BikeCourse* course = nullptr;

	// ---- Waypoint following ----
	float lookahead_dist_base   = 1.5f;  // min lookahead distance (m)
	float lookahead_dist_per_ms = 0.5f;  // additional metres per m/s of speed
	float steer_k               = 2.3f;   // gain on lateral error — higher = harder correction

	float corner_look_m  = 50.f;   // metres ahead to scan for corners (wider = earlier braking)
	// v_max in corner = sqrt(corner_speed_k * g * R * traction)
	float corner_speed_k = 15.f;

	// ---- Power ----
	float target_power_watts   = 250.f;  // set by strategy layer (Layer 6); fixed for now
	float actual_power_command = 150.f;  // smoothed toward target
	static constexpr float POWER_SLEW = 0.05f; // damp rate

	// ---- Paceline rotation ----
	PacelineState paceline_state  = PacelineState::Following;
	float pull_timer              = 0.f;   // elapsed seconds in Pulling state
	float pull_duration           = 10.f;  // randomised seconds to pull before peeling
	float peel_dir                = 1.f;   // +1 = peel toward positive lateral, -1 = negative
	float peel_lateral_tgt        = 0.f;   // absolute lateral target while peeling/drifting

	// ---- Double echelon lane assignment ----
	// preferred_lateral = 0 → single paceline (default)
	// preferred_lateral = ±1.5 → double echelon lane assignment
	// lane_strength blends cohesion between following rider-ahead and returning to lane
	float preferred_lateral       = 0.f;
	float lane_strength           = 0.f;

	// ---- Debug ----
	glm::vec3 dbg_lookahead_pt{};  // world-space lookahead point, drawn in debug
	float dbg_steer_pre_boids    = 0.f;  // PID steer before boid forces
	float dbg_steer_pre_hard     = 0.f;  // steer after boids, before hard clamp
	float dbg_steer_final        = 0.f;  // final clamped steer submitted to physics
	float dbg_power_base         = 0.f;  // smoothed target power before boid modifications
	float dbg_power_align_nudge  = 0.f;  // speed-alignment nudge from pack average
	float dbg_power_seek_bonus   = 0.f;  // gap-close bonus toward rider ahead
	float dbg_power_final        = 0.f;  // total power submitted to physics
	float dbg_brake_amount       = 0.f;  // brake command [0,1]
	float dbg_min_r              = 0.f;  // nearest corner radius (m) from scan window
	float dbg_v_max              = 0.f;  // safe cornering speed (m/s) for that radius
	float dbg_lookahead_dist     = 0.f;  // actual lookahead distance used this frame (m)
};

// ============================================================
// Neural net steering POC — behavior cloning from BikeAI
// ============================================================
static constexpr int BIKENN_INPUT_DIM  = 7;
static constexpr int BIKENN_HIDDEN_DIM = 32;

// Feature vector extracted from BikeObject + BikeCourse each frame.
// Used identically by the recorder and the inference path.
struct BikeNNFeatures {
	float v[BIKENN_INPUT_DIM];
	// Fills v[] from current bike state.  Requires course->is_built.
	static BikeNNFeatures extract(BikeObject* bike, const BikeCourse* course);
};

// Writes (features[7], steer_label[1]) records to a binary file for offline training.
class BikeNNRecorder {
public:
	~BikeNNRecorder() { close(); }
	void open(const char* path);
	void close();
	// Records one sample if enabled and frame_skip allows it.
	void try_record(const BikeNNFeatures& f, float steer_label);

	bool  enabled    = false;
	int   frame_skip = 1;   // record every N frames
	int   frame_count = 0;
	int   sample_count = 0;
private:
	FILE* file_ = nullptr;
};
extern BikeNNRecorder g_nn_recorder;

// IBikeInput implementation that runs neural net inference for steering.
// Power management is a simplified constant-power slew (no gap following).
class BikeNNInput : public IBikeInput {
public:
	~BikeNNInput() override = default;
	void evaluate(BikeObject* my_bike) final;

	BikeCourse* course = nullptr;

	// Load weights written by BikeNNTrainer::train_and_save.
	// Returns false on failure (evaluate will fall back to steer=0).
	bool load_weights(const char* path);
	bool weights_loaded = false;

	float target_power_watts   = 250.f;
	float actual_power_command = 150.f;
	static constexpr float POWER_SLEW = 0.05f;

	float dbg_steer = 0.f;

private:
	float mean_[BIKENN_INPUT_DIM]                        = {};
	float std_ [BIKENN_INPUT_DIM]                        = {};
	float W1_  [BIKENN_HIDDEN_DIM][BIKENN_INPUT_DIM]     = {};
	float b1_  [BIKENN_HIDDEN_DIM]                       = {};
	float W2_  [BIKENN_HIDDEN_DIM]                       = {};
	float b2_                                            = 0.f;

	float forward(const BikeNNFeatures& f) const;
};

// Trains a BikeNNInput-compatible MLP from recorded data entirely in C++.
// Call train_and_save() from the debug menu after recording enough laps.
class BikeNNTrainer {
public:
	struct TrainParams {
		int   epochs     = 500;
		float lr         = 1e-3f;
		int   batch_size = 64;
	};

	// Reads data_path, runs Adam training, writes weights to weights_path.
	// Reports progress via sys_print.  Blocks for ~50-200 ms typical.
	// Returns final mean-squared error, or <0 on I/O failure.
	float train_and_save(const char* data_path, const char* weights_path,
	                     const TrainParams& p = {});
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

	// Boid debug (written each frame during evaluate, read by debug menu)
	float dbg_boid_sep_steer      = 0.f;
	float dbg_boid_align_steer    = 0.f;
	float dbg_boid_cohesion_steer = 0.f;
	float dbg_boid_align_power    = 0.f;
	float dbg_draft_seek_power    = 0.f;
	float dbg_steer_before_boids  = 0.f;
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
	float current_turn = 0.0;
	float cadence = 0.f;	// cadence at gear
	float current_roll = 0.0;
	float current_steer = 0.f; // inertia-smoothed steer, persists briefly after input release
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

	// Drafting (written by BikeGameApplication::update_drafting before physics runs)
	// 1.0 = no draft (open air), 0.65 = full draft at ideal position
	float draft_factor = 1.0f;

	// Lateral position history for boid derivative term (written at end of update_boids)
	float prev_lateral_pos = 0.f;

	// Pack context (written by BikeGameApplication::update_gaps each frame)
	float       gap_to_ahead_m       = 999.f;   // gap to locked rider_ahead (m)
	float       gap_to_behind_m      = 999.f;   // gap to immediate rider behind (m)
	BikeObject* rider_ahead          = nullptr;  // sticky locked target ahead; null if none in range
	BikeObject* rider_behind         = nullptr;  // immediate rider behind (simple, not sticky)
	float       rider_ahead_switch_t = 0.f;      // urgency-weighted time accumulating toward switch

	// Boid outputs (written by BikeGameApplication::update_boids, read by input handlers)
	float boid_separation_steer  = 0.f;  // lateral push away from overlapping riders
	float boid_align_steer       = 0.f;  // steer nudge to match pack heading direction
	float boid_cohesion_steer    = 0.f;  // steer nudge toward rider-ahead's lateral (paceline)
	float boid_align_power_nudge = 0.f;  // W to add/subtract so rider converges to pack speed
	float boid_long_sep_power    = 0.f;  // W shed when side-by-side (slightly behind yields)

	// Hard steer limits (written by BikeGameApplication::update_boids, applied by BikeAI)
	// Clamp the final steer command to prevent the bike from steering into a neighbour.
	// [-1, +1] at rest; narrowed to [0,+1] or [-1,0] when a rider is in the exclusion zone.
	float hard_steer_min = -1.f;
	float hard_steer_max =  1.f;

	// Cohesion PD debug terms (written alongside boid_cohesion_steer)
	float dbg_cohesion_lat_err = 0.f;  // kp term input: lateral offset to rider ahead
	float dbg_cohesion_lat_vel = 0.f;  // kd term input: my lateral velocity

	EntityPtr fork_entity;

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

	BikeObject* create_player(glm::vec3 pos);
	BikeObject* create_ai(glm::vec3 pos);

	BikeCourse course;

	// All riders (player + AI), populated by create_player / create_ai
	std::vector<BikeObject*> all_riders;
	// Sorted front-to-back by course_dist_m each frame (index 0 = race leader)
	std::vector<BikeObject*> riders_sorted;

	// Paceline/echelon mode: when true, AIs are assigned alternating preferred_lateral lanes
	// and the pull-through rotation is active.
	bool paceline_active  = false;
	bool echelon_mode     = false;
	int  num_ai           = 5;
	bool use_nn_ai        = true;  // when true, AI riders use BikeNNInput instead of BikeAI

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
	void update_gaps();
	void update_drafting();
	void update_boids();
	void update_paceline();
	void update_crack_triggers();
	void debug_draw_course() const;
};
