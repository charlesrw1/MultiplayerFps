#include "BikeHeaders.h"
#include "Debug.h"

#include "Game/GameplayStatic.h"

// Wind state accessed via g_wind (defined in BikeWind.cpp)
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Framework/MathLib.h"
#include "Framework/Config.h"
#include "Physics/Physics2.h"
#include "imgui.h"
#include <cfloat>

// ============================================================
// Physics constants
// ============================================================
static constexpr float BIKE_MASS        = 83.f;
static constexpr float BIKE_WHEEL_CIRC  = 2.1f;
static constexpr float BIKE_CDA         = 0.3f;
static constexpr float BIKE_AIR_DENSITY = 1.225f;
static constexpr float BIKE_ROLL_RESIST = 0.004f;
static constexpr float BIKE_GRAVITY     = 9.81f;
static  float BIKE_MAX_BRAKE   = 1.8f;
static constexpr float BIKE_REAR_Z      = -0.449f;
static constexpr float BIKE_FRONT_Z     =  0.5394f;
static constexpr float BIKE_WHEELBASE   = BIKE_FRONT_Z - BIKE_REAR_Z; // ~0.9884

// ============================================================
// Steering / physics tuning vars
// ============================================================

static float steer_speed_gate_lo   = 4.f;    // m/s — speed_factor = 0 below this
static float steer_speed_gate_hi   = 14.f;   // m/s — speed_factor = 1 above this
static float steer_lean_threshold  = 0.40f;  // input fraction at which committed-turn inertia starts
static float steer_lean_range      = 0.35f;  // input range over which inertia ramps to full
static float steer_build_lo        = 0.04f;  // build smoothing for micro/small inputs (snappy)
static float steer_build_hi        = 0.12f;  // build smoothing for large committed inputs at high speed
static float steer_vel_scale       = 4.f;    // stick units/s at which velocity boost is fully applied
static float steer_vel_boost       = 0.01f;  // smoothing floor when flicking fast (lower = snappier)
static float steer_release_lo      = 0.012f; // release smoothing when not committed
static float steer_release_hi      = 0.05f;  // release smoothing when deeply committed (gradual corner exit)
static float steer_max_deg         = 45.f;   // max steer angle at/below reference speed (degrees)
static float steer_max_deg_hi      = 4.f;    // minimum steer angle floor at very high speed (degrees)
static float steer_ref_speed       = 2.5f;    // m/s — speed at which full steer authority begins decaying
static float steer_speed_power     = 2.0f;   // falloff exponent: 2 = quadratic (car-like), 1 = linear
static float steer_min_radius      = 1.5f;   // minimum turn radius (m)
static float steer_radius_coeff    = 0.11f;  // speed² coefficient for min radius (lower = more responsive at speed)
static float lean_max_deg          = 32.f;   // visual lean cap (degrees) — prevents extreme angles
static float lean_steer_min        = 0.12f;  // steer fraction below which there is zero lean
static float lean_steer_full       = 0.45f;  // steer fraction at which lean reaches its full physics value
static float bar_scale_lo_steer    = 1.5f;   // handlebar visual amplifier at near-zero steer
static float bar_scale_hi_steer    = 1.0f;   // handlebar visual amplifier at full steer input
static float bar_visual_lean_min   = 1.5f;  // residual bar scale at full lean
static float bar_lean_fade_lo      = 0.0f;  // lean fraction where bar fade begins [0,1]
static float bar_lean_fade_hi      = 1.f;  // lean fraction where bar is at minimum [0,1]
static float bike_gear_shift_cooldown = 3.f;

// Returns the maximum steer angle (radians) at the given speed.
// Uses a speed^power falloff so authority shrinks as (ref_speed/speed)^power —
// same relationship car racing games use to make high-speed steering feel less twitchy.
static float compute_max_steer_rad(float speed)
{
	const float safe_speed = glm::max(speed, steer_ref_speed);
	const float t          = glm::pow(steer_ref_speed / safe_speed, steer_speed_power);
	return glm::radians(steer_max_deg_hi + (steer_max_deg - steer_max_deg_hi) * t);
}

// Crack visual pitch spring (cosmetic only)
static float crack_vis_pitch_impulse = 5;    // rad/s applied per unit crack_impulse
static float crack_vis_spring        = 340;  // spring constant
static float crack_vis_damp          = 10.f; // damping

// Bump steer: handlebar kickback from road irregularities
static float bump_steer_thresh  = 0.003f;  // minimum total_bump before steer kick fires
static float bump_steer_kick    = 1.f;    // vel impulse (steer-frac/s) per unit excess bump
static float bump_steer_spring  = 200.f;   // stiffness — snaps back to centre
static float bump_steer_damp    = 14.f;    // damping
static float bump_steer_max     = 0.18f;   // max displacement as fraction of full steer

// Bump speed / power loss
static float bump_speed_thresh  = 0.003f;  // minimum total_bump before speed is shed
static float bump_speed_loss    = 0.06f;   // m/s lost per unit excess bump (direct KE loss)
static float bump_rolling_scale = 12.f;    // extra resistive force (N) per unit bump — raises effective Crr

// Pedal stroke
static float stroke_amp = 1.0f;  // 0=constant power, 1=full realistic lurch (power only on downstroke)

// Low-speed uphill wobble
static float wobble_speed_max    = 4.5f;   // m/s — fully active below this
static float wobble_gradient_min = 0.06f;  // rad — starts here (~3.4 deg)
static float wobble_kick         = 0.25f;  // size of each random kick to wobble_vel
static float wobble_interval     = 0.5f;   // seconds between random kicks
static float wobble_instability  = 0.8f;   // mild inverted-pendulum: wobble_steer amplifies vel
static float wobble_counter      = 4.0f;   // player steer input cancels wobble_vel
static float wobble_damping      = 0.15f;  // smoothing base for vel decay (lower = faster decay)

// Crosswind buffeting
static float crosswind_gust_kick       = 0.08f;  // steer-vel kick per gust event
static float crosswind_gust_interval_lo = 3.f;   // min seconds between gusts
static float crosswind_gust_interval_hi = 8.f;   // max seconds between gusts
static float crosswind_vel_decay       = 0.002f; // wind_vel decay (tau ~0.07 s)
static float crosswind_steer_decay     = 0.004f; // wind_steer decay (tau ~0.1 s)
static float crosswind_speed_stable    = 12.f;   // m/s — gyro stability 65% reduction at this speed

// Traction tuning
static float traction_mu           = 1.0f;   // dry-road tire µ (was 0.8 — lean compensation justifies raising)
static float traction_lean_comp    = 0.20f;  // bicycle lean handles most cornering; ~0.2 of car centripetal demand reaches the tire
static float traction_build_rate   = 2.0f;   // slide builds at (slip_ratio-1) × this per second
static float traction_recover_rate = 1.5f;   // slide recovers at this per second
static BikeObject* s_bike_debug = nullptr;  // set each tick for debug menu

// ============================================================
// BikeObject
// ============================================================

void BikeObject::start()
{
	set_ticking(true);
	auto* model = get_owner()->create_component<MeshComponent>();
	model->set_model(Model::load("props/road_bike/road_bike.cmdl"));
	bike_direction = glm::vec3(0.f, 0.f, 1.f);

	auto* fork_ent = GameplayStatic::spawn_entity()->create_component<MeshComponent>();
	fork_ent->set_model(Model::load("props/road_bike/road_bike_fork.cmdl"));
	fork_ent->get_owner()->parent_to(get_owner());
	fork_ent->get_owner()->set_ls_position_rotation(
		glm::vec3(0.f, 0.747, -0.349), 
		glm::quat(glm::vec3(glm::radians(17.77f), 0.f, 0.f))
	);

	this->fork_entity = fork_ent->get_owner();
}

void BikeObject::update()
{
	s_bike_debug = this;
	if (input)
		input->evaluate(this);
}

BikeObject::ControlInput BikeObject::update_tick(ControlInput ci)
{
	const float dt = eng->get_dt();
	tick_stamina(ci, dt);
	tick_physics(ci, dt);
	tick_steer(ci, dt);
	tick_gears(dt);
	tick_transform(ci, dt);
	return ci;
}

// ------------------------------------------------------------
void BikeObject::tick_stamina(ControlInput& ci, float dt)
{
	StaminaState& s = stamina;
	const RiderStats& r = rider;

	// Heat stress: solar + ambient + effort heat vs. convective cooling from airspeed
	{
		const float temp_above_neutral = glm::max(0.f, g_wind.ambient_temp - 18.f);
		const float solar_heat   = g_wind.sun_exposure * 0.00025f;
		const float ambient_heat = temp_above_neutral * 0.000015f;
		const float effort_heat  = s.actual_power * 0.0000005f;
		const float air_flow     = glm::max(1.f, speed - get_wind_along_bike());
		const float cooling      = air_flow * 0.000035f;
		s.heat_stress += (solar_heat + ambient_heat + effort_heat - cooling) * dt;
		s.heat_stress  = glm::clamp(s.heat_stress, 0.f, 1.f);
	}

	// Effective FTP and power ceiling
	const float glycogen_factor = 0.55f + 0.45f * s.glycogen;
	const float heat_ftp_factor = 1.f - s.heat_stress * 0.12f;
	s.effective_ftp  = r.base_ftp * glycogen_factor * heat_ftp_factor;
	s.power_ceiling  = s.effective_ftp + (r.sprint_watts - s.effective_ftp) * (s.w_prime / r.w_prime_max);
	s.actual_power   = glm::min(ci.power, glm::max(0.f, s.power_ceiling));
	ci.power         = s.actual_power;

	// W' drain / recovery
	const float recovery_pct = 0.85f;
	if (s.actual_power > s.effective_ftp) {
		s.w_prime = glm::max(0.f, s.w_prime - (s.actual_power - s.effective_ftp) * dt);
	} else if (s.actual_power < s.effective_ftp * recovery_pct) {
		const float zone2_factor = 1.f - s.actual_power / (s.effective_ftp * recovery_pct);
		s.w_prime = glm::min(r.w_prime_max, s.w_prime + 22.f * zone2_factor * dt);
	}

	// Glycogen drain
	if (s.actual_power > 0.f) {
		const float ratio     = s.actual_power / s.effective_ftp;
		const float heat_mult = 1.f + s.heat_stress * 0.30f;
		s.glycogen = glm::max(0.f, s.glycogen - 0.0000833f * glm::pow(ratio, 1.5f) * heat_mult * dt);
	}

	// Lactate (O2-debt): accumulates above FTP, decays tau~5min
	if (s.actual_power > s.effective_ftp)
		s.lactate += (s.actual_power - s.effective_ftp) * dt;
	s.lactate = glm::min(20000.f, damp_dt_independent(0.f, s.lactate, 0.9967f, dt));
	const float lactate_hr = s.lactate * 0.002f;

	// Heart rate
	const float power_frac          = glm::clamp(s.actual_power / (s.effective_ftp * 1.15f), 0.f, 1.f);
	const float hr_target_from_power = r.hr_rest + (r.hr_max - r.hr_rest) * power_frac + s.heat_stress * 20.f;
	const float drift_target         = (1.f - s.glycogen) * 18.f * (1.f + s.heat_stress);
	s.hr_drift = damp_dt_independent(drift_target, s.hr_drift, 0.9917f, dt);
	const float hr_target = glm::max(hr_target_from_power, r.hr_rest + lactate_hr) + s.hr_drift;
	const float tau_coeff = (s.hr_current < hr_target) ? exp(-1/30.0) : exp(-1/60.0);
	s.hr_current = glm::clamp(damp_dt_independent(hr_target, s.hr_current, tau_coeff, dt), r.hr_rest, r.hr_max + 5.f);

	// HR pulse phase
	s.hr_pulse_phase += (s.hr_current / 60.f) * 6.2832f * dt;
	if (s.hr_pulse_phase > 6.2832f) s.hr_pulse_phase -= 6.2832f;
}

// ------------------------------------------------------------
void BikeObject::tick_physics(ControlInput& ci, float dt)
{
	const float safe_speed = glm::max(speed, 0.3f);

	// Pedal stroke: use heavily smoothed speed (tau~2s) to advance phase so
	// per-stroke speed fluctuations don't collapse the feedback loop.
	stroke_speed_smooth = damp_dt_independent(speed, stroke_speed_smooth, 0.368f, dt);
	const float gear_ratio_now = (float)gear.front_cogs[gear.current_low_gear]
	                           / (float)gear.back_cogs[gear.current_high_gear];
	const float stable_cadence = stroke_speed_smooth / glm::max(gear_ratio_now * BIKE_WHEEL_CIRC, 0.01f);
	stroke_phase += stable_cadence * glm::two_pi<float>() * dt;
	if (stroke_phase > glm::two_pi<float>()) stroke_phase -= glm::two_pi<float>();

	float effective_power = ci.power;
	if (stable_cadence > 0.1f) {
		const float envelope = glm::max(0.f, glm::sin(stroke_phase * 2.f));
		effective_power = ci.power * glm::mix(1.f, glm::pi<float>() * envelope, stroke_amp);
	}

	// Forces
	const float wind_along_bike  = get_wind_along_bike();
	const float apparent_speed   = speed - wind_along_bike;
	const float drag             = 0.5f * BIKE_AIR_DENSITY * (BIKE_CDA * draft_factor) * apparent_speed * glm::abs(apparent_speed);
	const float rolling          = (speed > 0.05f) ? (BIKE_ROLL_RESIST * BIKE_MASS * BIKE_GRAVITY) : 0.f;
	const float normal_force_brk = BIKE_MASS * BIKE_GRAVITY * glm::cos(terrain_gradient);
	const float kinetic_friction = traction_mu * 0.7f * normal_force_brk;
	const float braking          = glm::mix(ci.brake_amount * BIKE_MASS * BIKE_MAX_BRAKE, kinetic_friction, rear_skid);
	const float slope_force      = BIKE_MASS * BIKE_GRAVITY * glm::sin(terrain_gradient);
	const float drive_force      = effective_power / safe_speed;

	// Crash: decay speed, timer
	if (is_crashed) {
		crash_timer -= dt;
		speed = damp_dt_independent(0.f, speed, 0.015f, dt);
		if (crash_timer <= 0.f) { is_crashed = false; crash_timer = 0.f; corner_warn_timer = 0.f; }
	}

	// Crack decals raise effective rolling resistance and shed kinetic energy
	const float excess_bump     = glm::max(0.f, crack_impulse - bump_speed_thresh);
	const float bump_resistance = crack_impulse * bump_rolling_scale;  // extra resistive N

	const float net_force = is_crashed ? 0.f : (drive_force - drag - rolling - bump_resistance - braking - slope_force);
	speed = glm::max(0.f, speed + (net_force / BIKE_MASS) * dt);

	// Direct KE loss: energy absorbed by frame/body on sharp bumps
	if (excess_bump > 0.f)
		speed = glm::max(0.f, speed - excess_bump * bump_speed_loss);

	// Traction / slip
	{
		const float max_grip = surface_traction * traction_mu * normal_force_brk;

		// Rear skid from heavy braking
		const float brake_demanded = ci.brake_amount * BIKE_MASS * BIKE_MAX_BRAKE;
		const float long_slip = (max_grip > 1.f) ? brake_demanded / max_grip : 0.f;
		if (long_slip > 1.f)
			rear_skid = glm::min(1.f, rear_skid + (long_slip - 1.f) * traction_build_rate * dt);
		else
			rear_skid = glm::max(0.f, rear_skid - traction_recover_rate * dt);
		bump_impulse = glm::max(bump_impulse, rear_skid * 0.4f);

		// Corner overspeed → crash
		const float wheelbase     = BIKE_WHEELBASE;
		const float min_turn_r    = glm::max(steer_min_radius, speed * speed * steer_radius_coeff);
		const float max_steer_rad = compute_max_steer_rad(speed);
		const float steer_angle   = current_steer * max_steer_rad;
		if (!is_crashed && glm::abs(steer_angle) > 0.001f && speed > 3.f) {
			const float turn_r      = glm::max(wheelbase / glm::abs(glm::tan(steer_angle)), min_turn_r);
			const float centripetal = BIKE_MASS * speed * speed / turn_r * traction_lean_comp;
			if (centripetal > max_grip) {
				corner_warn_timer += dt;
				if (corner_warn_timer >= 0.15f) { is_crashed = true; crash_timer = 3.f; speed *= 0.1f; }
			} else {
				corner_warn_timer = glm::max(0.f, corner_warn_timer - dt * 3.f);
			}
		}
	}

	// Debug
	const float eff_wind_speed = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);
	//GameplayStatic::debug_text(string_format("wind_along_bike = %+.2f m/s (eff %.1f)", wind_along_bike, eff_wind_speed));
	//GameplayStatic::debug_text(string_format("apparent_speed = %.2f m/s", apparent_speed));
}

// ------------------------------------------------------------
void BikeObject::tick_steer(const ControlInput& ci, float dt)
{
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

	//GameplayStatic::debug_text(string_format("spd=%.1f build_i=%.2f rel_i=%.2f steer=%.3f", speed_factor, build_inertia, release_inertia, current_steer));

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
			const glm::vec3 wdir       = glm::length(g_wind.wind_direction) > 0.001f
			                             ? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
			const glm::vec3 b_right    = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
			// How perpendicular the wind is to the bike: 0 = head/tailwind, ±1 = pure crosswind.
			// This is both the direction of the kick and its scale.
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

		//GameplayStatic::debug_text(string_format("wind_steer=%.3f  gust_t=%.1f", wind_steer, crosswind_gust_timer));
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

// ------------------------------------------------------------
void BikeObject::tick_gears(float dt)
{
	speed_smoothed      = damp_dt_independent(speed, speed_smoothed, 0.04f, dt);
	gear_shift_cooldown = glm::max(0.f, gear_shift_cooldown - dt);
	just_shifted        = false;

	if (gear_shift_cooldown == 0.f) {
		int   best_gear = gear.current_high_gear;
		float best_diff = FLT_MAX;
		for (int i = 0; i < (int)gear.back_cogs.size(); i++) {
			const float ratio = (float)gear.front_cogs[gear.current_low_gear] / (float)gear.back_cogs[i];
			const float diff  = glm::abs(speed_smoothed / (ratio * BIKE_WHEEL_CIRC) - 1.5f);
			if (diff < best_diff) { best_diff = diff; best_gear = i; }
		}
		const float cur_ratio  = (float)gear.front_cogs[gear.current_low_gear]
		                       / (float)gear.back_cogs[gear.current_high_gear];
		const float cur_rpm    = (speed_smoothed > 0.1f) ? (speed_smoothed / (cur_ratio * BIKE_WHEEL_CIRC)) * 60.f : 0.f;
		if (best_gear != gear.current_high_gear && (cur_rpm < 80.f || cur_rpm > 95.f)) {
			gear.current_high_gear = best_gear;
			gear_shift_cooldown    = bike_gear_shift_cooldown;
			just_shifted           = true;
		}
	}

	const float gear_ratio = (float)gear.front_cogs[gear.current_low_gear]
	                       / (float)gear.back_cogs[gear.current_high_gear];
	cadence = (speed_smoothed > 0.1f) ? (speed_smoothed / (gear_ratio * BIKE_WHEEL_CIRC)) : 0.f;
}

// ------------------------------------------------------------
void BikeObject::tick_transform(const ControlInput& ci, float dt)
{
	// Move: advance rear contact point, reconstruct entity origin
	glm::vec3 pos = get_owner()->get_ws_position();
	{
		glm::vec3 rear_contact = pos + bike_direction * BIKE_REAR_Z;
		rear_contact += bike_direction * speed * dt;
		pos = rear_contact - bike_direction * BIKE_REAR_Z;
	}

	// Terrain raycasts
	// The fork rotates around the head-tube hinge, not the bike origin.
	// Fork entity local position is (0, 0.747, -0.349). The local Z column of
	// the bike's orientation matrix is -bike_direction, so local z=-0.349 maps
	// to +0.349 along bike_direction in world space.
	// The fork leg length (hinge to front axle) is BIKE_FRONT_Z - hinge_forward.
	// When steered, the front contact patch rotates around the hinge, not origin.
	static constexpr float FORK_HINGE_FWD = 0.349f;
	static constexpr float FORK_LEG       = BIKE_FRONT_Z - FORK_HINGE_FWD; // ~0.63884
	const float ray_up    = 1.5f;
	const float ray_reach = 4.0f;
	const float steer_max_rad_t = compute_max_steer_rad(speed);
	const float steer_angle_t   = current_steer * steer_max_rad_t;
	const glm::vec3 steered_front_dir = glm::normalize(
	    glm::mat3(glm::rotate(glm::mat4(1.f), -steer_angle_t, glm::vec3(0, 1, 0))) * bike_direction);
	const glm::vec3 hinge_pos = pos + bike_direction * FORK_HINGE_FWD;
	const glm::vec3 front_org = hinge_pos + steered_front_dir * FORK_LEG + glm::vec3(0, ray_up, 0);
	const glm::vec3 rear_org  = pos + bike_direction * BIKE_REAR_Z  + glm::vec3(0, ray_up, 0);
	const glm::vec3 down      = glm::vec3(0, -(ray_up + ray_reach), 0);

	world_query_result front_res, rear_res;
	const bool front_hit = g_physics.trace_ray(front_res, front_org, front_org + down, nullptr, UINT32_MAX);
	const bool rear_hit  = g_physics.trace_ray(rear_res,  rear_org,  rear_org  + down, nullptr, UINT32_MAX);

	// When steered, the front contact is laterally displaced from bike_direction.
	// slope_vec (front - rear) therefore has a lateral component that would corrupt:
	//   - terrain_gradient (diagonal XZ distance > forward distance → underestimates slope)
	//   - terrain_forward_dir (points sideways, misaligning pitch orientation)
	//   - orig_t for pos.y (was hardcoded to unsteered BIKE_FRONT_Z)
	// Fix: measure gradient and height interpolation along the bike's forward axis only.
	const float front_fwd = FORK_HINGE_FWD + glm::cos(steer_angle_t) * FORK_LEG;
	terrain_forward_dir = bike_direction;
	if (front_hit && rear_hit) {
		const float rise    = front_res.hit_pos.y - rear_res.hit_pos.y;
		const float horiz   = front_fwd - BIKE_REAR_Z; // forward span along bike axis
		terrain_gradient    = atan2f(rise, horiz);
		terrain_forward_dir = glm::normalize(bike_direction * horiz + glm::vec3(0, rise, 0));
		const float orig_t  = -BIKE_REAR_Z / horiz;
		pos.y = rear_res.hit_pos.y + rise * orig_t;
	} else if (rear_hit)  { pos.y = rear_res.hit_pos.y;  terrain_gradient = 0.f; }
	else if (front_hit)   { pos.y = front_res.hit_pos.y; terrain_gradient = 0.f; }
	else                  { terrain_gradient = 0.f; }

	// Wheel trail debug lines
	{
		const glm::vec3 fc = front_hit ? front_res.hit_pos
		                               : (hinge_pos + steered_front_dir * FORK_LEG);
		const glm::vec3 rc = rear_hit  ? rear_res.hit_pos  : (pos + bike_direction * BIKE_REAR_Z);
		if (wheel_history_initialized) {
			Debug::add_line(prev_front_wheel_pos, fc, COLOR_CYAN,  3.f, false);
			Debug::add_line(prev_rear_wheel_pos,  rc, COLOR_GREEN, 3.f, false);
			//Debug::add_sphere(front_res.hit_pos, 0.5f, COLOR_RED, -1);
		}
		prev_front_wheel_pos = fc;
		prev_rear_wheel_pos  = rc;
		wheel_history_initialized = true;
	}

	// Bump detection
	prev_gradient = damp_dt_independent(terrain_gradient, prev_gradient, 0.08f, dt);
	bump_impulse  = glm::abs(terrain_gradient - prev_gradient) * speed;

	// Orientation: terrain pitch + roll
	const glm::vec3 right       = glm::normalize(glm::cross(bike_direction, glm::vec3(0, 1, 0)));
	const glm::vec3 terrain_up  = glm::normalize(glm::cross(right, terrain_forward_dir));
	const glm::quat base_orient = glm::quat(glm::mat3(right, terrain_up, -terrain_forward_dir));
	const glm::quat orient      = glm::angleAxis(current_roll, terrain_forward_dir) * base_orient;

	// Cosmetic crack pitch spring — fires on crack_impulse, springs back to neutral
	if (crack_impulse > 0.f)
		crack_pitch_vel -= crack_vis_pitch_impulse * crack_impulse;
	crack_pitch_vel  += (-crack_vis_spring * crack_pitch_disp - crack_vis_damp * crack_pitch_vel) * dt;
	crack_pitch_disp += crack_pitch_vel * dt;
	const glm::quat crack_pitch_rot = glm::angleAxis(crack_pitch_disp, right);

	get_owner()->set_ws_position_rotation(pos, crack_pitch_rot * orient);

	// Fork steer rotation
	if (fork_entity) {
		static constexpr float HEAD_TUBE_RAD = glm::radians(17.77f);
		const float max_steer_rad = compute_max_steer_rad(speed);
		const float fork_angle    = current_steer * max_steer_rad;
		// Non-linear steer response: large amplification at small steer, smaller at full steer.
		// Then lean fade reduces bars further toward neutral at full lean.
		const float steer_abs  = glm::abs(current_steer);
		const float steer_scale = glm::mix(bar_scale_lo_steer, bar_scale_hi_steer, steer_abs);
		const float lean_frac  = glm::clamp(glm::abs(current_roll) / glm::radians(lean_max_deg), 0.f, 1.f);
		const float lean_fade  = glm::smoothstep(bar_lean_fade_lo, bar_lean_fade_hi, lean_frac);
		const float bar_scale  = glm::mix(steer_scale, bar_visual_lean_min, lean_fade);
		const float fork_visual = fork_angle * bar_scale - current_roll * glm::sin(HEAD_TUBE_RAD);
		fork_entity->set_ls_euler_rotation(glm::vec3(HEAD_TUBE_RAD, -fork_visual, 0.f));
	}
}

float BikeObject::get_wind_along_bike() const
{
	const float eff_wind_speed = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);
	const glm::vec3 wdir_norm = glm::length(g_wind.wind_direction) > 0.001f
		? glm::normalize(g_wind.wind_direction) : glm::vec3(0.f);
	const float wind_along_bike = glm::dot(wdir_norm, bike_direction) * eff_wind_speed;

	return wind_along_bike;
}

glm::vec3 BikeObject::get_wind_along_bike_vector() const
{
	const float eff_wind_speed = g_wind.wind_speed * (1.f + g_wind.wind_gust_factor * 1.5f);


	return g_wind.wind_direction * eff_wind_speed - bike_direction*speed;
}

// ============================================================
// Debug menu: Bike Physics
// ============================================================

static void bike_physics_debug()
{
	ImGui::SeparatorText("Steering");
	{
#define DAMP_IMGUI(name) ImGui::DragFloat(#name, &name, 0.001f, 0.f, 0.2f)
		DAMP_IMGUI(steer_build_lo);
		DAMP_IMGUI(steer_build_hi);
		DAMP_IMGUI(steer_release_lo);
		DAMP_IMGUI(steer_release_hi);
#undef DAMP_IMGUI
		ImGui::DragFloat("bike_brake", &BIKE_MAX_BRAKE, 0.05, 0, 8);
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
		if (s_bike_debug)
			ImGui::Text("current max steer: %.1f deg  (speed %.1f m/s)",
			            glm::degrees(compute_max_steer_rad(s_bike_debug->speed)), s_bike_debug->speed);
		ImGui::DragFloat("steer_radius_coeff",   &steer_radius_coeff,   0.005f, 0.f,   0.2f);
		ImGui::DragFloat("lean_max_deg",         &lean_max_deg,         0.5f,   5.f,  50.f);
		ImGui::DragFloat("lean_steer_min",       &lean_steer_min,       0.01f,  0.f,   0.5f);
		ImGui::DragFloat("lean_steer_full",      &lean_steer_full,      0.01f,  0.1f,  1.0f);
		if (s_bike_debug)
			ImGui::Text("  lean_scale = %.2f  (current_steer=%.3f)",
				glm::smoothstep(lean_steer_min, lean_steer_full, glm::abs(s_bike_debug->current_steer)),
				s_bike_debug->current_steer);
		ImGui::DragFloat("steer_vel_scale",      &steer_vel_scale,      0.1f,   0.5f, 20.f);
		ImGui::DragFloat("steer_vel_boost",      &steer_vel_boost,      0.002f, 0.f,  0.1f);
		ImGui::SeparatorText("Handlebar Visual");
		ImGui::DragFloat("bar_scale_lo_steer",  &bar_scale_lo_steer,  0.1f, 0.5f, 12.f);
		ImGui::DragFloat("bar_scale_hi_steer",  &bar_scale_hi_steer,  0.1f, 0.1f,  6.f);
		ImGui::DragFloat("bar_visual_lean_min", &bar_visual_lean_min, 0.02f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_lo",    &bar_lean_fade_lo,    0.01f, 0.f,  1.f);
		ImGui::DragFloat("bar_lean_fade_hi",    &bar_lean_fade_hi,    0.01f, 0.f,  1.f);
		if (s_bike_debug) {
			const float sa    = glm::abs(s_bike_debug->current_steer);
			const float ss    = glm::mix(bar_scale_lo_steer, bar_scale_hi_steer, sa);
			const float lf    = glm::clamp(glm::abs(s_bike_debug->current_roll) / glm::radians(lean_max_deg), 0.f, 1.f);
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
		if (s_bike_debug)
			ImGui::Text("wobble: steer=%.3f vel=%.3f timer=%.2f",
				s_bike_debug->wobble_steer, s_bike_debug->wobble_vel, s_bike_debug->wobble_timer);
	}
	ImGui::SeparatorText("Crosswind Gusts");
	{
		ImGui::DragFloat("crosswind_gust_kick",         &crosswind_gust_kick,         0.005f, 0.f,  0.5f);
		ImGui::DragFloat2("crosswind_interval lo/hi",   &crosswind_gust_interval_lo,  0.5f,   0.5f, 30.f);
		ImGui::DragFloat("crosswind_vel_decay",         &crosswind_vel_decay,         0.0005f, 0.f,  0.02f);
		ImGui::DragFloat("crosswind_steer_decay",       &crosswind_steer_decay,       0.001f, 0.f,  0.02f);
		ImGui::DragFloat("crosswind_speed_stable",      &crosswind_speed_stable,      0.5f,   1.f,  30.f);
		if (s_bike_debug)
			ImGui::Text("wind_steer=%.3f  vel=%.3f  gust_t=%.1f",
				s_bike_debug->wind_steer, s_bike_debug->wind_vel, s_bike_debug->crosswind_gust_timer);
	}
	ImGui::SeparatorText("Pedal Stroke");
	{
		ImGui::DragFloat("stroke_amp", &stroke_amp, 0.01f, 0.f, 1.f);
		if (s_bike_debug)
			ImGui::Text("phase: %.2f  envelope: %.2f",
				s_bike_debug->stroke_phase,
				glm::max(0.f, glm::sin(s_bike_debug->stroke_phase * 2.f)));
	}
	ImGui::SeparatorText("Gear");
	{
		ImGui::InputFloat("shift_cooldown", &bike_gear_shift_cooldown);
	}
	ImGui::SeparatorText("Bump Steer");
	{
		ImGui::DragFloat("bump_steer_thresh",  &bump_steer_thresh,  0.0005f, 0.f,   0.05f);
		ImGui::DragFloat("bump_steer_kick",    &bump_steer_kick,    1.f,     0.f,   200.f);
		ImGui::DragFloat("bump_steer_spring",  &bump_steer_spring,  5.f,     10.f,  500.f);
		ImGui::DragFloat("bump_steer_damp",    &bump_steer_damp,    0.5f,    0.f,   40.f);
		ImGui::DragFloat("bump_steer_max",     &bump_steer_max,     0.005f,  0.f,   0.5f);
		if (s_bike_debug)
			ImGui::Text("disp=%.3f  vel=%.2f", s_bike_debug->bump_steer_disp, s_bike_debug->bump_steer_vel);
	}
	ImGui::SeparatorText("Bump Speed/Power Loss");
	{
		ImGui::DragFloat("bump_speed_thresh",  &bump_speed_thresh,  0.0005f, 0.f,  0.05f);
		ImGui::DragFloat("bump_speed_loss",    &bump_speed_loss,    0.002f,  0.f,  0.5f);
		ImGui::DragFloat("bump_rolling_scale", &bump_rolling_scale, 0.5f,    0.f,  50.f);
	}
	ImGui::SeparatorText("Crack Visual Pitch");
	{
		ImGui::DragFloat("crack_vis_pitch_impulse (deg)", &crack_vis_pitch_impulse, 0.001f, 0.f, glm::radians(20.f));
		ImGui::DragFloat("crack_vis_spring",              &crack_vis_spring,        1.f,    10.f, 400.f);
		ImGui::DragFloat("crack_vis_damp",                &crack_vis_damp,          0.1f,   0.f,  30.f);
		if (s_bike_debug)
			ImGui::Text("pitch_disp=%.3f rad  vel=%.2f", s_bike_debug->crack_pitch_disp, s_bike_debug->crack_pitch_vel);
	}
	ImGui::SeparatorText("Traction");
	{
		ImGui::DragFloat("traction_mu",          &traction_mu,          0.01f, 0.5f,  2.f);
		ImGui::DragFloat("traction_lean_comp",   &traction_lean_comp,   0.01f, 0.05f, 1.f);
		ImGui::DragFloat("traction_build_rate",  &traction_build_rate,  0.1f,  0.f,  10.f);
		ImGui::DragFloat("traction_recover_rate",&traction_recover_rate, 0.1f,  0.f,  10.f);
		if (s_bike_debug) {
			ImGui::DragFloat("surface_traction",  &s_bike_debug->surface_traction, 0.01f, 0.f, 1.f);
			ImGui::ProgressBar(s_bike_debug->rear_skid, ImVec2(-1, 0), "rear skid");
			ImGui::ProgressBar(s_bike_debug->corner_warn_timer / 0.15f, ImVec2(-1, 0), "corner warn");
			ImGui::Text("crashed: %s (%.1fs)", s_bike_debug->is_crashed ? "YES" : "no", s_bike_debug->crash_timer);
		}
	}
}
ADD_TO_DEBUG_MENU(bike_physics_debug);
