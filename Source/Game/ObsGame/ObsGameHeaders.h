#pragma once
// ObsGame — "Human Fall Flat"-like physics climbing demo.
//
// Architecture (see plan at plans/want-to-make-a-streamed-whisper.md):
//   ObsGameApplication  : Application subclass; owns the obstacle course + player.
//   ObsPlayer           : Component on the chest entity; kinematic capsule via
//                         CharacterMovementComponent. Owns two child hand entities.
//   ObsHand             : Component on each hand entity (dynamic SphereComponent).
//                         PD-force reach + grab FSM. Spawns a locked D6
//                         AdvancedJointComponent on grab; destroys it on release.
//   ObsCamera           : Component on a separate camera entity. Right-stick
//                         orbit follow tracking the chest.
//
// No shoulder D6 joint exists: AdvancedJointComponent's linear stiff/damp
// fields are not wired up to the underlying PxD6 drive, and its Y/Z motion
// fields are stomped by x_motion in init_joint (PhysicsJoints.cpp:200-202).
// Per-frame apply_force on the hand body covers reach + rest pose. Climb
// pull comes from ObsPlayer computing a target capsule velocity from the
// average grabbed hand offset.

#include "GameEnginePublic.h"
#include "Game/EntityComponent.h"
#include "Game/Entity.h"
#include "Game/EntityPtr.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Input/Sdl2CompatGamepad.h"

class ObsPlayer;
class ObsHand;
class ObsCamera;

class ObsGameApplication : public Application {
public:
	CLASS_BODY(ObsGameApplication);

	void start() final;
	void update() final;

	static ObsGameApplication* instance;
	static ObsGameApplication* get() { return instance; }

	// Spawn obstacle course geometry into the current level.
	void build_level();
	// Spawn the player (chest entity + two hand entities + camera entity).
	void spawn_player(glm::vec3 pos);

	// Goal callback — fires when player chest overlaps the goal trigger.
	void on_goal_reached();

	// --- Designer tuning ---
	REF float walk_speed         = 4.0f;
	REF float air_control        = 0.3f;
	REF float jump_velocity      = 5.5f;
	REF float gravity            = 18.0f;

	REF float arm_stiffness      = 80.0f;    // PD P-gain on hand apply_force
	REF float arm_damping        = 8.0f;     // PD D-gain
	REF float reach_distance     = 0.9f;     // extension along cam-forward at trigger=1
	REF float shoulder_offset_y  = 0.3f;
	REF float shoulder_offset_x  = 0.25f;
	REF float shoulder_offset_z  = 0.25f;    // baseline forward offset of hands
	REF float hand_radius        = 0.12f;
	REF float hand_density       = 2.0f;

	REF float grab_radius        = 0.22f;
	REF float pull_force         = 12.0f;    // capsule pull-toward-hand strength when hanging
	REF float hang_gravity_scale = 0.0f;

	REF float cam_distance       = 4.5f;
	REF float cam_height_offset  = 0.4f;
	REF float cam_pitch_min      = -1.1f;
	REF float cam_pitch_max      =  0.9f;
	REF float cam_yaw_sens       = 2.8f;
	REF float cam_pitch_sens     = 2.0f;

	REF float capsule_height     = 1.6f;
	REF float capsule_radius     = 0.3f;

	ObsPlayer* player    = nullptr;
	ObsCamera* camera    = nullptr;
	bool       reached_goal = false;
};

// ============================================================
// ObsPlayer — sits on the chest entity. Owns capsule controller,
// hand entities, and hanging-state logic.
// ============================================================
class ObsPlayer : public Component {
public:
	CLASS_BODY(ObsPlayer);

	void start() final;
	void update() final;
	void stop() final;

	// Test/CI hook: bypass SDL gamepad and inject input directly.
	REF void debug_drive(glm::vec3 move_xz, float left_trig, float right_trig, bool jump);
	REF void debug_set_use_synthetic_input(bool b) { use_synthetic_input = b; }

	// Queried by ObsCamera and ObsHand.
	REF glm::vec3 get_chest_pos() const;
	bool          is_hanging() const;

	// References (set during start by ObsGameApplication).
	CapsuleComponent*           capsule  = nullptr;
	CharacterMovementComponent* movement = nullptr;
	ObsHand*                    left_hand  = nullptr;
	ObsHand*                    right_hand = nullptr;

private:
	void read_input(glm::vec3& out_move_camspace,
	                float& out_left_trig, float& out_right_trig,
	                bool& out_jump_pressed);

	// Runtime state.
	glm::vec3 velocity            = glm::vec3(0.f);
	bool      synthetic_jump      = false;
	glm::vec3 synthetic_move      = glm::vec3(0.f);
	float     synthetic_left_trig = 0.f;
	float     synthetic_right_trig= 0.f;
	bool      use_synthetic_input = false;
	bool      was_grounded_last   = false;
};

// ============================================================
// ObsHand — per-hand state machine and physics-force driver.
// ============================================================
enum class ObsHandState : uint8_t {
	Idle,
	Reaching,
	Holding,
};

class ObsHand : public Component {
public:
	CLASS_BODY(ObsHand);

	void start() final;
	void update() final;
	void stop() final;

	// Per-tick called by ObsPlayer (kept simple — also runs from auto update()
	// as a redundancy if ObsPlayer isn't driving us; ObsPlayer::update sets
	// frame_inputs before our update runs, so reading them is safe).
	void tick_logic(const glm::vec3& chest_pos,
	                const glm::vec3& cam_forward,
	                const glm::vec3& cam_right,
	                const glm::vec3& cam_up,
	                float trigger_value);

	bool is_grabbing() const { return state == ObsHandState::Holding; }
	glm::vec3 get_hand_pos() const;
	glm::vec3 get_grab_pos() const { return grab_world_pos; }

	// Set by ObsPlayer at spawn.
	ObsPlayer* owner_player  = nullptr;
	float      shoulder_x_sign = 1.0f;   // +1 for right hand, -1 for left

	// Cached references (set in start()).
	SphereComponent* body = nullptr;

private:
	void try_begin_grab(const glm::vec3& probe_center);
	void end_grab();

	ObsHandState              state              = ObsHandState::Idle;
	AdvancedJointComponent*   grab_joint         = nullptr;
	glm::vec3                 grab_world_pos     = glm::vec3(0.f);

	// Per-frame inputs cached by tick_logic.
	glm::vec3 last_target_pos = glm::vec3(0.f);
	float     last_trigger    = 0.f;
};

// ============================================================
// ObsCamera — third-person orbital camera follow.
// ============================================================
class ObsCamera : public Component {
public:
	CLASS_BODY(ObsCamera);

	void start() final;
	void update() final;

	// Read by ObsHand for arm reach direction.
	glm::vec3 get_forward() const { return forward; }
	glm::vec3 get_right()   const { return right; }
	glm::vec3 get_up()      const { return up; }

	CameraComponent* cc = nullptr;

private:
	float     yaw      = 0.f;
	float     pitch    = -0.2f;
	glm::vec3 forward  = glm::vec3(0, 0, -1);
	glm::vec3 right    = glm::vec3(1, 0, 0);
	glm::vec3 up       = glm::vec3(0, 1, 0);
};
