#include "BikeHeaders.h"

#include "Framework/MathLib.h"
#include "Render/DrawPublic.h"
#include "Render/RenderObj.h"
#include "imgui.h"

#include <cstdlib>
#include <glm/gtc/noise.hpp>

// ============================================================
// Wind state — exported to BikeCamera.cpp and BikeApplication.cpp
// ============================================================

glm::vec3 wind_direction    = glm::vec3(1.f, 0.f, 0.f);
float     wind_speed        = 4.f;
float     wind_gust_factor  = 0.f;   // [0,1] Perlin-driven
float     wind_elapsed_time = 0.f;

// Per-streak property ranges (interpolated by radius at spawn time)
static float radius_min   =  6.f;
static float radius_max   = 34;
static float len_near     =  7.f;    // ribbon length  at min radius
static float len_far      = 19.f;    // ribbon length  at max radius
static float width_near   =  0.055f;  // half-width     at min radius
static float width_far    =  0.19f;  // half-width     at max radius
static float alpha_near   = 200.f;   // max alpha      at min radius
static float alpha_far    =  70.f;   // max alpha      at max radius

// Shared
static float wind_v_spread     = 1.3f;
static float wind_life_min     = 2.f;
static float wind_life_max     = 8.f;
static float wind_fade_time    = 0.6f;
static float wind_cam_fade_dist = 8.f;  // fade to 0 alpha within this distance of camera

// Flow field — defines how wind actually curves through space
static float flow_lat_scale    = 0.23f;   // lateral amplitude (world units)
static float flow_vert_scale   = 0.35f;  // vertical amplitude
static float flow_freq         = 0.07f;  // spatial frequency of large-scale field
static float flow_time_speed   = 0.09f;  // how fast the field evolves
static float flow_turb_scale   = 0.28f;  // small-scale turbulence amplitude
static float flow_turb_freq    = 0.28f;  // small-scale frequency

// Perlin gust
static float gust_noise_speed    = 0.12f;
static float gust_speed_amp      = 1.5f;
static float gust_lateral_boost  = 2.2f;

// ============================================================
// BikePlayer::update_wind
// ============================================================

void BikePlayer::update_wind(BikeObject* bike)
{
	const float dt = eng->get_dt();
	wind_elapsed_time += dt;

	auto velocity = bike->bike_direction * bike->speed;
	velocity.y = 0;

	const glm::vec3 bike_pos = bike->get_owner()->get_ws_position();
	const glm::vec3 wdir = glm::length(wind_direction) > 0.001f
	                       ? glm::normalize(wind_direction) : glm::vec3(1.f, 0.f, 0.f);
	const glm::vec3 wright = glm::normalize(glm::cross(wdir, glm::vec3(0, 1, 0)));

	// --- Perlin gust: smooth [0,1] that slowly rolls in and out ---
	const float gust_raw = glm::perlin(glm::vec2(wind_elapsed_time * gust_noise_speed, 3.7f));
	wind_gust_factor     = glm::clamp((gust_raw + 0.7f) / 1.4f, 0.f, 1.f);

	const float eff_wind_speed = wind_speed * (1.f + wind_gust_factor * gust_speed_amp);
	const float lat_amp        = flow_lat_scale * (1.f + wind_gust_factor * gust_lateral_boost);

	// --- Flow field ---
	// Returns a normalized direction for any point in space.
	// Two Perlin octaves: large-scale lateral+vertical, small-scale turbulence.
	// Time is encoded as a slow drift along the X noise axis so each slice
	// evolves continuously without a 3D perlin sample.
	auto flow_dir = [&](glm::vec3 pos) -> glm::vec3 {
		const float tx = pos.x * flow_freq + wind_elapsed_time * flow_time_speed;
		const float tz = pos.z * flow_freq;
		const float lat  = glm::perlin(glm::vec2(tx,         tz));
		const float vert = glm::perlin(glm::vec2(tx + 17.3f, tz + 8.1f));
		const float turb = glm::perlin(glm::vec2(pos.x * flow_turb_freq + 53.f,
		                                          pos.z * flow_turb_freq));
		return glm::normalize(
			wdir
			+ wright          * (lat * lat_amp + turb * flow_turb_scale)
			+ glm::vec3(0,1,0) * (vert * flow_vert_scale)
		);
	};

	// --- Spawn / respawn ---
	auto rand_range = [](float lo, float hi) {
		return lo + (float)(rand() % 10000) / 10000.f * (hi - lo);
	};

	auto spawn_streak = [&](BikePlayer::WindLine& wl, bool stagger) {
		// Each streak picks its own radius, then derives length/width/alpha from it
		wl.radius = rand_range(radius_min, radius_max);
		const float t = (wl.radius - radius_min) / glm::max(radius_max - radius_min, 0.001f);
		wl.len   = glm::mix(len_near,   len_far,   t);
		wl.width = glm::mix(width_near, width_far, t);
		wl.alpha = glm::mix(alpha_near, alpha_far, t);

		// Spawn randomly in a disc around the player.
		// Flow field drives directional feel; disc ensures streaks are always visible
		// regardless of whether the player is riding with or against the wind.
		const float angle = rand_range(0.f, glm::two_pi<float>());
		const float r     = glm::sqrt(rand_range(0.001f, 1.f)) * wl.radius;
		wl.max_life   = rand_range(wind_life_min, wind_life_max);
		wl.lifetime   = stagger ? rand_range(0.f, wl.max_life) : 0.f;
		auto pos_to_use = bike_pos + (wl.max_life-wl.lifetime) * velocity * 0.35f;
		wl.pos.x = pos_to_use.x + glm::cos(angle) * r;
		wl.pos.z = pos_to_use.z + glm::sin(angle) * r;
		wl.pos.y = pos_to_use.y + rand_range(-wind_v_spread, wind_v_spread);

		wl.wave_phase = 0.f;
		wl.wave_speed = rand_range(0.7f, 1.3f);
	};

	if (!wind_initialized) {
		for (int i = 0; i < WIND_LINE_COUNT; i++)
			spawn_streak(wind_lines[i], true);
		wind_initialized = true;
	}

	constexpr int WIND_SEGS = 20;
	constexpr int MID = WIND_SEGS / 2;
	constexpr glm::vec3 WORLD_UP{0.f, 1.f, 0.f};
	wind_mb.Begin();

	for (int i = 0; i < WIND_LINE_COUNT; i++) {
		WindLine& wl = wind_lines[i];

		// Move along the flow field at per-streak speed
		wl.pos      += flow_dir(wl.pos) * eff_wind_speed * wl.wave_speed * dt;
		wl.lifetime += dt;

		// Die when too far from player (drifted out of the disc), or timed out
		const glm::vec2 hdiff(wl.pos.x - bike_pos.x, wl.pos.z - bike_pos.z);
		if (glm::length(hdiff) > wl.radius * 1.5f || wl.lifetime >= wl.max_life)
			spawn_streak(wl, false);

		// Fade in/out
		const float fade_in  = glm::clamp(wl.lifetime / wind_fade_time, 0.f, 1.f);
		const float fade_out = glm::clamp((wl.max_life - wl.lifetime) / wind_fade_time, 0.f, 1.f);
		const float base_a   = wl.alpha * glm::min(fade_in, fade_out);
		if (base_a < 2.f) continue;

		// Build ribbon by integrating the flow field from the streak's center
		glm::vec3 pts[WIND_SEGS + 1];
		const float step = wl.len / WIND_SEGS;
		pts[MID] = wl.pos;

		for (int s = MID; s < WIND_SEGS; s++)
			pts[s + 1] = pts[s] + flow_dir(pts[s]) * step;
		for (int s = MID; s > 0; s--)
			pts[s - 1] = pts[s] - flow_dir(pts[s]) * step;

		// --- Emit ribbon geometry ---
		glm::vec3 prev_lo{}, prev_hi{};
		Color32   prev_c{};
		bool prev_valid = false;

		for (int s = 0; s <= WIND_SEGS; s++) {
			const float t     = (float)s / WIND_SEGS;
			const float env_w   = glm::pow(glm::sin(glm::pi<float>() * t), 0.55f);
			const float env_a   = glm::pow(glm::sin(glm::pi<float>() * t), 0.40f);
			const float cam_d   = glm::length(pts[s] - camera_pos);
			const float cam_fade = glm::clamp(cam_d / wind_cam_fade_dist, 0.f, 1.f);
			const float w       = wl.width * env_w;
			const uint8_t a     = (uint8_t)glm::clamp(base_a * env_a * cam_fade, 0.f, 255.f);

			const Color32   col{230, 245, 255, a};
			const glm::vec3 lo = pts[s] - WORLD_UP * w;
			const glm::vec3 hi = pts[s] + WORLD_UP * w;

			if (prev_valid) {
				int base = wind_mb.GetBaseVertex();
				wind_mb.AddVertex({prev_lo, prev_c});
				wind_mb.AddVertex({prev_hi, prev_c});
				wind_mb.AddVertex({lo,      col});
				wind_mb.AddVertex({hi,      col});
				wind_mb.AddTriangle(base + 0, base + 1, base + 3);
				wind_mb.AddTriangle(base + 0, base + 3, base + 2);
			}
			prev_lo = lo; prev_hi = hi; prev_c = col; prev_valid = true;
		}
	}
	wind_mb.End();

	Particle_Object wpo{};
	wpo.meshbuilder = &wind_mb;
	wpo.transform   = glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(wind_handle, wpo);
}

// ============================================================
// Debug menu: Bike Wind
// ============================================================

static void bike_wind_debug()
{
	ImGui::SeparatorText("Wind");
	{
		ImGui::DragFloat3("wind_direction", &wind_direction.x,  0.05f);
		ImGui::DragFloat ("wind_speed",     &wind_speed,         0.1f, 0.f, 20.f);
		ImGui::DragFloat ("wind_v_spread",  &wind_v_spread,      0.1f, 0.f,  8.f);
		ImGui::DragFloat ("wind_life_min",  &wind_life_min,      0.05f, 0.1f, 5.f);
		ImGui::DragFloat ("wind_life_max",  &wind_life_max,      0.05f, 0.5f, 10.f);
		ImGui::DragFloat ("wind_fade_time",     &wind_fade_time,      0.02f, 0.f,  2.f);
		ImGui::DragFloat ("wind_cam_fade_dist", &wind_cam_fade_dist,  0.1f,  0.f, 20.f);
	}
	ImGui::SeparatorText("Streak Variation (near → far)");
	{
		ImGui::DragFloat2("radius  min/max", &radius_min,  0.2f,   0.f, 60.f);
		ImGui::DragFloat2("len     near/far", &len_near,   0.5f,   1.f, 60.f);
		ImGui::DragFloat2("width   near/far", &width_near, 0.005f, 0.01f, 1.f);
		ImGui::DragFloat2("alpha   near/far", &alpha_near, 1.f,    0.f, 255.f);
	}
	ImGui::SeparatorText("Flow Field");
	{
		ImGui::DragFloat("flow_lat_scale",   &flow_lat_scale,   0.05f, 0.f,   6.f);
		ImGui::DragFloat("flow_vert_scale",  &flow_vert_scale,  0.02f, 0.f,   2.f);
		ImGui::DragFloat("flow_freq",        &flow_freq,        0.005f, 0.01f, 0.5f);
		ImGui::DragFloat("flow_time_speed",  &flow_time_speed,  0.005f, 0.f,   0.5f);
		ImGui::DragFloat("flow_turb_scale",  &flow_turb_scale,  0.02f, 0.f,   2.f);
		ImGui::DragFloat("flow_turb_freq",   &flow_turb_freq,   0.01f, 0.01f, 1.f);
	}
	ImGui::SeparatorText("Perlin Gust");
	{
		ImGui::DragFloat("gust_noise_speed",   &gust_noise_speed,   0.005f, 0.01f, 1.f);
		ImGui::DragFloat("gust_speed_amp",     &gust_speed_amp,     0.05f,  0.f,   4.f);
		ImGui::DragFloat("gust_lateral_boost", &gust_lateral_boost, 0.05f,  0.f,   6.f);
		ImGui::ProgressBar(wind_gust_factor, ImVec2(-1, 0), "gust factor");
	}
}
ADD_TO_DEBUG_MENU(bike_wind_debug);
