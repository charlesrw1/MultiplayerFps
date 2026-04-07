#include "BikeHeaders.h"

#include "Game/GameplayStatic.h"
#include "Game/Components/PhysicsComponents.h"
#include "Game/Entities/CharacterController.h"
#include "Input/InputSystem.h"
#include "GameEnginePublic.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"
#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "UI/Gui.h"
#include <algorithm>

// Shared wind state (defined in BikeWind.cpp)
extern glm::vec3 wind_direction;
extern float     wind_speed;
extern float     wind_gust_factor;
extern float     gust_speed_amp;

// Speed hold tuning
static float sh_power_up   = 0.012f;
static float sh_power_down = 0.025f;
static float sh_power_max  = 800.f;

// Freewheel sound fade
static float free_wheel_fade = 0.0002f;

// Debug pointers (set each frame in evaluate)
static BikeObject* bo_for_debug = nullptr;
static BikePlayer* bp_for_debug = nullptr;

// ============================================================
// Helpers
// ============================================================

static float apply_deadzone(float val, float dz) {
	if (glm::abs(val) < dz) return 0.f;
	return glm::sign(val) * (glm::abs(val) - dz) / (1.f - dz);
}

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
// BikePlayer — constructor / destructor
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
	freewheel_player->asset          = SoundFile::load("sounds/free_wheel.wav");
	freewheel_player->looping        = true;
	freewheel_player->attenuate      = false;
	freewheel_player->spatialize     = false;
	freewheel_player->volume_multiply = 0.f;
	freewheel_player->set_play(true);

	wind_player = isound->register_sound_player();
	wind_player->asset           = SoundFile::load("sounds/wind_loop.wav");
	wind_player->looping         = true;
	wind_player->attenuate       = false;
	wind_player->spatialize      = false;
	wind_player->volume_multiply = 0.f;
	wind_player->set_play(true);

	pedal_player = isound->register_sound_player();
	pedal_player->asset           = SoundFile::load("sounds/bike_pedal.wav");
	pedal_player->looping         = true;
	pedal_player->attenuate       = false;
	pedal_player->spatialize      = false;
	pedal_player->volume_multiply = 0.f;
	pedal_player->set_play(true);

	// Speed lines particle object
	{
		speedlines_handle = idraw->get_scene()->register_particle_obj();
		Particle_Object spo{};
		spo.meshbuilder = &speedlines_mb;
		idraw->get_scene()->update_particle_obj(speedlines_handle, spo);
	}
	// Wind lines particle object
	{
		wind_handle = idraw->get_scene()->register_particle_obj();
		Particle_Object wpo{};
		wpo.meshbuilder = &wind_mb;
		idraw->get_scene()->update_particle_obj(wind_handle, wpo);
	}
}

BikePlayer::~BikePlayer()
{
	isound->remove_sound_player(freewheel_player);
	isound->remove_sound_player(wind_player);
	isound->remove_sound_player(pedal_player);
	idraw->get_scene()->remove_particle_obj(speedlines_handle);
	idraw->get_scene()->remove_particle_obj(wind_handle);
}

// ============================================================
// BikePlayer::evaluate
// ============================================================
#include "Debug.h"
void BikePlayer::evaluate(BikeObject* my_bike)
{
	const float dt = eng->get_dt();

	// --- Gamepad input ---
	const float steer        = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX), 0.15f);
	const float brake_amount = apply_deadzone((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT), 0.05f);

	const bool power_up_press   = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_press = Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool power_up_held    = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_UP);
	const bool power_down_held  = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_DPAD_DOWN);
	const bool coast_btn      = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_B);
	const bool speed_hold_btn = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	// Keyboard fallback
	const bool kb_left       = Input::is_key_down(SDL_SCANCODE_A) || Input::is_key_down(SDL_SCANCODE_LEFT);
	const bool kb_right      = Input::is_key_down(SDL_SCANCODE_D) || Input::is_key_down(SDL_SCANCODE_RIGHT);
	const bool kb_brake      = Input::is_key_down(SDL_SCANCODE_SPACE);
	const bool kb_up_press   = Input::was_key_pressed(SDL_SCANCODE_UP);
	const bool kb_down_press = Input::was_key_pressed(SDL_SCANCODE_DOWN);
	const bool kb_up_held    = Input::is_key_down(SDL_SCANCODE_UP);
	const bool kb_down_held  = Input::is_key_down(SDL_SCANCODE_DOWN);
	const bool kb_coast      = Input::is_key_down(SDL_SCANCODE_C);
	const bool kb_speed_hold = Input::is_key_down(SDL_SCANCODE_V);

	// --- Power level stepping: tap = 1 step, hold = rapid repeat after delay ---
	constexpr float POWER_HOLD_DELAY    = 0.2f;  // seconds before repeat starts
	constexpr float POWER_REPEAT_RATE   = 0.04f; // seconds per repeat step

	auto step_power = [&](int dir) {
		power_level_idx = glm::clamp(power_level_idx + dir, 0, BIKE_NUM_POWER_LEVELS - 1);
	};

	if (power_up_press   || kb_up_press)   step_power(+1);
	if (power_down_press || kb_down_press) step_power(-1);

	const int held_dir = (power_up_held || kb_up_held) ? 1 : (power_down_held || kb_down_held) ? -1 : 0;
	if (held_dir != 0) {
		power_hold_timer += dt;
		if (power_hold_timer >= POWER_HOLD_DELAY) {
			power_repeat_timer += dt;
			while (power_repeat_timer >= POWER_REPEAT_RATE) {
				step_power(held_dir);
				power_repeat_timer -= POWER_REPEAT_RATE;
			}
		}
	} else {
		power_hold_timer   = 0.f;
		power_repeat_timer = 0.f;
	}
	is_coasting = coast_btn || kb_coast;

	// --- Speed hold ---
	const bool want_speed_hold = (speed_hold_btn || kb_speed_hold) && !is_coasting;
	if (want_speed_hold && !speed_hold_active) {
		speed_hold_active = true;
		speed_hold_target = my_bike->speed;
		speed_hold_power  = (float)BIKE_POWER_LEVELS[power_level_idx];
	} else if (!want_speed_hold) {
		speed_hold_active = false;
	}

	// --- Combine steer ---
	float steer_combined = steer;
	if (kb_left)  steer_combined -= 1.f;
	if (kb_right) steer_combined += 1.f;
	steer_combined = glm::clamp(steer_combined, -1.f, 1.f);

	// --- Fill ControlInput ---
	BikeObject::ControlInput ci;
	ci.steer        = steer_combined;
	ci.brake_amount = kb_brake ? 1.f : brake_amount;

	if (speed_hold_active) {
		const float v            = glm::max(my_bike->speed, 0.3f);
		const float eff_wind_spd = wind_speed * (1.f + wind_gust_factor * 1.5f);
		const glm::vec3 wdir_n   = glm::length(wind_direction) > 0.001f
		                           ? glm::normalize(wind_direction) : glm::vec3(0.f);
		const float wind_along   = glm::dot(wdir_n, my_bike->bike_direction) * eff_wind_spd;
		const float app_speed    = my_bike->speed - wind_along;
		const float drag         = 0.5f * 1.225f * 0.3f * app_speed * glm::abs(app_speed);
		const float rolling      = 0.004f * 83.f * 9.81f;
		const float slope        = 83.f * 9.81f * glm::sin(my_bike->terrain_gradient);
		const float ideal_power  = glm::max((drag + rolling + slope) * v, 0.f);

		const float toward = (ideal_power > speed_hold_power) ? sh_power_up : sh_power_down;
		speed_hold_power = damp_dt_independent(ideal_power, speed_hold_power, toward, dt);
		speed_hold_power = glm::clamp(speed_hold_power, 0.f, sh_power_max);

		ci.power = speed_hold_power;
	} else {
		ci.power = is_coasting ? 0.f : (float)BIKE_POWER_LEVELS[power_level_idx];
	}

	current_power = ci.power;
	my_bike->update_tick(ci);

	// --- Wind ---
	update_wind(my_bike);

	// --- Camera ---
	update_camera(my_bike, ci.steer, ci.brake_amount);

	// --- Sound: freewheel ---
	const float fw_target = ci.is_coasting() ? 1.f : 0.f;
	freewheel_player->volume_multiply = damp_dt_independent(fw_target, freewheel_player->volume_multiply, free_wheel_fade, dt);
	freewheel_player->pitch_multiply  = 0.8f + my_bike->speed * 0.04f;
	freewheel_player->update();

	// --- Sound: pedalling ---
	// Audible when power is applied; pitch tracks cadence (rev/s → semitone-linear scale).
	// Cadence of 1.5 rev/s (target ~90 RPM) maps to pitch 1.0.
	{
		float pedal_vol = ci.is_coasting() ? 0.f : 1.f;
		pedal_vol *= glm::clamp(my_bike->cadence / 0.9f, 0.f, 1.f) * 0.35f;
		pedal_player->volume_multiply = damp_dt_independent(pedal_vol, pedal_player->volume_multiply, 0.03f, dt);
		pedal_player->pitch_multiply = map_range(my_bike->cadence * 90.f, 70.f, 110.f, 0.95, 1.05);
		pedal_player->update();
	}

	// --- Sound: wind ambient ---
	{
		const float eff_spd = -my_bike->get_wind_along_bike();// wind_speed* (1.f + wind_gust_factor * gust_speed_amp);
		const float vol_target = glm::clamp(eff_spd / 10.f, 0.f, 1.f)*0.8 + 0.4;
		wind_player->volume_multiply = damp_dt_independent(vol_target, wind_player->volume_multiply, 0.04f, dt);
		wind_player->pitch_multiply  = 0.75f + vol_target * 0.5f;
		wind_player->spatialize = true;
		auto wind_dir = my_bike->get_wind_along_bike_vector();
		//Debug::add_line(my_bike->get_ws_position(), my_bike->get_ws_position() + wind_dir, COLOR_WHITE, -1);
		wind_player->spatial_pos = cc->get_ws_position() - wind_dir;

		wind_player->update();
	}

	// --- Sound: gear change one-shot ---
	if (my_bike->just_shifted) {
		static const SoundFile* gear_snd = SoundFile::load("sounds/gear_change.wav");
		isound->play_sound(gear_snd, 0.2, 1.2f, 0.f, 0.f, SndAtn::Linear, false, false, {});
	}

	draw_power_meter(current_power, power_level_idx, is_coasting, speed_hold_active, speed_hold_power);

	bp_for_debug = this;
	bo_for_debug = my_bike;
}

// ============================================================
// Power-zone meter HUD (top-left)
// ============================================================

void BikePlayer::draw_power_meter(float current_watts, int power_idx, bool coasting, bool speed_hold, float speed_hold_watts)
{
	struct ZoneColor {
		int   w_min;
		float r_lit, g_lit, b_lit;
		float r_dim, g_dim, b_dim;
	};
	// Zone boundaries and their hue colors (lit / darkened)
	static const ZoneColor zone_colors[] = {
		{   0,  0.16f, 0.31f, 0.86f,  0.04f, 0.09f, 0.25f },  // blue
		{ 150,  0.16f, 0.73f, 0.24f,  0.04f, 0.20f, 0.07f },  // green
		{ 250,  0.88f, 0.80f, 0.14f,  0.25f, 0.22f, 0.04f },  // yellow
		{ 350,  0.88f, 0.43f, 0.08f,  0.25f, 0.12f, 0.02f },  // orange
		{ 500,  0.80f, 0.12f, 0.12f,  0.22f, 0.03f, 0.03f },  // red
	};
	constexpr int NUM_ZONES = 5;

	// Resolve zone color for a given wattage
	auto zone_for_watts = [&](int w) -> const ZoneColor& {
		int z = 0;
		for (int i = 1; i < NUM_ZONES; ++i)
			if (w >= zone_colors[i].w_min) z = i;
		return zone_colors[z];
	};

	constexpr int BOX_H  = 10;
	constexpr int BOX_W  = 38;
	constexpr int GAP    = 2;
	constexpr int X      = 12;
	constexpr int Y      = 12;

	// Draw highest level at top, lowest at bottom — iterate reversed
	for (int i = BIKE_NUM_POWER_LEVELS - 1; i >= 0; --i) {
		const int y   = Y + (BIKE_NUM_POWER_LEVELS - 1 - i) * (BOX_H + GAP);
		const bool lit = !coasting && i <= power_idx;
		const ZoneColor& zc = zone_for_watts(BIKE_POWER_LEVELS[i]);

		if (lit)
			Gui::set_color(zc.r_lit, zc.g_lit, zc.b_lit, 0.92f);
		else
			Gui::set_color(zc.r_dim, zc.g_dim, zc.b_dim, 0.75f);
		Gui::rectangle(X, y, BOX_W, BOX_H);

		Gui::set_color(1.f, 1.f, 1.f, lit ? 0.18f : 0.07f);
		Gui::rectangle_outline(X, y, BOX_W, BOX_H, 1);
	}

	// Speed hold marker: white tick to the right of the meter at the matching box
	if (speed_hold) {
		int sh_idx = 0;
		for (int i = 0; i < BIKE_NUM_POWER_LEVELS; ++i)
			if (BIKE_POWER_LEVELS[i] <= (int)speed_hold_watts) sh_idx = i;

		const int marker_y = Y + (BIKE_NUM_POWER_LEVELS - 1 - sh_idx) * (BOX_H + GAP);
		constexpr int TICK_X = X + BOX_W + 3;
		constexpr int TICK_W = 6;
		Gui::set_color(1.f, 1.f, 1.f, 0.9f);
		Gui::rectangle(TICK_X, marker_y, TICK_W, BOX_H);
	}

	// Watt readout below the meter
	const int label_y = Y + BIKE_NUM_POWER_LEVELS * (BOX_H + GAP) + 2;
	Gui::set_color(0.f, 0.f, 0.f, 1.f);
	Gui::print(string_format("%.0fW", current_watts), X + 1, label_y + 1);
	Gui::set_color(1.f, 1.f, 1.f, 1.f);
	Gui::print(string_format("%.0fW", current_watts), X, label_y);
}

// ============================================================
// Debug menu: Bike Status
// ============================================================

static void bike_status_debug()
{
	if (!bp_for_debug) return;
	const int power_level_idx = bp_for_debug->power_level_idx;
	const bool is_coasting    = bp_for_debug->is_coasting;
	auto* my_bike = bo_for_debug;

	const float speed_kmh = my_bike->speed * 3.6f;
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
	const float eff_ws       = wind_speed * (1.f + wind_gust_factor * 1.5f);
	const glm::vec3 wdn      = glm::length(wind_direction) > 0.001f ? glm::normalize(wind_direction) : glm::vec3(0.f);
	const float wind_comp    = glm::dot(wdn, my_bike->bike_direction) * eff_ws;
	const float app_spd      = my_bike->speed - wind_comp;
	const float aero_drag_N  = 0.5f * 1.225f * 0.3f * app_spd * glm::abs(app_spd);
	ImGui::Text("Aero drag: %.1f N  (app wind %.1f m/s, %+.1f along)", aero_drag_N, eff_ws, wind_comp);
	ImGui::Text("Gradient:  %.1f%%  (%.1f deg)",
		glm::tan(my_bike->terrain_gradient) * 100.f,
		glm::degrees(my_bike->terrain_gradient));
	ImGui::Text("Steer committed: %.3f", my_bike->current_steer);
	ImGui::Text("[UP/DOWN] Power  [C/B] Coast  [SPACE] Brake  [V/X] Speed Hold");

	ImGui::SeparatorText("Speed Hold");
	{
		ImGui::DragFloat("sh_power_up",   &sh_power_up,   0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_down", &sh_power_down, 0.001f, 0.001f, 1.f);
		ImGui::DragFloat("sh_power_max",  &sh_power_max,  10.f,   0.f,   2000.f);
	}
	ImGui::SeparatorText("Sound");
	{
		ImGui::InputFloat("free_wheel_fade", &free_wheel_fade);
	}
}
ADD_TO_DEBUG_MENU(bike_status_debug);
