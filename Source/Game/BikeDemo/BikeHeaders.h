#pragma once
#include "Game/EntityPtr.h"
#include "Game/Entity.h"
#include <array>
#include "Framework/MulticastDelegate.h"
#include "Sound/SoundPublic.h"

class CharacterController;

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
	float fov_smoothed   = 65.f;

	// power control state
	int power_level_idx = 4;   // index into BIKE_POWER_LEVELS, default 300W
	bool is_coasting = false;

	// speed hold (hold X / V)
	bool  speed_hold_active = false;
	float speed_hold_target = 0.f;  // m/s to maintain
	float speed_hold_power  = 0.f;  // smoothed power output (W)

	// sound
	SoundPlayer* freewheel_player = nullptr;

	~BikePlayer();

private:
	void update_camera(BikeObject* bike, float steer, float brake_amount);
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
	void update_tick(ControlInput input);

	glm::vec3 bike_direction = glm::vec3(0.f, 0, 1.f);
	float speed = 0.f;
	float current_turn = 0.0;
	float cadence = 0.f;	// cadence at gear
	float current_roll = 0.0;
	float current_steer = 0.f; // inertia-smoothed steer, persists briefly after input release
	float terrain_gradient    = 0.f;   // radians, + = uphill, - = downhill
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
