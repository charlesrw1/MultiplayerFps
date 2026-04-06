#include "BikeHeaders.h"

#include "Game/GameplayStatic.h"
#include "Game/Components/MeshComponent.h"
#include "Game/Components/PhysicsComponents.h"
#include "Render/Model.h"
#include "Game/Entities/CharacterController.h"
#include "Input/InputSystem.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"

#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Physics/Physics2.h"
#include <algorithm>
#include <cfloat>

// ============================================================
// BikeGameApplication
// ============================================================

void BikeGameApplication::start()
{
	GameplayStatic::change_level("maps/bike_test_map.tmap");
	create_player(glm::vec3(0.f));
}

void BikeGameApplication::update()
{
}

BikeObject* BikeGameApplication::create_player(glm::vec3 pos)
{
	Entity* e = GameplayStatic::spawn_entity();
	e->set_ws_position(pos);
	auto bo = e->create_component<BikeObject>();
	bo->input = std::make_unique<BikePlayer>();
	return bo;
}


// ============================================================
// BikeObject
// ============================================================

void BikeObject::start()
{
	set_ticking(true);
	auto* model = get_owner()->create_component<MeshComponent>();
	model->set_model(Model::load("cube1m.cmdl"));

	bike_direction = glm::vec3(0.f, 0.f, 1.f);
}

void BikeObject::update()
{
	GameplayStatic::reset_debug_text_height();
	if (input)
		input->evaluate(this);
}

static float steer_speed_gate_lo   = 6.2f;   // m/s — speed_factor = 0 below this
static float steer_speed_gate_hi   = 18.f;   // m/s — speed_factor = 1 above this
static float steer_lean_threshold  = 0.55f;  // stick fraction at which inertia starts
static float steer_lean_range      = 0.35f;  // stick range over which inertia ramps to full
static float steer_build_lo        = 0.070f; // build smoothing at low speed (snappy)
static float steer_build_hi        = 0.120f; // build smoothing at high speed (gradual)
static float steer_release_lo      = 0.000f; // release smoothing when not committed
static float steer_release_hi      = 0.007f;  // release smoothing when deeply committed
static float steer_max_deg         = 45.f;   // max physical steer angle (degrees)
static float steer_min_radius      = 1.5f;   // minimum turn radius (m)
static float steer_radius_coeff    = 0.09f;  // speed² coefficient for min radius

void BikeObject::update_tick(ControlInput ci)
{
	const float dt = eng->get_dt();

	// --- Physics constants ---
	const float mass           = 83.f;    // kg  (rider ~75 + bike ~8)
	const float wheel_circ     = 2.1f;    // m   (700c wheel ~2.1m circumference)
	const float CdA            = 0.3f;    // m^2 (drag area, hoods position)
	const float air_density    = 1.225f;  // kg/m^3
	const float roll_resist    = 0.004f;  // dimensionless rolling resistance coeff
	const float gravity        = 9.81f;
	const float max_brake_decel = 7.f;    // m/s^2 at full brake lever

	// --- Drive force:  P = F*v  =>  F = P/v ---
	// Clamp min speed to avoid division by zero on standing start
	const float safe_speed  = glm::max(speed, 0.3f);
	const float drive_force = ci.power / safe_speed;

	// --- Resistance / gradient forces ---
	// Aero drag: quadratic with velocity
	const float drag    = 0.5f * air_density * CdA * speed * speed;
	// Rolling resistance: only while moving
	const float rolling = (speed > 0.05f) ? (roll_resist * mass * gravity) : 0.f;
	// Braking
	const float braking = ci.brake_amount * mass * max_brake_decel;
	// Gravity along slope: + = uphill resistance, - = downhill assist
	// terrain_gradient is updated at end of frame (one-frame delay is fine)
	const float slope_force = mass * gravity * glm::sin(terrain_gradient);

	const float net_force = drive_force - drag - rolling - braking - slope_force;
	speed += (net_force / mass) * dt;
	speed = glm::max(0.f, speed);

	// --- Committed steer (lean-in + inertia) ---
	//
	// Two effects gated by speed:
	//
	// 1. BLEND-IN: at high speed the effective steer builds gradually (you're
	//    committing lean mass, not just flicking bars). At low speed it's direct.
	//
	// 2. INERTIA ON RELEASE: only when BOTH speed is high AND the turn is deeply
	//    committed (>~55% stick). Small corrections and low-speed turns release
	//    instantly regardless of speed.
	//
	const float speed_range  = glm::max(steer_speed_gate_hi - steer_speed_gate_lo, 0.001f);
	const float speed_factor = glm::clamp((speed - steer_speed_gate_lo) / speed_range, 0.f, 1.f);
	const float lean_depth   = glm::clamp((glm::abs(current_steer) - steer_lean_threshold) / glm::max(steer_lean_range, 0.001f), 0.f, 1.f);
	const float inertia      = speed_factor * lean_depth;



	const float build_smoothing   = glm::mix(steer_build_lo,   steer_build_hi,   speed_factor);
	const float release_smoothing = glm::mix(steer_release_lo, steer_release_hi, inertia);


	if (glm::abs(ci.steer) > glm::abs(current_steer)) {
		current_steer = damp_dt_independent(ci.steer, current_steer, build_smoothing, dt);
	} else {
		current_steer = damp_dt_independent(ci.steer, current_steer, release_smoothing, dt);
	}
	GameplayStatic::debug_text(string_format("speed_factor = %.3f", speed_factor));
	GameplayStatic::debug_text(string_format("lean_depth = %.3f", lean_depth));
	GameplayStatic::debug_text(string_format("inertia = %.3f", inertia));
	GameplayStatic::debug_text(string_format("steer = %.3f", current_steer));



	// --- Steering (bicycle geometry) ---
	// Turn radius R = wheelbase / tan(steer_angle).
	// Minimum radius grows quadratically with speed — at 50 km/h a road bike
	// realistically needs 15-20m to corner, not 5m.
	const float wheelbase       = 1.0f;
	const float max_steer_rad   = glm::radians(45.f);
	const float min_turn_radius = glm::max(1.5f, speed * speed * 0.09f); // ~2m@5, ~9m@10, ~20m@15 m/s

	const float steer_angle = current_steer * max_steer_rad;
	float turn_rate = 0.f;
	if (glm::abs(steer_angle) > 0.001f) {
		const float radius = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_radius);
		turn_rate = (speed / radius) * glm::sign(current_steer);
	}

	if (glm::abs(turn_rate) > 0.0001f) {
		const float angle = -turn_rate * dt;
		const glm::mat3 rot = glm::mat3(glm::rotate(glm::mat4(1.f), angle, glm::vec3(0, 1, 0)));
		bike_direction = glm::normalize(rot * bike_direction);
	}

	// --- Visual lean (roll into turns) ---
	const float lean_speed_scale = glm::clamp(speed / 15.f, 0.f, 1.f);
	const float target_roll = current_steer * lean_speed_scale * 0.30f;
	current_roll = damp_dt_independent(target_roll, current_roll, 0.02f, dt);

	// --- Auto gear selection (target ~90 RPM = 1.5 rev/s) ---
	gear_shift_cooldown = glm::max(0.f, gear_shift_cooldown - dt);
	just_shifted = false;
	const float target_cadence_rps = 1.5f;
	if (gear_shift_cooldown == 0.f) {
		int best_gear = gear.current_high_gear;
		float best_diff = FLT_MAX;
		for (int i = 0; i < (int)gear.back_cogs.size(); i++) {
			const float ratio = (float)gear.front_cogs[gear.current_low_gear] / (float)gear.back_cogs[i];
			const float cad   = speed / (ratio * wheel_circ);
			const float diff  = glm::abs(cad - target_cadence_rps);
			if (diff < best_diff) { best_diff = diff; best_gear = i; }
		}
		if (best_gear != gear.current_high_gear) {
			gear.current_high_gear = best_gear;
			gear_shift_cooldown = 0.9f;
			just_shifted = true;
		} else {
			just_shifted = false;
		}
	}
	const float gear_ratio = (float)gear.front_cogs[gear.current_low_gear]
	                       / (float)gear.back_cogs[gear.current_high_gear];
	cadence = (speed > 0.1f) ? (speed / (gear_ratio * wheel_circ)) : 0.f; // rev/s

	// --- Move entity (flat XZ, terrain will correct Y) ---
	glm::vec3 pos = get_owner()->get_ws_position();
	pos += bike_direction * speed * dt;

	// --- Terrain raycasts (front + rear wheel) ---
	// Cast from above the current position downward to find ground contact.
	const float wheel_half     = 0.55f;  // half-wheelbase, m
	const float ray_up_offset  = 1.5f;  // start ray this far above pos
	const float ray_reach      = 4.0f;  // cast this far downward
	const float seat_height    = 0.3f;  // entity origin to ground

	const glm::vec3 front_org = pos + bike_direction * wheel_half + glm::vec3(0, ray_up_offset, 0);
	const glm::vec3 rear_org  = pos - bike_direction * wheel_half + glm::vec3(0, ray_up_offset, 0);
	const glm::vec3 down      = glm::vec3(0, -(ray_up_offset + ray_reach), 0);

	world_query_result front_res, rear_res;
	const bool front_hit = g_physics.trace_ray(front_res, front_org, front_org + down, nullptr, UINT32_MAX);
	const bool rear_hit  = g_physics.trace_ray(rear_res,  rear_org,  rear_org  + down, nullptr, UINT32_MAX);

	// terrain_forward: direction along slope (pitched), defaults to flat bike_direction
	glm::vec3 terrain_forward = bike_direction;

	if (front_hit && rear_hit) {
		const glm::vec3 slope_vec = front_res.hit_pos - rear_res.hit_pos;
		const float horiz = glm::length(glm::vec2(slope_vec.x, slope_vec.z));
		terrain_gradient  = atan2f(slope_vec.y, horiz); // radians, + = uphill
		terrain_forward   = glm::normalize(slope_vec);
		pos.y = (front_res.hit_pos.y + rear_res.hit_pos.y) * 0.5f + seat_height;
	} else if (rear_hit) {
		pos.y = rear_res.hit_pos.y + seat_height;
		terrain_gradient = 0.f;
	} else if (front_hit) {
		pos.y = front_res.hit_pos.y + seat_height;
		terrain_gradient = 0.f;
	} else {
		terrain_gradient = 0.f; // no ground found
	}

	// --- Orientation: steer direction (flat) + terrain pitch + roll ---
	// right is derived from flat bike_direction so steering stays independent of slope
	const glm::vec3 right        = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
	const glm::vec3 terrain_up   = glm::normalize(glm::cross(right, terrain_forward));
	const glm::mat3 basis(right, terrain_up, -terrain_forward);
	const glm::quat base_orient  = glm::quat(basis);
	const glm::quat roll_quat    = glm::angleAxis(current_roll, terrain_forward);
	const glm::quat orient       = roll_quat * base_orient;

	get_owner()->set_ws_position_rotation(pos, orient);
}


// ============================================================
// BikePlayer
// ============================================================

BikePlayer::BikePlayer()
{
	auto* cc_entity = GameplayStatic::spawn_entity();
	auto* cc_comp   = cc_entity->create_component<CameraComponent>();
	cc_comp->set_is_enabled(true);
	cc_comp->set_fov(65.f);
	assert(CameraComponent::get_scene_camera() == cc_comp);
	this->cc = cc_comp;

	freewheel_player = isound->register_sound_player();
	freewheel_player->asset  = SoundFile::load("sounds/free_wheel.wav");
	freewheel_player->looping = true;
	freewheel_player->attenuate = false;
	freewheel_player->spatialize = false;
	freewheel_player->volume_multiply = 0.f;
	freewheel_player->set_play(true);
}

BikePlayer::~BikePlayer()
{
	isound->remove_sound_player(freewheel_player);
}

static float bike_camera_lead = 15.f;
static float bike_camera_lead_interp = 0.02f;
static float bike_camera_brake = 8.f;
static float bike_camera_brake_fast = 0.003f;    // response on brake input
static float bike_camera_brake_slow = 0.04f;     // recovery when released
static float bike_camera_gradient = 0.6f;
static float bike_camera_gradient_fast = 0.002f; // snap up on climb
static float bike_camera_gradient_slow = 0.015f; // lazy return on descent/flat
static float bike_fov_fast = 0.003f;             // widen on acceleration
static float bike_fov_slow = 0.02f;              // slow bleed back

// Asymmetric damp: different smoothing depending on direction of change
static float damp_asymmetric(float target, float current, float fast, float slow, float dt) {
	float smoothing = (target > current) ? fast : slow;
	return damp_dt_independent(target, current, smoothing, dt);
}

// Deadzone: zero below threshold, rescale remainder to full 0..1 range (no jump at edge)
static float apply_deadzone(float val, float dz) {
	if (glm::abs(val) < dz) return 0.f;
	return glm::sign(val) * (glm::abs(val) - dz) / (1.f - dz);
}


static float sh_power_up   = 0.012f;  // smoothing toward higher power (smaller = slower)
static float sh_power_down = 0.025f;  // smoothing toward lower power
static float sh_power_max  = 800.f;   // hard cap on speed-hold output (W)

static BikeObject* bo_for_debug = nullptr;
static BikePlayer* bp_for_debug = nullptr;
void bike_object_debug() {
	if (!bp_for_debug)return;
	int power_level_idx = bp_for_debug->power_level_idx;
	bool is_coasting = bp_for_debug->is_coasting;

	auto* my_bike = bo_for_debug;
	float speed_kmh = my_bike->speed * 3.6f;
	ImGui::Text("Speed:   %.1f km/h", speed_kmh);
	if (bp_for_debug->speed_hold_active)
		ImGui::Text("Power:   (speed hold %.1f km/h)  %.0f W",
			bp_for_debug->speed_hold_target * 3.6f, bp_for_debug->speed_hold_power);
	else
		ImGui::Text("Power:   %s  %d W",
			is_coasting ? "(coast)" : "",
			is_coasting ? 0 : BIKE_POWER_LEVELS[power_level_idx]);
	ImGui::Text("Cadence: %.0f rpm", my_bike->cadence * 60.f);
	ImGui::Text("Gear:    %d / %d   [%d-%d]",
		my_bike->gear.current_low_gear + 1,
		my_bike->gear.current_high_gear + 1,
		my_bike->gear.front_cogs[my_bike->gear.current_low_gear],
		my_bike->gear.back_cogs[my_bike->gear.current_high_gear]);
	float aero_drag_N = 0.5f * 1.225f * 0.3f * (my_bike->speed * my_bike->speed);
	ImGui::Text("Aero drag: %.1f N", aero_drag_N);
	ImGui::Text("Gradient:  %.1f%%  (%.1f deg)",
		glm::tan(my_bike->terrain_gradient) * 100.f,
		glm::degrees(my_bike->terrain_gradient));
	ImGui::Text("[UP/DOWN] Power  [Hold C/B] Coast  [SPACE] Brake  [Hold V/X] Speed Hold");
	ImGui::Separator();
	ImGui::Text("Steer committed: %.3f", my_bike->current_steer);

	ImGui::SeparatorText("Speed Hold");
	{
		ImGui::DragFloat("sh_power_up",   &sh_power_up,   0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_down", &sh_power_down, 0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_max",  &sh_power_max,  10.f,   0.f,    2000.f);
	}
	ImGui::SeparatorText("Camera");
	{
#define DAMP_IMGUI(name) ImGui::DragFloat(#name,&name,0.001,0,0.2)
		ImGui::InputFloat("bike_camera_lead",          &bike_camera_lead);
		ImGui::InputFloat("bike_camera_lead_interp",   &bike_camera_lead_interp);
		ImGui::InputFloat("bike_camera_brake",         &bike_camera_brake);
		ImGui::InputFloat("bike_camera_gradient",      &bike_camera_gradient);
		ImGui::InputFloat("bike_camera_gradient_fast", &bike_camera_gradient_fast);
		ImGui::InputFloat("bike_camera_gradient_slow", &bike_camera_gradient_slow);
		ImGui::InputFloat("bike_camera_brake_fast",    &bike_camera_brake_fast);
		ImGui::InputFloat("bike_camera_brake_slow",    &bike_camera_brake_slow);
		ImGui::InputFloat("bike_fov_fast",             &bike_fov_fast);
		ImGui::InputFloat("bike_fov_slow",             &bike_fov_slow);
		DAMP_IMGUI(steer_build_lo);
		DAMP_IMGUI(steer_build_hi);
		DAMP_IMGUI(steer_release_lo);
		DAMP_IMGUI(steer_release_hi);
		ImGui::DragFloat("steer_lean_threshold", &steer_lean_threshold, 0.05);
		ImGui::DragFloat("steer_lean_range", &steer_lean_range, 0.05);
		ImGui::DragFloat("steer_speed_gate_lo", &steer_speed_gate_lo, 0.1);
		ImGui::DragFloat("steer_speed_gate_hi", &steer_speed_gate_hi, 0.1);



#undef DAMP_IMGUI
	}
}
ADD_TO_DEBUG_MENU(bike_object_debug);

void BikePlayer::evaluate(BikeObject* my_bike)
{
	const float dt = eng->get_dt();

	// --- Gamepad input (with deadzone rescaling) ---
	const float steer        = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX),  0.15f);
	const float brake_amount = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), 0.05f);

	const bool power_up      = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down    = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool coast_btn     = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_B);
	const bool speed_hold_btn = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	// Keyboard fallback (useful when no controller connected)
	const bool kb_left  = Input::is_key_down(SDL_SCANCODE_A) || Input::is_key_down(SDL_SCANCODE_LEFT);
	const bool kb_right = Input::is_key_down(SDL_SCANCODE_D) || Input::is_key_down(SDL_SCANCODE_RIGHT);
	const bool kb_brake = Input::is_key_down(SDL_SCANCODE_SPACE);
	const bool kb_up    = Input::was_key_pressed(SDL_SCANCODE_UP);
	const bool kb_down  = Input::was_key_pressed(SDL_SCANCODE_DOWN);
	const bool kb_coast = Input::is_key_down(SDL_SCANCODE_C);
	const bool kb_speed_hold = Input::is_key_down(SDL_SCANCODE_V);

	// --- Power level stepping ---
	if (power_up   || kb_up)   power_level_idx = std::min(power_level_idx + 1, BIKE_NUM_POWER_LEVELS - 1);
	if (power_down || kb_down) power_level_idx = std::max(power_level_idx - 1, 0);
	is_coasting = coast_btn || kb_coast;

	// --- Speed hold ---
	const bool want_speed_hold = (speed_hold_btn || kb_speed_hold) && !is_coasting;
	if (want_speed_hold && !speed_hold_active) {
		// Just engaged — latch current speed and seed power to current level so
		// there's no sudden jump.
		speed_hold_active = true;
		speed_hold_target = my_bike->speed;
		speed_hold_power  = (float)BIKE_POWER_LEVELS[power_level_idx];
	} else if (!want_speed_hold) {
		speed_hold_active = false;
	}

	// --- Combine steer (controller + keyboard) ---
	float steer_combined = steer;
	if (kb_left)  steer_combined -= 1.f;
	if (kb_right) steer_combined += 1.f;
	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);

	// --- Fill ControlInput ---
	BikeObject::ControlInput ci;
	ci.steer        = steer_combined;
	ci.brake_amount = kb_brake ? 1.f : brake_amount;

	if (speed_hold_active) {
		// Compute the power needed to sustain speed_hold_target against current
		// resistances (using live bike state for gradient/speed).
		const float v       = glm::max(my_bike->speed, 0.3f);
		const float drag    = 0.5f * 1.225f * 0.3f * v * v;
		const float rolling = 0.004f * 83.f * 9.81f;
		const float slope   = 83.f * 9.81f * glm::sin(my_bike->terrain_gradient);
		const float ideal_power = glm::max((drag + rolling + slope) * v, 0.f);

		// Lag the power output so cadence floats a bit on sharp transitions
		// (P→increase snaps faster than decrease to feel more natural)
		const float toward = (ideal_power > speed_hold_power) ? sh_power_up : sh_power_down;
		speed_hold_power = damp_dt_independent(ideal_power, speed_hold_power, toward, dt);
		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);

		ci.power = speed_hold_power;
	} else {
		ci.power = is_coasting ? 0.f : (float)BIKE_POWER_LEVELS[power_level_idx];
	}

	my_bike->update_tick(ci);

	// --- Camera ---
	update_camera(my_bike, ci.steer, ci.brake_amount);

	// --- Sound ---
	// Freewheel loop: fade in when coasting, fade out when pedalling
	const float fw_target = ci.is_coasting() ? 1.f : 0.f;
	freewheel_player->volume_multiply = damp_dt_independent(fw_target, freewheel_player->volume_multiply, 0.005f, dt);
	// pitch scales with speed: 1.0 at ~30 km/h (8.3 m/s), linear from 0.3 at standstill
	freewheel_player->pitch_multiply = 0.3f + my_bike->speed * 0.085f;
	freewheel_player->update();

	// Gear change one-shot
	if (my_bike->just_shifted) {
		static const SoundFile* gear_snd = SoundFile::load("sounds/gear_change.wav");
		isound->play_sound(gear_snd, 0.2,1.3f, 0.f, 0.f, SndAtn::Linear, false, false, {});
	}

	bp_for_debug = this;
	bo_for_debug = my_bike;

}

void BikePlayer::update_camera(BikeObject* bike, float steer, float brake_amount)
{
	const float dt = eng->get_dt();

	const glm::vec3 bike_pos = bike->get_owner()->get_ws_position();
	const glm::vec3 fwd      = bike->bike_direction;

	const float cam_dist      = 5.5f;
	const float default_pitch = glm::radians(20.f);
	const float rider_height  = 1.1f;
	const glm::vec3 pivot     = bike_pos + glm::vec3(0, rider_height, 0);

	// --- Right stick orbital control ---
	const float rx = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX), 0.15f);
	const float ry = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY), 0.15f);
	const bool stick_active = glm::abs(rx) > 0.f || glm::abs(ry) > 0.f;
	if (stick_active) {
		camera_yaw   += rx * 2.2f * dt;
		camera_pitch += ry * 2.2f * dt;
		camera_pitch  = glm::clamp(camera_pitch, glm::radians(-10.f), glm::radians(70.f));
	} else {
		camera_yaw   = damp_dt_independent(0.f,           camera_yaw,   0.01f, dt);
		camera_pitch = damp_dt_independent(default_pitch, camera_pitch, 0.01f, dt);
	}

	// --- Gradient pitch: slowly tracks terrain slope ---
	brake_pitch = damp_asymmetric(-brake_amount * glm::radians(bike_camera_brake), brake_pitch, bike_camera_brake_slow, bike_camera_brake_fast, dt);

	// --- Brake pitch: camera tips slightly forward under braking ---
	brake_pitch = damp_asymmetric(-brake_amount * glm::radians(bike_camera_brake), brake_pitch, bike_camera_brake_slow, bike_camera_brake_fast, dt);

	// --- Corner lead: look target leads into the turn ---
	lead_yaw = damp_dt_independent(-steer * glm::radians(bike_camera_lead), lead_yaw, bike_camera_lead_interp, dt);

	// --- Orbit position ---
	const glm::quat yaw_rot     = glm::angleAxis(camera_yaw, glm::vec3(0, 1, 0));
	const glm::vec3 yawed_dir   = yaw_rot * (-fwd);
	const glm::vec3 orbit_right = glm::normalize(glm::cross(yawed_dir, glm::vec3(0, 1, 0)));
	const float total_pitch     = camera_pitch + gradient_pitch + brake_pitch;
	const glm::quat pitch_rot   = glm::angleAxis(total_pitch, orbit_right);
	const glm::vec3 orbit_dir   = glm::normalize(pitch_rot * yawed_dir);
	const glm::vec3 target_pos  = pivot + orbit_dir * cam_dist;

	// --- Look-ahead target: scales with speed, leads into corners ---
	const float look_ahead      = 3.0f + bike->speed * 0.5f;
	const glm::quat lead_rot    = glm::angleAxis(lead_yaw, glm::vec3(0, 1, 0));
	const glm::vec3 look_target = pivot + (lead_rot * fwd) * look_ahead;

	if (!camera_initialized) {
		camera_pos         = target_pos;
		smooth_aim_pos     = look_target;
		camera_initialized = true;
	}

	// Position lag: heavier at speed for cinematic weight
	const float pos_lag = 0.015f + bike->speed * 0.002f;
	camera_pos = damp_dt_independent(target_pos, camera_pos, pos_lag, dt);

	// Aim lag: lighter than position lag so the camera rotates faster than it translates
	smooth_aim_pos = damp_dt_independent(look_target, smooth_aim_pos, 0.005f, dt);

	// Speed-based FOV
	const float fov_target = 65.f + glm::clamp(bike->speed * 0.4f, 0.f, 20.f);
	fov_smoothed = damp_asymmetric(fov_target, fov_smoothed, bike_fov_fast, bike_fov_slow, dt);
	cc->set_fov(fov_smoothed);

	// Build look-at from smoothed aim
	const glm::vec3 cam_fwd   = glm::normalize(smooth_aim_pos - camera_pos);
	const glm::vec3 cam_right = glm::normalize(glm::cross(cam_fwd, glm::vec3(0, 1, 0)));
	const glm::vec3 cam_up    = glm::cross(cam_right, cam_fwd);

	cc->get_owner()->set_ws_transform(glm::mat4(
		glm::vec4(cam_right, 0.f),
		glm::vec4(cam_up,    0.f),
		glm::vec4(-cam_fwd,  0.f),
		glm::vec4(camera_pos, 1.f)
	));
}
