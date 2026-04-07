#include "BikeHeaders.h"

#include "Framework/MathLib.h"
#include "Input/InputSystem.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "imgui.h"

#include <SDL2/SDL_gamecontroller.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>

// ============================================================
// Camera tuning vars
// ============================================================

static float bike_camera_lead          = 15.f;
static float bike_camera_lead_interp   = 0.02f;
static float bike_camera_brake         = 8.f;
static float bike_camera_brake_fast    = 0.003f;  // response on brake input
static float bike_camera_brake_slow    = 0.04f;   // recovery when released
static float bike_camera_gradient      = 0.6f;
static float bike_camera_gradient_fast = 0.002f;  // snap up on climb
static float bike_camera_gradient_slow = 0.015f;  // lazy return on descent/flat
static float bike_fov_fast             = 0.003f;  // widen on acceleration
static float bike_fov_slow             = 0.02f;   // slow bleed back
static float bike_fov_brake            = 8.f;     // degrees to narrow FOV at full brake
static float bike_fov_power_scale      = 0.03f;   // degrees per watt of power step
static float bike_fov_power_decay      = 0.004f;  // smoothing toward 0 (~0.5s half-life)

// Camera roll
static float cam_roll_scale  = 0.07f;
static float cam_roll_smooth = 0.04f;

// Cadence bob
static bool  bob_enabled      = true;
static float bob_power_thresh = 350.f;
static float bob_vert_amp     = 0.012f;
static float bob_pitch_amp    = 0.008f;

// Speed lines
static bool  lines_enabled    = true;
static int   lines_count      = 32;
static float lines_speed_min  = 10.f;
static float lines_speed_full = 22.f;
static float lines_inner      = 0.55f;
static float lines_outer      = 1.0f;
static float lines_max_len    = 0.22f;
static float lines_width      = 0.004f;
static float lines_phase_rate = 1.4f;
static float lines_alpha_peak = 200.f;

// Road bump FX
static bool  bump_enable_pitch   = true;
static bool  bump_enable_vert    = false;
static bool  bump_enable_shake   = false;
static bool  bump_enable_rumble  = false;
static float bump_detect_thresh  = 0.004f;
static float bump_speed_scale    = 0.08f;
static float bump_pitch_spring   = 180.f;
static float bump_pitch_damp     = 12.f;
static float bump_pitch_impulse  = 0.4f;
static float bump_vert_spring    = 140.f;
static float bump_vert_damp      = 10.f;
static float bump_vert_impulse   = 0.06f;
static float bump_shake_impulse  = 0.04f;
static float bump_shake_decay    = 8.f;
static float bump_rumble_scale   = 600.f;

// Wind camera shake (camera-side parameter, driven by wind state)
static float wind_camera_shake_deg = 0.2f;

// Shared wind state from BikeWind.cpp
extern glm::vec3 wind_direction;
extern float     wind_speed;
extern float     wind_gust_factor;
extern float     gust_speed_amp;
extern float     wind_elapsed_time;

// ============================================================
// Helpers
// ============================================================

static float damp_asymmetric(float target, float current, float fast, float slow, float dt) {
	float smoothing = (target > current) ? fast : slow;
	return damp_dt_independent(target, current, smoothing, dt);
}

static float apply_deadzone_cam(float val, float dz) {
	if (glm::abs(val) < dz) return 0.f;
	return glm::sign(val) * (glm::abs(val) - dz) / (1.f - dz);
}

// ============================================================
// BikePlayer::update_camera
// ============================================================

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
	const float rx = apply_deadzone_cam((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX), 0.15f);
	const float ry = apply_deadzone_cam((float)Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY), 0.15f);
	const bool stick_active = glm::abs(rx) > 0.f || glm::abs(ry) > 0.f;
	if (stick_active) {
		camera_yaw   += rx * 2.2f * dt;
		camera_pitch += ry * 2.2f * dt;
		camera_pitch  = glm::clamp(camera_pitch, glm::radians(-10.f), glm::radians(70.f));
	} else {
		camera_yaw   = damp_dt_independent(0.f,           camera_yaw,   0.01f, dt);
		camera_pitch = damp_dt_independent(default_pitch, camera_pitch, 0.01f, dt);
	}

	// --- Gradient pitch ---
	gradient_pitch = damp_asymmetric(bike->terrain_gradient * bike_camera_gradient, gradient_pitch, bike_camera_gradient_fast, bike_camera_gradient_slow, dt);

	// --- Brake pitch ---
	brake_pitch = damp_asymmetric(-brake_amount * glm::radians(bike_camera_brake), brake_pitch, bike_camera_brake_slow, bike_camera_brake_fast, dt);

	// --- Corner lead ---
	lead_yaw = damp_dt_independent(-steer * glm::radians(bike_camera_lead), lead_yaw, bike_camera_lead_interp, dt);

	// --- Orbit position ---
	const glm::quat yaw_rot     = glm::angleAxis(camera_yaw, glm::vec3(0, 1, 0));
	const glm::vec3 yawed_dir   = yaw_rot * (-fwd);
	const glm::vec3 orbit_right = glm::normalize(glm::cross(yawed_dir, glm::vec3(0, 1, 0)));
	const float total_pitch     = camera_pitch + gradient_pitch + brake_pitch;
	const glm::quat pitch_rot   = glm::angleAxis(total_pitch, orbit_right);
	const glm::vec3 orbit_dir   = glm::normalize(pitch_rot * yawed_dir);
	const glm::vec3 target_pos  = pivot + orbit_dir * cam_dist;

	// --- Look-ahead target ---
	const float look_ahead      = 3.0f + bike->speed * 0.5f;
	const glm::quat lead_rot    = glm::angleAxis(lead_yaw, glm::vec3(0, 1, 0));
	const glm::vec3 look_target = pivot + (lead_rot * fwd) * look_ahead;

	if (!camera_initialized) {
		camera_pos         = target_pos;
		smooth_aim_pos     = look_target;
		camera_initialized = true;
	}

	const float pos_lag = 0.015f + bike->speed * 0.002f;
	camera_pos     = damp_dt_independent(target_pos,  camera_pos,     pos_lag, dt);
	smooth_aim_pos = damp_dt_independent(look_target, smooth_aim_pos, 0.005f,  dt);



	// Speed-based FOV
	const float fov_target = 65.f + glm::clamp(bike->speed * 0.4f, 0.f, 20.f) - brake_amount * bike_fov_brake;
	fov_smoothed = damp_asymmetric(fov_target, fov_smoothed, bike_fov_fast, bike_fov_slow, dt);
	cc->set_fov(fov_smoothed);

	// --- Road bump FX ---
	const float raw_impulse = bike->bump_impulse;
	const float bump = (raw_impulse > bump_detect_thresh) ? raw_impulse * bump_speed_scale : 0.f;

	// Pitch spring
	if (bump_enable_pitch) {
		if (bump > 0.f)
			bump_pitch_vel -= bump * bump_pitch_impulse;
		bump_pitch_vel  += (-bump_pitch_spring * bump_pitch_disp - bump_pitch_damp * bump_pitch_vel) * dt;
		bump_pitch_disp += bump_pitch_vel * dt;
	} else {
		bump_pitch_disp = bump_pitch_vel = 0.f;
	}

	// Vertical spring
	if (bump_enable_vert) {
		if (bump > 0.f)
			bump_vert_vel += bump * bump_vert_impulse;
		bump_vert_vel  += (-bump_vert_spring * bump_vert_disp - bump_vert_damp * bump_vert_vel) * dt;
		bump_vert_disp += bump_vert_vel * dt;
	} else {
		bump_vert_disp = bump_vert_vel = 0.f;
	}

	// Camera shake
	if (bump_enable_shake) {
		if (bump > 0.f) {
			shake_magnitude += bump * bump_shake_impulse;
			shake_offset = glm::vec3(
				((float)rand() / RAND_MAX * 2.f - 1.f),
				((float)rand() / RAND_MAX * 2.f - 1.f),
				((float)rand() / RAND_MAX * 2.f - 1.f)) * shake_magnitude;
		}
		shake_magnitude = glm::max(0.f, shake_magnitude - bump_shake_decay * shake_magnitude * dt);
		shake_offset = damp_dt_independent(glm::vec3(0.f), shake_offset, 0.08f, dt);
	} else {
		shake_offset = glm::vec3(0.f);
		shake_magnitude = 0.f;
	}

	const glm::vec3 orbit_up   = glm::normalize(glm::cross(orbit_right, orbit_dir));
	const glm::vec3 bumped_pos = camera_pos + orbit_up * bump_vert_disp + shake_offset;

	// Controller rumble
	if (bump_enable_rumble && bump > 0.f) {
		const uint16_t intensity = (uint16_t)glm::clamp(bump * bump_rumble_scale, 0.f, 65535.f);
		Input::rumble(intensity / 2, intensity, 80);
	}

	// --- Camera roll ---
	const float roll_target = -bike->current_steer * bike->current_roll * cam_roll_scale
	                          * glm::clamp(bike->speed / 10.f, 0.f, 1.f);
	camera_roll = damp_dt_independent(roll_target, camera_roll, cam_roll_smooth, dt);

	// --- Cadence bob ---
	float bob_vert  = 0.f;
	float bob_pitch = 0.f;
	if (bob_enabled && bike->cadence > 0.f) {
		const float power_t = glm::clamp((bike->speed > 0.1f ? (float)BIKE_POWER_LEVELS[power_level_idx] : 0.f) / bob_power_thresh - 1.f, 0.f, 1.f);
		if (power_t > 0.f) {
			cadence_bob_phase += bike->cadence * glm::two_pi<float>() * dt;
			bob_vert  = glm::sin(cadence_bob_phase) * bob_vert_amp  * power_t;
			bob_pitch = glm::sin(cadence_bob_phase) * bob_pitch_amp * power_t;
		}
	}

	// --- Build look-at ---
	glm::vec3 base_fwd = glm::normalize(smooth_aim_pos - bumped_pos);

	// Wind microshake
	{

		const float wind_along_bike = bike->get_wind_along_bike();// glm::dot(wdir_norm, bike->bike_direction)* eff_wind_speed;

		const float shake_rad = glm::radians(wind_camera_shake_deg)
		                        * glm::clamp(-wind_along_bike / 10.f, 0.f, 1.f);
		if (shake_rad > 1e-5f) {
			const float t  = wind_elapsed_time*2.5f;
			const float nx = glm::sin(t * 1.7f) * glm::cos(t * 2.3f + 0.5f);
			const float ny = glm::sin(t * 1.3f) * glm::cos(t * 2.9f + 1.2f);
			const glm::vec3 right_approx = glm::normalize(glm::cross(base_fwd, glm::vec3(0, 1, 0)));
			base_fwd = glm::normalize(
				glm::angleAxis(nx * shake_rad, glm::vec3(0, 1, 0)) *
				glm::angleAxis(ny * shake_rad, right_approx) *
				base_fwd);
		}
	}

	const glm::vec3 cam_right = glm::normalize(glm::cross(base_fwd, glm::vec3(0, 1, 0)));
	const glm::quat pitch_spring_rot = glm::angleAxis(bump_pitch_disp + bob_pitch, cam_right);
	const glm::vec3 cam_fwd   = glm::normalize(pitch_spring_rot * base_fwd);
	const glm::vec3 cam_up_raw = glm::cross(cam_right, cam_fwd);
	const glm::quat roll_rot  = glm::angleAxis(camera_roll, cam_fwd);
	const glm::vec3 cam_up    = roll_rot * cam_up_raw;
	const glm::vec3 final_pos = bumped_pos + orbit_up * bob_vert;

	cc->get_owner()->set_ws_transform(glm::mat4(
		glm::vec4(cam_right, 0.f),
		glm::vec4(cam_up,    0.f),
		glm::vec4(-cam_fwd,  0.f),
		glm::vec4(final_pos, 1.f)
	));

	// --- Speed lines ---
	{
		if (!sl_initialized) {
			const float step = glm::two_pi<float>() / (float)MAX_SPEEDLINES;
			for (int i = 0; i < MAX_SPEEDLINES; i++) {
				sl_angles[i] = step * i + ((float)(rand() % 100) / 100.f) * step * 0.8f;
				sl_phases[i] = (float)i / (float)MAX_SPEEDLINES;
			}
			sl_initialized = true;
		}

		const float speed_t = glm::clamp((bike->speed - lines_speed_min) / (lines_speed_full - lines_speed_min), 0.f, 1.f);
		const int n = glm::clamp(lines_count, 1, MAX_SPEEDLINES);
		for (int i = 0; i < n; i++) {
			sl_phases[i] += lines_phase_rate * speed_t * dt;
			if (sl_phases[i] > 1.f) {
				sl_phases[i] -= 1.f;
				sl_angles[i] += ((float)(rand() % 100) / 100.f - 0.5f) * 0.15f;
			}
		}

		speedlines_mb.Begin();
		if (lines_enabled && speed_t > 0.f) {
			const float fov_rad   = glm::radians(fov_smoothed);
			const float half_h    = glm::tan(fov_rad * 0.5f);
			const float depth     = 1.f;
			const glm::vec3 origin = final_pos + cam_fwd * depth;
			const float ring_span = (lines_outer - lines_inner) * half_h;

			for (int i = 0; i < n; i++) {
				const float ph  = sl_phases[i];
				const float env = glm::clamp(glm::min(ph / 0.15f, (1.f - ph) / 0.15f), 0.f, 1.f);
				const float a   = env * speed_t * lines_alpha_peak;
				if (a < 1.f) continue;

				const float r_centre = (lines_inner + ph * (lines_outer - lines_inner)) * half_h;
				const float half_len = lines_max_len * ring_span * 0.5f * env;
				const float r0 = r_centre - half_len;
				const float r1 = r_centre + half_len;

				const float ca = glm::cos(sl_angles[i]), sa = glm::sin(sl_angles[i]);
				const glm::vec3 radial  = cam_right * ca + cam_up * sa;
				const glm::vec3 tangent = -cam_right * sa + cam_up * ca;
				const float hw = lines_width * half_h;

				const Color32 c0{255, 255, 255, 0};
				const Color32 cm{255, 255, 255, (uint8_t)glm::clamp(a, 0.f, 255.f)};

				int base = speedlines_mb.GetBaseVertex();
				speedlines_mb.AddVertex({origin + radial * r0,              c0});
				speedlines_mb.AddVertex({origin + radial * r0,              c0});
				speedlines_mb.AddVertex({origin + radial * r_centre - tangent * hw, cm});
				speedlines_mb.AddVertex({origin + radial * r_centre + tangent * hw, cm});
				speedlines_mb.AddVertex({origin + radial * r1,              c0});
				speedlines_mb.AddVertex({origin + radial * r1,              c0});
				speedlines_mb.AddTriangle(base+0, base+2, base+3);
				speedlines_mb.AddTriangle(base+0, base+3, base+1);
				speedlines_mb.AddTriangle(base+2, base+4, base+5);
				speedlines_mb.AddTriangle(base+2, base+5, base+3);
			}
		}
		speedlines_mb.End();

		Particle_Object spo{};
		spo.meshbuilder = &speedlines_mb;
		spo.transform   = glm::mat4(1.f);
		idraw->get_scene()->update_particle_obj(speedlines_handle, spo);
	}
}

// ============================================================
// Debug menu: Bike Camera
// ============================================================

static void bike_camera_debug()
{
	ImGui::SeparatorText("Camera");
	{
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
		ImGui::InputFloat("bike_fov_brake",            &bike_fov_brake);
		ImGui::DragFloat("bike_fov_power_scale",       &bike_fov_power_scale, 0.001f, 0.f, 0.2f);
		ImGui::DragFloat("bike_fov_power_decay",       &bike_fov_power_decay, 0.001f, 0.f, 0.1f);
		ImGui::DragFloat("cam_roll_scale",  &cam_roll_scale,  0.001f, 0.f, 0.3f);
		ImGui::DragFloat("cam_roll_smooth", &cam_roll_smooth, 0.001f, 0.f, 0.3f);
	}
	ImGui::SeparatorText("Cadence Bob");
	{
		ImGui::Checkbox("bob_enabled",       &bob_enabled);
		ImGui::DragFloat("bob_power_thresh", &bob_power_thresh, 5.f,   0.f, 1000.f);
		ImGui::DragFloat("bob_vert_amp",     &bob_vert_amp,     0.001f, 0.f, 0.1f);
		ImGui::DragFloat("bob_pitch_amp",    &bob_pitch_amp,    0.001f, 0.f, 0.1f);
	}
	ImGui::SeparatorText("Speed Lines");
	{
		ImGui::Checkbox("lines_enabled",       &lines_enabled);
		ImGui::DragInt  ("lines_count",        &lines_count,      1,      4,    64);
		ImGui::DragFloat("lines_speed_min",    &lines_speed_min,  0.5f,   0.f,  30.f);
		ImGui::DragFloat("lines_speed_full",   &lines_speed_full, 0.5f,   0.f,  50.f);
		ImGui::DragFloat("lines_inner",        &lines_inner,      0.01f,  0.f,  2.f);
		ImGui::DragFloat("lines_outer",        &lines_outer,      0.01f,  0.f,  2.f);
		ImGui::DragFloat("lines_max_len",      &lines_max_len,    0.01f,  0.f,  1.f);
		ImGui::DragFloat("lines_width",        &lines_width,      0.0005f, 0.f, 0.05f);
		ImGui::DragFloat("lines_phase_rate",   &lines_phase_rate, 0.05f,  0.f,  5.f);
		ImGui::DragFloat("lines_alpha_peak",   &lines_alpha_peak, 1.f,    0.f,  255.f);
	}
	ImGui::SeparatorText("Road Bump FX");
	{
		ImGui::Checkbox("pitch",  &bump_enable_pitch);  ImGui::SameLine();
		ImGui::Checkbox("vert",   &bump_enable_vert);   ImGui::SameLine();
		ImGui::Checkbox("shake",  &bump_enable_shake);  ImGui::SameLine();
		ImGui::Checkbox("rumble", &bump_enable_rumble);
		ImGui::DragFloat("detect_thresh", &bump_detect_thresh, 0.0001f, 0.f, 0.1f);
		ImGui::DragFloat("speed_scale",   &bump_speed_scale,   0.001f,  0.f, 1.f);
		ImGui::DragFloat("pitch_spring",  &bump_pitch_spring,  1.f);
		ImGui::DragFloat("pitch_damp",    &bump_pitch_damp,    0.1f);
		ImGui::DragFloat("pitch_impulse", &bump_pitch_impulse, 0.01f);
		ImGui::DragFloat("vert_spring",   &bump_vert_spring,   1.f);
		ImGui::DragFloat("vert_damp",     &bump_vert_damp,     0.1f);
		ImGui::DragFloat("vert_impulse",  &bump_vert_impulse,  0.001f);
		ImGui::DragFloat("shake_impulse", &bump_shake_impulse, 0.001f);
		ImGui::DragFloat("shake_decay",   &bump_shake_decay,   0.1f);
		ImGui::DragFloat("rumble_scale",  &bump_rumble_scale,  10.f);
	}
	ImGui::SeparatorText("Wind Camera");
	{
		ImGui::DragFloat("wind_camera_shake_deg", &wind_camera_shake_deg, 0.01f, 0.f, 2.f);
	}
}
ADD_TO_DEBUG_MENU(bike_camera_debug);
