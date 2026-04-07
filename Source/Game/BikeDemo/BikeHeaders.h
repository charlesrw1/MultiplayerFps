#pragma once
#include "Game/EntityPtr.h"
#include "Game/Entity.h"
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
	float hr_current   = 55.f;     // bpm   — lagging HR
	float hr_drift     = 0.f;      // bpm   — cardiac drift from glycogen depletion
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
class BikeAI : public IBikeInput {
public:
	// ai logic, then feeds into bike character
	// state machine logic
};
class BikePlayer : public IBikeInput {
public:
	BikePlayer();
	void evaluate(BikeObject* my_bike) final;
	void update_camera(BikeObject* bike, float steer, float brake_amount);
	void update_wind(BikeObject* bike);
	void draw_power_meter(float current_watts, int power_idx, bool coasting, bool speed_hold, float speed_hold_watts, float actual_watts, float power_ceiling);
	void draw_stamina_ui(const StaminaState& s, const RiderStats& r);

	// camera
	CameraComponent* cc = nullptr;
	glm::vec3 camera_pos{};
	glm::vec3 smooth_aim_pos{};  // lagged look-at target
	bool camera_initialized = false;
	float camera_yaw     = 0.f;
	float camera_pitch   = 0.f;
	float gradient_pitch = 0.f;
	float brake_pitch    = 0.f;
	float lead_yaw       = 0.f;
	float fov_smoothed      = 65.f;
	float current_power     = 0.f;   // power output this frame (0 when coasting)

	// power control state
	int   power_level_idx  = 4;    // index into BIKE_POWER_LEVELS, default 300W
	bool  is_coasting      = false;
	float power_hold_timer  = 0.f; // how long dpad/key has been held
	float power_repeat_timer = 0.f; // accumulator for repeat firings

	// speed hold (hold X / V)
	bool  speed_hold_active = false;
	float speed_hold_target = 0.f;  // m/s to maintain
	float speed_hold_power  = 0.f;  // smoothed power output (W)

	// sound
	SoundPlayer* freewheel_player = nullptr;
	SoundPlayer* wind_player      = nullptr;
	SoundPlayer* pedal_player     = nullptr;

	// road bump fx state
	float prev_gradient   = 0.f;
	// pitch spring: impulse forces camera down, springs back
	float bump_pitch_disp = 0.f;
	float bump_pitch_vel  = 0.f;
	// vertical spring: impulse pushes camera up, springs back
	float bump_vert_disp  = 0.f;
	float bump_vert_vel   = 0.f;
	// camera shake: random offset decaying over time
	glm::vec3 shake_offset{};
	float shake_magnitude = 0.f;

	// camera roll on cornering
	float camera_roll = 0.f;

	// cadence bob
	float cadence_bob_phase = 0.f;

	// stamina UI
	Texture* heart_icon_tex = nullptr;

	// speed lines (animated, flow outward)
	static constexpr int MAX_SPEEDLINES = 48;
	float sl_phases[MAX_SPEEDLINES] = {};  // per-line 0→1 phase (0=inner, 1=outer)
	float sl_angles[MAX_SPEEDLINES] = {};  // per-line fixed base angle (radians)
	bool  sl_initialized = false;
	MeshBuilder speedlines_mb{};
	handle<Particle_Object> speedlines_handle{};

	// wind lines
	struct WindLine {
		glm::vec3 pos;
		float lifetime;
		float max_life;
		float wave_phase;   // unused, reserved
		float wave_speed;   // per-streak speed multiplier
		float radius;       // spawn/death distance from player
		float len;          // ribbon length
		float width;        // ribbon max half-width
		float alpha;        // max alpha (0-255)
	};
	static constexpr int WIND_LINE_COUNT = 20;
	WindLine wind_lines[WIND_LINE_COUNT] = {};
	bool wind_initialized = false;
	MeshBuilder wind_mb{};
	handle<Particle_Object> wind_handle{};

	~BikePlayer();
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
	float get_wind_along_bike() const;
	glm::vec3 get_wind_along_bike_vector() const;


	glm::vec3 bike_direction = glm::vec3(0.f, 0, 1.f);
	float speed = 0.f;
	float speed_smoothed = 0.f; // low-pass filtered speed, used for gear cadence checks
	float current_turn = 0.0;
	float cadence = 0.f;	// cadence at gear
	float current_roll = 0.0;
	float current_steer = 0.f; // inertia-smoothed steer, persists briefly after input release
	float terrain_gradient    = 0.f;   // radians, + = uphill, - = downhill
	float prev_gradient       = 0.f;   // last frame gradient, for bump detection
	float bump_impulse        = 0.f;   // magnitude of bump this frame (speed-scaled)
	float gear_shift_cooldown = 0.f;   // seconds remaining until next shift is allowed
	bool  just_shifted        = false; // set true for one tick when a shift occurs
	GearSelector gear;

};


#include "Framework/Hashset.h"
class BikeGameApplication : public Application {
public:
	CLASS_BODY(BikeGameApplication);

	void start() final;
	void update() final;

	BikeObject* create_player(glm::vec3 pos);
};
