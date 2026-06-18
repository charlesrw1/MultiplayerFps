#include "fpsDebugCamera.h"
#include "GameEnginePublic.h"
#include "Game/GameplayStatic.h"
#include "Game/Components/CameraComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "Debug.h"
#include "imgui.h"
#include <glm/gtc/matrix_transform.hpp>

ConfigVar g_debugcamera("g_debugcamera", "0", CVAR_BOOL | CVAR_DEV, "enable fly debug camera");

void fpsDebugCamera::init() {
	debug_cam_ent = GameplayStatic::spawn_entity();
	debug_cam_ent->create_component<CameraComponent>();
}

void fpsDebugCamera::shutdown() {
	
}

void fpsDebugCamera::update(EntityPtr game_camera_ent) {
	bool active = g_debugcamera.get_bool();
	auto* game_cam = game_camera_ent->get_component<CameraComponent>();
	auto* debug_cam = debug_cam_ent->get_component<CameraComponent>();

	if (active && !was_active) {
		if (!initialized_fly_cam) {
			glm::mat4 cam_t = game_camera_ent->get_ws_transform();
			fly_cam.position = glm::vec3(cam_t[3]);
			glm::vec3 forward = -glm::vec3(cam_t[2]);
			fly_cam.yaw = atan2f(forward.x, forward.z);
			fly_cam.pitch = asinf(glm::clamp(forward.y, -0.99f, 0.99f));
			fly_cam.orbit_mode = false;
			fly_cam.move_speed = 0.1f;
			initialized_fly_cam = true;
		}

		// spawn camera model to visualize the game camera
		if (!camera_model_ent.get()) {
			camera_model_ent = GameplayStatic::spawn_entity();
			auto* mesh = camera_model_ent->create_component<MeshComponent>();
			mesh->set_model_str("camera_model.cmdl");
		}
	}

	if (!active && was_active) {
		if (camera_model_ent.get()) {
			camera_model_ent->destroy();
			camera_model_ent = nullptr;
		}
	}

	// set_is_enabled(true) auto-displaces the previous scene camera
	if (active)
		debug_cam->set_is_enabled(true);
	else
		game_cam->set_is_enabled(true);

	if (active) {
		bool rmb = Input::is_mouse_down(2);
		UiSystem::inst->set_game_capture_mouse(rmb);

		if (rmb) {
			auto window_size = get_app_window_size();
			float aspect = (float)window_size.x / (float)window_size.y;
			fly_cam.update_from_input(window_size.x, window_size.y, aspect, glm::radians(game_cam->fov));
		}

		// update debug camera entity transform from fly_cam
		glm::mat4 view = fly_cam.get_view_matrix();
		glm::mat4 world = glm::inverse(view);
		debug_cam_ent->set_ws_transform(world);
		debug_cam->set_fov(game_cam->fov);

		// place camera model on the game camera
		if (camera_model_ent.get()) {
			camera_model_ent->set_ws_transform(game_camera_ent->get_ws_transform());
		}

		draw_frustum_lines(game_cam);
	}

	was_active = active;
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
