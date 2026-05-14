#include "ObsGameHeaders.h"

#include "GameEnginePublic.h"
#include "Input/InputSystem.h"
#include "Framework/MathLib.h"

#include <glm/gtc/matrix_transform.hpp>

// ============================================================
// ObsCamera — third-person orbit follow, driven from ObsPlayer::update()
// (not auto-ticked so the order chest-pos → camera → hands is deterministic).
// ============================================================

void ObsCamera::start()
{
	set_ticking(false);
}

static float gamepad_axis(SDL_GameControllerAxis a)
{
	const float v = (float)Input::get_con_axis(a);
	const float dead = 0.2f;
	if (glm::abs(v) < dead) return 0.f;
	const float sign = v < 0.f ? -1.f : 1.f;
	return sign * (glm::abs(v) - dead) / (1.f - dead);
}

void ObsCamera::update()
{
	auto* app = ObsGameApplication::get();
	if (!app || !app->player || !cc) return;
	const float dt = (float)eng->get_dt();
	if (dt <= 0.f) return;

	// Stick input — right stick rotates the camera around chest.
	const float rx = gamepad_axis(SDL_CONTROLLER_AXIS_RIGHTX);
	const float ry = gamepad_axis(SDL_CONTROLLER_AXIS_RIGHTY);

	// Mouse fallback (kb+m users).
	float mouse_dx = 0.f, mouse_dy = 0.f;
	if (!Input::last_recieved_input_from_con()) {
		const auto md = Input::get_mouse_delta();
		mouse_dx = (float)md.x * 0.005f;
		mouse_dy = (float)md.y * 0.005f;
	}

	yaw   -= rx * app->cam_yaw_sens * dt + mouse_dx;
	pitch -= ry * app->cam_pitch_sens * dt + mouse_dy;
	pitch  = glm::clamp(pitch, app->cam_pitch_min, app->cam_pitch_max);

	// Spherical → cartesian offset from chest.
	const glm::vec3 chest = app->player->get_chest_pos()
		+ glm::vec3(0.f, app->cam_height_offset, 0.f);

	const float cp = std::cos(pitch);
	const float sp = std::sin(pitch);
	const float cy = std::cos(yaw);
	const float sy = std::sin(yaw);

	// Look direction (from camera toward chest).
	const glm::vec3 look_dir = glm::vec3(-cp * sy, -sp, -cp * cy);
	forward = look_dir;
	right   = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
	up      = glm::cross(right, forward);

	const glm::vec3 cam_pos = chest - look_dir * app->cam_distance;

	// Build view from basis: camera looks down -forward.
	glm::mat4 m(1.f);
	m[0] = glm::vec4(right,    0.f);
	m[1] = glm::vec4(up,       0.f);
	m[2] = glm::vec4(-forward, 0.f);
	m[3] = glm::vec4(cam_pos,  1.f);
	cc->get_owner()->set_ws_transform(m);
}
