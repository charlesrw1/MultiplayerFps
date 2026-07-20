#include "BikeHeaders.h"
#include "BikeObject_Local.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "imgui.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)

// ============================================================
// Steering / physics tuning vars
// Defined non-static so BikeObject.cpp and BikeObject_Physics.cpp can extern them.
// ============================================================

static float steer_speed_gate_lo   = 4.f;
static float steer_speed_gate_hi   = 14.f;
static float steer_lean_threshold  = 0.40f;
static float steer_lean_range      = 0.35f;
static float steer_build_lo        = 0.04f;
static float steer_build_hi        = 0.12f;
static float steer_vel_scale       = 4.f;
static float steer_vel_boost       = 0.01f;
static float steer_release_lo      = 0.012f;
static float steer_release_hi      = 0.05f;
float steer_max_deg         = 45.f;
float steer_max_deg_hi      = 4.f;
float steer_ref_speed       = 2.5f;
float steer_speed_power     = 2.0f;
float steer_min_radius      = 1.5f;
float steer_radius_coeff    = 0.11f;
float lean_max_deg          = 32.f;
static float lean_steer_min        = 0.12f;
static float lean_steer_full       = 0.45f;
float bar_scale_lo_steer    = 1.5f;
float bar_scale_hi_steer    = 1.0f;
float bar_visual_lean_min   = 1.5f;
float bar_lean_fade_lo      = 0.0f;
float bar_lean_fade_hi      = 1.f;

// Low-speed uphill wobble
static float wobble_speed_max    = 4.5f;   // m/s — fully active below this
static float wobble_gradient_min = 0.06f;  // rad — starts here (~3.4 deg)
static float wobble_kick         = 0.25f;  // size of each random kick to wobble_vel
static float wobble_interval     = 0.5f;   // seconds between random kicks
static float wobble_instability  = 0.8f;   // mild inverted-pendulum: wobble_steer amplifies vel
static float wobble_counter      = 4.0f;   // player steer input cancels wobble_vel
static float wobble_damping      = 0.15f;  // smoothing base for vel decay (lower = faster decay)

// Crosswind buffeting
static float crosswind_gust_kick        = 0.08f;  // steer-vel kick per gust event
static float crosswind_gust_interval_lo = 3.f;    // min seconds between gusts
static float crosswind_gust_interval_hi = 8.f;    // max seconds between gusts
static float crosswind_vel_decay        = 0.002f; // wind_vel decay (tau ~0.07 s)
static float crosswind_steer_decay      = 0.004f; // wind_steer decay (tau ~0.1 s)
static float crosswind_speed_stable     = 12.f;   // m/s — gyro stability 65% reduction at this speed

// Bump steer: handlebar kickback from road irregularities
static float bump_steer_thresh  = 0.003f;  // minimum total_bump before steer kick fires
static float bump_steer_kick    = 1.f;    // vel impulse (steer-frac/s) per unit excess bump
static float bump_steer_spring  = 200.f;   // stiffness — snaps back to centre
static float bump_steer_damp    = 14.f;    // damping
static float bump_steer_max     = 0.18f;   // max displacement as fraction of full steer

static BikeObject* s_steer_debug = nullptr;  // set each tick for debug menu

float compute_max_steer_rad(float speed)
{
	ASSERT(speed >= 0.f);
	const float safe_speed = glm::max(speed, steer_ref_speed);
	const float t          = glm::pow(steer_ref_speed / safe_speed, steer_speed_power);
	return glm::radians(steer_max_deg_hi + (steer_max_deg - steer_max_deg_hi) * t);
}

// ------------------------------------------------------------
// AI direct-steer path.
//
// The player path below simulates imperfect human hands: asymmetric build/
// release inertia, uphill wobble, crosswind gusts, bump-steer kicks, all
// funneled through a tan()-based bicycle-geometry conversion. Layering an AI
// controller's steer command through all of that — on top of the AI's own
// wheel-chasing PD terms — produces weave/oscillation: noise and lag that
// exist to simulate human error get amplified by a controller that recomputes
// its target every frame from a moving wheel. AI riders have steady hands, so
// they get ONE smoothing constant straight to a capped turn rate — no
// bicycle-geometry indirection, no noise layers.
// ------------------------------------------------------------
static float ai_steer_smoothing_tau = 0.12f;  // s — single low-pass, no build/release asymmetry
static float ai_max_turn_rate_dps   = 220.f;  // deg/s hard ceiling regardless of the speed-based cap

static void tick_steer_ai(BikeObject& b, const BikeObject::ControlInput& ci, float dt)
{
	b.steer_input_raw = b.is_crashed ? 0.f : ci.steer;
	b.current_steer   = damp_dt_independent(b.steer_input_raw, b.current_steer, ai_steer_smoothing_tau, dt);
	b.steer_committed = b.current_steer;

	// Speed-scaled turn-rate cap — same physical grounding as the player's
	// min-turn-radius floor (tighter turns only above a minimum radius), applied
	// directly to turn rate instead of routed through wheelbase/tan().
	const float min_turn_r = glm::max(steer_min_radius, b.speed * b.speed * steer_radius_coeff);
	const float speed_cap  = (min_turn_r > 0.01f) ? (b.speed / min_turn_r) : 0.f;
	const float max_rate   = glm::min(speed_cap, glm::radians(ai_max_turn_rate_dps));

	b.turn_rate = b.current_steer * max_rate;
	const float angle = -b.turn_rate * dt;
	b.bike_direction = glm::normalize(glm::mat3(glm::rotate(glm::mat4(1.f), angle, glm::vec3(0, 1, 0))) * b.bike_direction);

	// Visual lean — same formula as the player path, derived from the resulting turn rate.
	float target_roll = 0.f;
	if (glm::abs(b.turn_rate) > 0.001f && b.speed > 0.1f) {
		const float turn_r             = b.speed / glm::max(glm::abs(b.turn_rate), 0.001f);
		const float centripetal_accel  = (b.speed * b.speed) / turn_r;
		const float lean_speed_scale   = glm::smoothstep(0.f, 8.f, b.speed);
		const float lean_uncapped      = atanf(centripetal_accel / BIKE_GRAVITY) * lean_speed_scale;
		target_roll = glm::sign(b.current_steer) * glm::min(lean_uncapped, glm::radians(lean_max_deg));
	}
	b.current_roll = damp_dt_independent(target_roll, b.current_roll, 0.01f, dt);
}

// ------------------------------------------------------------
// BikeObject::tick_steer
//
// AI riders take tick_steer_ai() above and return immediately. The rest of
// this function (inertia, wobble, crosswind, bump steer, bicycle geometry) is
// the player path only.
//
// Resolves steering inertia, uphill wobble, crosswind gusts, and bump steer
// into current_steer.  Applies bicycle geometry to yaw bike_direction and
// compute visual lean (current_roll).
// Sign conventions: positive steer = turn LEFT. bike_right points LEFT.
// See [[bike/sign_conventions]]
// ------------------------------------------------------------
void BikeObject::tick_steer(const ControlInput& ci, float dt)
{
	ASSERT(dt > 0.f);
	s_steer_debug = this;

	if (dynamic_cast<BikeAI*>(input.get())) {
		tick_steer_ai(*this, ci, dt);
		return;
	}

	const float speed_range  = glm::max(steer_speed_gate_hi - steer_speed_gate_lo, 0.001f);
	const float speed_factor = glm::clamp((speed - steer_speed_gate_lo) / speed_range, 0.f, 1.f);

	steer_input_raw = is_crashed ? 0.f : ci.steer;

	// Build inertia from INPUT magnitude × speed — small inputs are always snappy at any speed.
	// Only large committed inputs at high speed slow down to steer_build_hi.
	const float input_depth  = glm::clamp((glm::abs(steer_input_raw) - steer_lean_threshold)
	                             / glm::max(steer_lean_range, 0.001f), 0.f, 1.f);
	const float build_inertia = speed_factor * input_depth;

	// Release inertia from CURRENT_STEER depth — committed corners exit smoothly, not snapped.
	const float release_depth  = glm::clamp((glm::abs(current_steer) - steer_lean_threshold)
	                               / glm::max(steer_lean_range, 0.001f), 0.f, 1.f);
	const float release_inertia = speed_factor * release_depth;

	// Stick velocity boost: fast flick overrides build smoothing → even snappier
	const float stick_vel       = glm::abs(steer_input_raw - prev_steer_input) / glm::max(dt, 0.001f);
	const float stick_vel_t     = glm::clamp(stick_vel / steer_vel_scale, 0.f, 1.f);
	const float base_build      = glm::mix(steer_build_lo, steer_build_hi, build_inertia);
	const float build_smoothing = glm::mix(base_build, steer_vel_boost, stick_vel_t);
	const float rel_smoothing   = glm::mix(steer_release_lo, steer_release_hi, release_inertia);
	prev_steer_input = steer_input_raw;

	if (glm::abs(steer_input_raw) > glm::abs(current_steer))
		current_steer = damp_dt_independent(steer_input_raw, current_steer, build_smoothing, dt);
	else
		current_steer = damp_dt_independent(steer_input_raw, current_steer, rel_smoothing, dt);
	steer_committed = current_steer;  // snapshot before wobble/wind/bump disturbances

	// Uphill wobble
	{
		const float spd_t      = 1.f - glm::clamp(speed / wobble_speed_max, 0.f, 1.f);
		const float grad_t     = glm::clamp((terrain_gradient - wobble_gradient_min) / 0.12f, 0.f, 1.f);
		const float intensity  = is_crashed ? 0.f : spd_t * grad_t;

		if (intensity > 0.f) {
			wobble_timer -= dt;
			if (wobble_timer <= 0.f) {
				wobble_vel  += (((float)rand() / RAND_MAX) * 2.f - 1.f) * wobble_kick * intensity;
				wobble_timer = wobble_interval * (0.6f + 0.8f * ((float)rand() / RAND_MAX));
			}
			wobble_vel += wobble_steer * wobble_instability * intensity * dt;
			wobble_vel += steer_input_raw * wobble_counter * intensity * dt;
		}

		const float vel_decay   = glm::mix(0.001f, wobble_damping, intensity);
		const float steer_decay = glm::mix(0.001f, 0.5f, intensity);
		wobble_vel   = damp_dt_independent(0.f, wobble_vel,   vel_decay,   dt);
		wobble_steer = damp_dt_independent(0.f, wobble_steer, steer_decay, dt);
		wobble_steer = glm::clamp(wobble_steer + wobble_vel * dt, -0.6f, 0.6f);
	}

	// Crosswind gusts: periodic random kicks from the wind's lateral component.
	// wind_gust_factor is too biased/slow to use directly — instead fire discrete
	// gust events on a random timer, like wobble kicks.
	{
		crosswind_gust_timer -= dt;
		if (crosswind_gust_timer <= 0.f) {
			const glm::vec3 wdir    = glm::length(g_wind.wind_direction) > 0.001f
			                          ? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
			// bike_right = cross(bike_direction, up) — NOTE: points LEFT in world space (misnamed).
			// dot against it gives perpendicular wind component; sign determines kick direction.
			const glm::vec3 b_right = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
			const float perp_component = glm::dot(wdir, b_right);

			const float speed_stab = 1.f - glm::clamp(speed / crosswind_speed_stable, 0.f, 1.f) * 0.65f;
			const float kick       = crosswind_gust_kick * g_wind.wind_speed * speed_stab
			                         * (0.5f + 0.5f * ((float)rand() / RAND_MAX));
			wind_vel += perp_component * kick;

			const float interval = crosswind_gust_interval_lo
			    + (crosswind_gust_interval_hi - crosswind_gust_interval_lo)
			      * ((float)rand() / RAND_MAX);
			crosswind_gust_timer = interval;
		}
		wind_vel   = damp_dt_independent(0.f, wind_vel,   crosswind_vel_decay,   dt);
		wind_steer = glm::clamp(wind_steer + wind_vel * dt, -0.15f, 0.15f);
		wind_steer = damp_dt_independent(0.f, wind_steer, crosswind_steer_decay, dt);
	}

	// Bump steer: crack decals kick the handlebar with a random-signed impulse
	{
		const float excess_bump = glm::max(0.f, crack_impulse - bump_steer_thresh);
		if (excess_bump > 0.f) {
			const float kick_dir = (rand() & 1) ? 1.f : -1.f;
			bump_steer_vel += kick_dir * excess_bump * bump_steer_kick;
		}
		bump_steer_vel  += (-bump_steer_spring * bump_steer_disp - bump_steer_damp * bump_steer_vel) * dt;
		bump_steer_disp += bump_steer_vel * dt;
		bump_steer_disp  = glm::clamp(bump_steer_disp, -bump_steer_max, bump_steer_max);
	}

	current_steer = glm::clamp(current_steer + wobble_steer + wind_steer + bump_steer_disp, -1.f, 1.f);

	// Steering geometry → yaw bike_direction
	// Positive steer = turn LEFT (see [[bike/sign_conventions]])
	const float wheelbase     = BIKE_WHEELBASE;
	const float max_steer_rad = compute_max_steer_rad(speed);
	const float min_turn_r    = glm::max(steer_min_radius, speed * speed * steer_radius_coeff);
	const float steer_angle   = current_steer * max_steer_rad;

	turn_rate = 0.f;
	if (glm::abs(steer_angle) > 0.001f) {
		const float radius = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_r);
		turn_rate          = (speed / radius) * glm::sign(current_steer);
		const float angle  = -turn_rate * dt;
		bike_direction = glm::normalize(glm::mat3(glm::rotate(glm::mat4(1.f), angle, glm::vec3(0,1,0))) * bike_direction);
	}

	// Visual lean
	// lean_scale: smoothstep ramp so tiny corrections produce no lean;
	// only real committed turns tilt the bike.
	float target_roll = 0.f;
	if (glm::abs(steer_angle) > 0.001f && speed > 0.1f) {
		const float turn_r            = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_r);
		const float centripetal_accel = (speed * speed) / turn_r;
		const float lean_speed_scale  = glm::smoothstep(0.f, 8.f, speed);
		const float lean_uncapped     = atanf(centripetal_accel / BIKE_GRAVITY) * lean_speed_scale;
		const float lean_scale        = glm::smoothstep(lean_steer_min, lean_steer_full, glm::abs(current_steer));
		target_roll = glm::sign(current_steer) * glm::min(lean_uncapped, glm::radians(lean_max_deg)) * lean_scale;
	}
	current_roll = damp_dt_independent(target_roll, current_roll, 0.01f, dt);
}

// ============================================================
// Debug menu: Steering / Wobble / Crosswind / Bump Steer
// ============================================================

static void bike_steer_debug()
{
	ImGui::SeparatorText("Steering");
	{
#define DAMP_IMGUI(name) ImGui::DragFloat(#name, &name, 0.001f, 0.f, 0.2f)
		DAMP_IMGUI(steer_build_lo);
		DAMP_IMGUI(steer_build_hi);
		DAMP_IMGUI(steer_release_lo);
		DAMP_IMGUI(steer_release_hi);
#undef DAMP_IMGUI
		ImGui::DragFloat("steer_lean_threshold", &steer_lean_threshold, 0.02f, 0.f, 1.f);
		ImGui::Text("  input < threshold -> snappy at any speed");
		ImGui::DragFloat("steer_lean_range",     &steer_lean_range,     0.02f, 0.f, 1.f);
		ImGui::DragFloat("steer_speed_gate_lo",  &steer_speed_gate_lo,  0.1f);
		ImGui::DragFloat("steer_speed_gate_hi",  &steer_speed_gate_hi,  0.1f);
		ImGui::DragFloat("steer_max_deg",        &steer_max_deg,        0.5f, 5.f,  45.f);
		ImGui::DragFloat("steer_max_deg_hi",     &steer_max_deg_hi,     0.5f, 0.5f, 15.f);
		ImGui::DragFloat("steer_ref_speed",      &steer_ref_speed,      0.1f, 1.f,  15.f);
		ImGui::DragFloat("steer_speed_power",    &steer_speed_power,    0.1f, 0.5f,  5.f);
		ImGui::Text("  power=1: linear  2: car-like v^2  3: cubic");
		if (s_steer_debug)
			ImGui::Text("current max steer: %.1f deg  (speed %.1f m/s)",
			            glm::degrees(compute_max_steer_rad(s_steer_debug->speed)), s_steer_debug->speed);
		ImGui::DragFloat("steer_radius_coeff",   &steer_radius_coeff,   0.005f, 0.f,   0.2f);
		ImGui::DragFloat("lean_max_deg",         &lean_max_deg,         0.5f,   5.f,  50.f);
		ImGui::DragFloat("lean_steer_min",       &lean_steer_min,       0.01f,  0.f,   0.5f);
		ImGui::DragFloat("lean_steer_full",      &lean_steer_full,      0.01f,  0.1f,  1.0f);
		if (s_steer_debug)
			ImGui::Text("  lean_scale = %.2f  (current_steer=%.3f)",
				glm::smoothstep(lean_steer_min, lean_steer_full, glm::abs(s_steer_debug->current_steer)),
				s_steer_debug->current_steer);
		ImGui::DragFloat("steer_vel_scale",      &steer_vel_scale,      0.1f,   0.5f, 20.f);
		ImGui::DragFloat("steer_vel_boost",      &steer_vel_boost,      0.002f, 0.f,  0.1f);
		ImGui::SeparatorText("Handlebar Visual");
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
		if (s_steer_debug) {
			const float sa    = glm::abs(s_steer_debug->current_steer);
			const float ss    = glm::mix(bar_scale_lo_steer, bar_scale_hi_steer, sa);
			const float lf    = glm::clamp(glm::abs(s_steer_debug->current_roll) / glm::radians(lean_max_deg), 0.f, 1.f);
			const float bscl  = glm::mix(ss, bar_visual_lean_min, glm::smoothstep(bar_lean_fade_lo, bar_lean_fade_hi, lf));
			ImGui::Text("  steer=%.2f steer_scale=%.2f  lean_frac=%.2f  bar_scale=%.2f", sa, ss, lf, bscl);
		}
	}
	ImGui::SeparatorText("Uphill Wobble");
	{
		ImGui::DragFloat("wobble_speed_max",    &wobble_speed_max,    0.1f,   0.f,  10.f);
		ImGui::DragFloat("wobble_gradient_min", &wobble_gradient_min, 0.005f, 0.f,   0.3f);
		ImGui::DragFloat("wobble_kick",         &wobble_kick,         0.01f,  0.f,   1.f);
		ImGui::DragFloat("wobble_interval",     &wobble_interval,     0.05f,  0.1f,  3.f);
		ImGui::DragFloat("wobble_instability",  &wobble_instability,  0.05f,  0.f,   5.f);
		ImGui::DragFloat("wobble_counter",      &wobble_counter,      0.1f,   0.f,  10.f);
		ImGui::DragFloat("wobble_damping",      &wobble_damping,      0.01f,  0.f,   1.f);
		if (s_steer_debug)
			ImGui::Text("wobble: steer=%.3f vel=%.3f timer=%.2f",
				s_steer_debug->wobble_steer, s_steer_debug->wobble_vel, s_steer_debug->wobble_timer);
	}
	ImGui::SeparatorText("Crosswind Gusts");
	{
		ImGui::DragFloat("crosswind_gust_kick",         &crosswind_gust_kick,         0.005f, 0.f,  0.5f);
		ImGui::DragFloat2("crosswind_interval lo/hi",   &crosswind_gust_interval_lo,  0.5f,   0.5f, 30.f);
		ImGui::DragFloat("crosswind_vel_decay",         &crosswind_vel_decay,         0.0005f, 0.f,  0.02f);
		ImGui::DragFloat("crosswind_steer_decay",       &crosswind_steer_decay,       0.001f, 0.f,  0.02f);
		ImGui::DragFloat("crosswind_speed_stable",      &crosswind_speed_stable,      0.5f,   1.f,  30.f);
		if (s_steer_debug)
			ImGui::Text("wind_steer=%.3f  vel=%.3f  gust_t=%.1f",
				s_steer_debug->wind_steer, s_steer_debug->wind_vel, s_steer_debug->crosswind_gust_timer);
	}
	ImGui::SeparatorText("Bump Steer");
	{
		ImGui::DragFloat("bump_steer_thresh",  &bump_steer_thresh,  0.0005f, 0.f,   0.05f);
		ImGui::DragFloat("bump_steer_kick",    &bump_steer_kick,    1.f,     0.f,   200.f);
		ImGui::DragFloat("bump_steer_spring",  &bump_steer_spring,  5.f,     10.f,  500.f);
		ImGui::DragFloat("bump_steer_damp",    &bump_steer_damp,    0.5f,    0.f,   40.f);
		ImGui::DragFloat("bump_steer_max",     &bump_steer_max,     0.005f,  0.f,   0.5f);
		if (s_steer_debug)
			ImGui::Text("disp=%.3f  vel=%.2f", s_steer_debug->bump_steer_disp, s_steer_debug->bump_steer_vel);
	}
}
ADD_TO_DEBUG_MENU(bike_steer_debug);
