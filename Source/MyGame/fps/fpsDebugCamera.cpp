#include "fpsDebugCamera.h"
#include "GameEnginePublic.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "Debug.h"
#include "imgui.h"
#include "fpsObjects.h"
#include "Framework/MathLib.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>

ConfigVar g_debugcamera("g_debugcamera", "0", CVAR_BOOL | CVAR_DEV, "enable fly debug camera");
ConfigVar g_thirdperson_orbit("g_thirdperson_orbit", "0", CVAR_BOOL | CVAR_DEV,
							  "third person orbit camera, visualizes the pogo controller");

void fpsDebugCamera::init() {
	debug_cam_ent = GameplayStatic::spawn_entity();
	debug_cam_ent->create_component<CameraComponent>();
}

void fpsDebugCamera::shutdown() {
	
}

void fpsDebugCamera::update(EntityPtr game_camera_ent, fpsPlayer* player) {
	bool thirdperson = g_thirdperson_orbit.get_bool();
	bool freefly = g_debugcamera.get_bool();
	bool active = freefly || thirdperson;
	auto* game_cam = game_camera_ent->get_component<CameraComponent>();
	auto* debug_cam = debug_cam_ent->get_component<CameraComponent>();

	// the camera model (visualizing the real game camera) only makes sense while flying
	// free; in third person we're looking at the player instead.
	bool want_camera_model = freefly;

	if (freefly && !initialized_fly_cam) {
		glm::mat4 cam_t = game_camera_ent->get_ws_transform();
		fly_cam.position = glm::vec3(cam_t[3]);
		glm::vec3 forward = -glm::vec3(cam_t[2]);
		fly_cam.yaw = atan2f(forward.x, forward.z);
		fly_cam.pitch = asinf(glm::clamp(forward.y, -0.99f, 0.99f));
		fly_cam.move_speed = 0.1f;
		initialized_fly_cam = true;
	}

	if (want_camera_model && !camera_model_ent.get()) {
		camera_model_ent = GameplayStatic::spawn_entity();
		auto* mesh = camera_model_ent->create_component<MeshComponent>();
		mesh->set_model_str("camera_model.cmdl");
	}
	if (!want_camera_model && camera_model_ent.get()) {
		camera_model_ent->destroy();
		camera_model_ent = nullptr;
	}

	// set_is_enabled(true) auto-displaces the previous scene camera
	if (active)
		debug_cam->set_is_enabled(true);
	else
		game_cam->set_is_enabled(true);

	if (freefly) {
		// free-fly cam only takes input while RMB is held, to leave the mouse free otherwise.
		bool rmb = Input::is_mouse_down(2);
		UiSystem::inst->set_game_capture_mouse(rmb);

		if (rmb) {
			auto window_size = get_app_window_size();
			float aspect = (float)window_size.x / (float)window_size.y;
			fly_cam.update_from_input(window_size.x, window_size.y, aspect, glm::radians(game_cam->fov));
		}

		glm::mat4 view = fly_cam.get_view_matrix();
		glm::mat4 world = glm::inverse(view);
		debug_cam_ent->set_ws_transform(world);
		debug_cam->set_fov(game_cam->fov);

		if (camera_model_ent.get()) {
			camera_model_ent->set_ws_transform(game_camera_ent->get_ws_transform());
		}

		draw_frustum_lines(game_cam);
	} else if (thirdperson && player) {
		// mirror the fps camera's look direction (same view_yaw/view_pitch, same mouse
		// capture behavior - click to capture, escape to release), just pulled back and
		// placed behind the character instead of at the eye.
		glm::vec3 eye = player->get_eye_position();
		glm::vec3 forward = player->get_view_forward();
		glm::vec3 cam_pos = eye - forward * thirdperson_distance + glm::vec3(0, thirdperson_height, 0);

		glm::mat4 view = glm::lookAt(cam_pos, cam_pos + forward, glm::vec3(0, 1, 0));
		debug_cam_ent->set_ws_transform(glm::inverse(view));
		debug_cam->set_fov(game_cam->fov);

		draw_pogo_visualization(player);
	}

	was_active = active;
}

void fpsDebugCamera::draw_pogo_visualization(fpsPlayer* player) {
	fpsPogoDebugInfo info = player->get_pogo_debug_info();

	glm::vec3 feet = info.feet_pos;
	glm::vec3 cyl_bottom = feet + glm::vec3(0, info.capsule_radius, 0);
	glm::vec3 cyl_top = feet + glm::vec3(0, glm::max(info.capsule_height - info.capsule_radius, info.capsule_radius), 0);

	Color32 capsule_col = COLOR_CYAN;
	Debug::add_circle(cyl_bottom, glm::vec3(0, 1, 0), info.capsule_radius, capsule_col, 0.f, false);
	Debug::add_circle(cyl_top, glm::vec3(0, 1, 0), info.capsule_radius, capsule_col, 0.f, false);
	for (int i = 0; i < 4; i++) {
		float a = i * HALFPI;
		glm::vec3 offset = glm::vec3(cosf(a), 0.f, sinf(a)) * info.capsule_radius;
		Debug::add_line(cyl_bottom + offset, cyl_top + offset, capsule_col, 0.f, false);
	}

	char buf[256];
	if (info.enabled) {
		Color32 spring_col = info.grounded ? COLOR_GREEN : COLOR_RED;
		Debug::add_line(feet, info.ground_point, spring_col, 0.f, false);
		Debug::add_sphere(info.ground_point, 0.05f, spring_col, 0.f, false);

		snprintf(buf, sizeof(buf), "pogo: %s\nride_height: %.2f\ncompression: %+.3f\nground_dist: %.2f",
				 info.grounded ? "grounded" : "airborne", info.ride_height, info.compression, info.ground_dist);
	} else {
		snprintf(buf, sizeof(buf), "pogo: disabled\n(capsule controller active)");
	}
	Debug::add_text_ex(cyl_top + glm::vec3(info.capsule_radius + 0.2f, 0.f, 0.f), buf, COLOR_WHITE, 0.f, false, false,
					   false);
}

void fpsDebugCamera::draw_frustum_lines(CameraComponent* game_camera) {
	glm::mat4 cam_t = game_camera->get_ws_transform();
	glm::vec3 origin = glm::vec3(cam_t[3]);
	glm::vec3 forward = -glm::vec3(cam_t[2]);
	glm::vec3 right = glm::vec3(cam_t[0]);
	glm::vec3 up = glm::vec3(cam_t[1]);

	float fov_rad = glm::radians(game_camera->fov);
	auto window_size = get_app_window_size();
	float aspect = (float)window_size.x / (float)window_size.y;

	float half_h = tanf(fov_rad * 0.5f);
	float half_w = half_h * aspect;
	float line_length = 8.0f;

	glm::vec3 corners[4] = {
		glm::normalize(forward + up * half_h + right * half_w),
		glm::normalize(forward + up * half_h - right * half_w),
		glm::normalize(forward - up * half_h + right * half_w),
		glm::normalize(forward - up * half_h - right * half_w),
	};

	Color32 col = {255, 200, 0, 255};
	for (int i = 0; i < 4; i++) {
		Debug::add_line(origin, origin + corners[i] * line_length, col, 0.f, false);
	}
}

void fpsDebugCamera::on_imgui() {
	if (!ImGui::Begin("Gameplay Debug")) {
		ImGui::End();
		return;
	}

	bool active = g_debugcamera.get_bool();
	if (ImGui::Checkbox("Debug Camera", &active))
		g_debugcamera.set_bool(active);

	bool thirdperson = g_thirdperson_orbit.get_bool();
	if (ImGui::Checkbox("Third Person (visualize pogo)", &thirdperson))
		g_thirdperson_orbit.set_bool(thirdperson);
	ImGui::SliderFloat("3p distance", &thirdperson_distance, 0.5f, 10.f);
	ImGui::SliderFloat("3p height", &thirdperson_height, -1.f, 3.f);

	ImGui::Separator();
	ImGui::Text("Time Control");

	bool paused = g_slomo.get_float() < 0.001f;
	if (ImGui::Button(paused ? "Play" : "Pause")) {
		g_slomo.set_float(paused ? 1.0f : 0.0001f);
	}

	ImGui::SameLine();
	float slomo = g_slomo.get_float();
	if (ImGui::SliderFloat("Time Scale", &slomo, 0.0001f, 5.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
		g_slomo.set_float(slomo);

	if (ImGui::Button("1x")) g_slomo.set_float(1.0f);
	ImGui::SameLine();
	if (ImGui::Button("0.5x")) g_slomo.set_float(0.5f);
	ImGui::SameLine();
	if (ImGui::Button("0.25x")) g_slomo.set_float(0.25f);
	ImGui::SameLine();
	if (ImGui::Button("0.1x")) g_slomo.set_float(0.1f);

	ImGui::End();
}
