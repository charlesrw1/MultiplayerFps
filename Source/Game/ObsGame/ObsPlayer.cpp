#include "ObsGameHeaders.h"

#include "GameEnginePublic.h"
#include "Input/InputSystem.h"
#include "Game/GameplayStatic.h"
#include "Framework/MathLib.h"
#include "Debug.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

// ============================================================
// ObsPlayer
// ============================================================

void ObsPlayer::start()
{
	// Cross-component references (capsule/movement/hands) are populated by
	// ObsGameApplication::spawn_player AFTER create_component<ObsPlayer> runs,
	// so do not ASSERT them here. update() guards against null.
	set_ticking(true);
}

void ObsPlayer::stop() {}

glm::vec3 ObsPlayer::get_chest_pos() const
{
	return movement ? movement->get_position() : get_owner()->get_ws_position();
}

bool ObsPlayer::is_hanging() const
{
	const bool on_ground = movement && movement->is_touching_down();
	return !on_ground && ((left_hand && left_hand->is_grabbing()) ||
	                      (right_hand && right_hand->is_grabbing()));
}

// ============================================================
// Input
// ============================================================

static float gamepad_deadzone(double raw_norm)
{
	const float v = (float)raw_norm;
	const float dead = 0.18f;
	if (glm::abs(v) < dead) return 0.f;
	const float sign = v < 0.f ? -1.f : 1.f;
	return sign * (glm::abs(v) - dead) / (1.f - dead);
}

void ObsPlayer::read_input(glm::vec3& out_move_camspace,
                           float& out_left_trig, float& out_right_trig,
                           bool& out_jump_pressed)
{
	if (use_synthetic_input) {
		out_move_camspace = synthetic_move;
		out_left_trig     = synthetic_left_trig;
		out_right_trig    = synthetic_right_trig;
		out_jump_pressed  = synthetic_jump;
		synthetic_jump    = false; // one-shot
		return;
	}

	// Left stick → planar move (camera-space; ObsCamera resolves orientation).
	const float lx = gamepad_deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX));
	const float ly = gamepad_deadzone(Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY));
	// SDL: stick up = negative Y. We want forward (toward cam_forward) on stick up.
	out_move_camspace = glm::vec3(lx, 0.f, ly);

	// Triggers — SDL maps to [0..32767] then normalized to [0..1]; treat <0.05 as 0.
	const float lt_raw = (float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT);
	const float rt_raw = (float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
	out_left_trig  = glm::clamp(lt_raw,  0.f, 1.f);
	out_right_trig = glm::clamp(rt_raw, 0.f, 1.f);

	// Keyboard fallback for triggers — Q/E grab L/R; useful when no gamepad.
	if (Input::is_key_down(SDL_SCANCODE_Q)) out_left_trig  = 1.f;
	if (Input::is_key_down(SDL_SCANCODE_E)) out_right_trig = 1.f;

	// Keyboard WASD fallback for movement.
	if (!Input::last_recieved_input_from_con()) {
		glm::vec3 kb(0.f);
		if (Input::is_key_down(SDL_SCANCODE_A)) kb.x -= 1.f;
		if (Input::is_key_down(SDL_SCANCODE_D)) kb.x += 1.f;
		if (Input::is_key_down(SDL_SCANCODE_W)) kb.z -= 1.f;
		if (Input::is_key_down(SDL_SCANCODE_S)) kb.z += 1.f;
		if (glm::length(kb) > 0.001f)
			out_move_camspace = kb;
	}

	out_jump_pressed = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_A)
	                || Input::was_key_pressed(SDL_SCANCODE_SPACE);
}

void ObsPlayer::debug_drive(glm::vec3 move_xz, float left_trig, float right_trig, bool jump)
{
	use_synthetic_input  = true;
	synthetic_move       = glm::vec3(move_xz.x, 0.f, move_xz.z);
	synthetic_left_trig  = glm::clamp(left_trig,  0.f, 1.f);
	synthetic_right_trig = glm::clamp(right_trig, 0.f, 1.f);
	if (jump) synthetic_jump = true;
}

// ============================================================
// Tick
// ============================================================

void ObsPlayer::update()
{
	auto* app = ObsGameApplication::get();
	if (!app || !movement || !capsule) return;
	const float dt = (float)eng->get_dt();
	if (dt <= 0.f) return;

	// 1. Camera first — its basis is consumed by hands and locomotion below.
	ObsCamera* cam = app->camera;
	if (cam) cam->update();

	// 2. Read input.
	glm::vec3 move_cam = glm::vec3(0.f);
	float left_trig = 0.f, right_trig = 0.f;
	bool jump_pressed = false;
	read_input(move_cam, left_trig, right_trig, jump_pressed);

	const glm::vec3 cam_fwd = cam ? cam->get_forward() : glm::vec3(0, 0, -1);
	const glm::vec3 cam_right_v = cam ? cam->get_right()   : glm::vec3(1, 0, 0);
	const glm::vec3 cam_up_v    = cam ? cam->get_up()      : glm::vec3(0, 1, 0);

	// Project cam_fwd / cam_right onto horizontal plane for locomotion.
	glm::vec3 fwd_h = glm::vec3(cam_fwd.x, 0.f, cam_fwd.z);
	if (glm::length(fwd_h) > 1e-4f) fwd_h = glm::normalize(fwd_h);
	else                            fwd_h = glm::vec3(0, 0, -1);
	const glm::vec3 right_h = glm::vec3(fwd_h.z, 0.f, -fwd_h.x);

	const glm::vec3 chest_pos = get_chest_pos();

	// 3. Tick hands (PD reach + grab probe). Pass full 3D camera basis.
	if (left_hand)
		left_hand->tick_logic(chest_pos, cam_fwd, cam_right_v, cam_up_v, left_trig);
	if (right_hand)
		right_hand->tick_logic(chest_pos, cam_fwd, cam_right_v, cam_up_v, right_trig);

	// 4. Compute capsule velocity.
	const bool grounded = movement->is_touching_down();
	const bool hanging  = is_hanging();

	if (hanging) {
		// Pull chest toward average grabbed-hand position.
		glm::vec3 avg(0.f);
		int n = 0;
		if (left_hand && left_hand->is_grabbing())  { avg += left_hand->get_hand_pos();  ++n; }
		if (right_hand && right_hand->is_grabbing()) { avg += right_hand->get_hand_pos(); ++n; }
		if (n > 0) {
			avg /= (float)n;
			const glm::vec3 to_hands = avg - chest_pos
				- glm::vec3(0.f, app->shoulder_offset_y, 0.f); // bias so chest hangs slightly below the hands
			velocity = to_hands * app->pull_force;
		}
		// Hanging gravity (usually 0).
		velocity.y -= app->gravity * app->hang_gravity_scale * dt;
		// Bleed velocity so we don't drift forever.
		velocity *= glm::clamp(1.f - 4.f * dt, 0.f, 1.f);
	}
	else {
		// Ground / air locomotion.
		const float speed_scale = grounded ? 1.f : app->air_control;
		const glm::vec3 stick_world = (fwd_h * (-move_cam.z)) + (right_h * move_cam.x);
		const glm::vec3 desired_horiz = stick_world * app->walk_speed * speed_scale;

		// Snap horizontal velocity; preserve vertical for gravity/jump.
		velocity.x = desired_horiz.x;
		velocity.z = desired_horiz.z;

		// Gravity.
		velocity.y -= app->gravity * dt;

		// Jump.
		if (grounded && jump_pressed) {
			velocity.y = app->jump_velocity;
		}
		else if (grounded && velocity.y < 0.f) {
			velocity.y = -1.f; // small downward bias to keep contact with floor
		}
	}

	// 5. Move capsule.
	int flags = 0;
	(void)flags;
	movement->move(velocity * dt, dt, 0.001f);

	// CharacterMovementComponent::move writes its result into its internal
	// velocity (already in m/s — controller does disp/dt internally). Read it
	// back so we don't keep pushing into walls.
	velocity = movement->get_result_velocity();
	if (movement->is_touching_top() && velocity.y > 0.f)    velocity.y = 0.f;
	if (movement->is_touching_down() && velocity.y < 0.f && !jump_pressed) velocity.y = 0.f;

	// 6. Sync chest entity transform to the controller's authoritative position.
	get_owner()->set_ws_position(movement->get_position());

	was_grounded_last = grounded;

	// Debug HUD (one-line).
	if (app && !eng->is_editor_level()) {
		const glm::vec3 p = movement->get_position();
		char buf[160];
		snprintf(buf, sizeof(buf), "ObsGame  pos=(%.1f,%.1f,%.1f)  grounded=%d  hanging=%d  L=%.0f%%  R=%.0f%%",
		         p.x, p.y, p.z, grounded ? 1 : 0, hanging ? 1 : 0,
		         left_trig * 100.f, right_trig * 100.f);
		GameplayStatic::debug_text(std::string(buf));
	}
}
