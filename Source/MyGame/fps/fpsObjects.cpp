#include "fpsObjects.h"
#include "../../Game/GameplayStatic.h"
#include "../../Game/Entities/CharacterController.h"
#include "../../Game/Components/CameraComponent.h"
#include "../../Game/Components/PhysicsComponents.h"
#include "Input/InputSystem.h"
#include "UI/GUISystemPublic.h"
#include "Framework/MathLib.h"
#include "Debug.h"
#include "imgui.h"
#include "fpsApp.h"
#include "fpsDebugCamera.h"
#include <glm/gtc/matrix_transform.hpp>

using glm::vec2;
using glm::vec3;
using glm::cross;
using glm::dot;
using glm::normalize;

// movement cvars
static float eye_height = 1.6f;
static float ground_accel = 50.f;
static float air_accel = 10.f;
static float ground_friction = 8.f;
static float max_ground_speed = 6.f;
static float max_sprint_speed = 9.f;
static float max_air_speed = 2.f;
static float jump_impulse = 5.f;
static float gravity = 18.f;
static float mouse_sens = 0.15f;

static bool is_in_debug_mode() {
	return g_debugcamera.get_bool();
}

static void fps_move_vars_menu() {
	ImGui::SliderFloat("eye_height", &eye_height, 0.5f, 2.5f);
	ImGui::SliderFloat("ground_accel", &ground_accel, 1.f, 100.f);
	ImGui::SliderFloat("air_accel", &air_accel, 1.f, 50.f);
	ImGui::SliderFloat("ground_friction", &ground_friction, 0.f, 20.f);
	ImGui::SliderFloat("max_ground_speed", &max_ground_speed, 1.f, 20.f);
	ImGui::SliderFloat("max_sprint_speed", &max_sprint_speed, 1.f, 25.f);
	ImGui::SliderFloat("max_air_speed", &max_air_speed, 0.f, 10.f);
	ImGui::SliderFloat("jump_impulse", &jump_impulse, 0.f, 20.f);
	ImGui::SliderFloat("gravity", &gravity, 0.f, 40.f);
	ImGui::SliderFloat("mouse_sens", &mouse_sens, 0.01f, 1.f);
}
ADD_TO_DEBUG_MENU(fps_move_vars_menu);

fpsPlayer::fpsPlayer() = default;
// Out-of-line so CharacterController (forward-declared in fpsObjects.h) is a complete type
// wherever this destructor gets instantiated, including from the codegen'd MEGA translation unit.
fpsPlayer::~fpsPlayer() = default;

void fpsPlayer::start() {
	auto* capsule = get_owner()->get_component<CapsuleComponent>();
	ASSERT(capsule);

	controller = std::make_unique<CharacterController>(capsule);
	controller->set_position(get_ws_position());
	controller->capsule_height = capsule->height;
	controller->capsule_radius = capsule->radius;

	Entity* cament = GameplayStatic::spawn_entity();
	cament->create_component<CameraComponent>();
	this->camera = cament;

	UiSystem::inst->set_game_capture_mouse(true);
}

void fpsPlayer::stop() {
	controller.reset();
}

void fpsPlayer::manualtick() {
	update_look();
	update_movement();
	update_camera();
}

void fpsPlayer::update_look() {
	if (is_in_debug_mode())
		return;

	if (!UiSystem::inst->is_game_capturing_mouse()) {
		if (Input::was_mouse_pressed(0) && !UiSystem::inst->blocking_mouse_inputs())
			UiSystem::inst->set_game_capture_mouse(true);
		return;
	}

	if (Input::was_key_pressed(SDL_SCANCODE_ESCAPE)) {
		UiSystem::inst->set_game_capture_mouse(false);
		return;
	}

	auto mouse = Input::get_mouse_delta();
	float sens = mouse_sens * 0.01f;

	view_yaw += mouse.x * sens;
	view_pitch -= mouse.y * sens;

	view_pitch = glm::clamp(view_pitch, -HALFPI + 0.01f, HALFPI - 0.01f);
	view_yaw = glm::mod(view_yaw, TWOPI);
}

void fpsPlayer::update_movement() {
	float dt = (float)eng->get_dt();
	const bool has_focus = UiSystem::inst->is_game_capturing_mouse() && !is_in_debug_mode();

	// input (only when mouse is captured)
	vec2 input_dir{};
	if (has_focus) {
		if (Input::is_key_down(SDL_SCANCODE_W)) input_dir.x += 1.f;
		if (Input::is_key_down(SDL_SCANCODE_S)) input_dir.x -= 1.f;
		if (Input::is_key_down(SDL_SCANCODE_A)) input_dir.y -= 1.f;
		if (Input::is_key_down(SDL_SCANCODE_D)) input_dir.y += 1.f;
	}

	float input_len = glm::length(input_dir);
	if (input_len > 1.f)
		input_dir /= input_len;

	bool sprinting = has_focus && Input::is_key_down(SDL_SCANCODE_LSHIFT);

	// build wish direction in world space
	vec3 forward = AnglesToVector(view_pitch, view_yaw);
	vec3 flat_forward = normalize(vec3(forward.x, 0.f, forward.z));
	vec3 right = normalize(cross(flat_forward, vec3(0, 1, 0)));

	vec3 wishdir = flat_forward * input_dir.x + right * input_dir.y;
	float wishlen = glm::length(wishdir);
	if (wishlen > 0.001f)
		wishdir /= wishlen;

	// friction (ground only)
	if (on_ground) {
		float speed = glm::length(vec2(velocity.x, velocity.z));
		if (speed > 0.001f) {
			float drop = ground_friction * speed * dt;
			float factor = glm::max(speed - drop, 0.f) / speed;
			velocity.x *= factor;
			velocity.z *= factor;
		}
	}

	// acceleration (quake-style)
	float max_speed = on_ground ? (sprinting ? max_sprint_speed : max_ground_speed) : max_air_speed;
	float accel_val = on_ground ? ground_accel : air_accel;

	vec3 xz_vel = vec3(velocity.x, 0.f, velocity.z);
	float current_speed = dot(xz_vel, wishdir);
	float add_speed = glm::max(max_speed * wishlen - current_speed, 0.f);
	float accel_speed = glm::min(accel_val * max_speed * dt, add_speed);
	velocity.x += accel_speed * wishdir.x;
	velocity.z += accel_speed * wishdir.z;

	// stop threshold
	if (on_ground && wishlen < 0.01f) {
		float hz_speed = glm::length(vec2(velocity.x, velocity.z));
		if (hz_speed < 0.3f) {
			velocity.x = 0.f;
			velocity.z = 0.f;
		}
	}

	// jump
	if (has_focus && on_ground && Input::was_key_pressed(SDL_SCANCODE_SPACE))
		velocity.y = jump_impulse;

	// gravity
	velocity.y -= gravity * dt;

	// move via character controller
	int flags = 0;
	vec3 out_vel;
	controller->move(velocity * dt, dt, 0.001f, flags, out_vel);
	velocity = out_vel;
	on_ground = (flags & CCCF_BELOW) != 0;

	get_owner()->set_ws_position(controller->get_character_pos());
}

void fpsPlayer::update_camera() {
	vec3 eye = get_ws_position() + vec3(0, eye_height, 0);
	vec3 forward = AnglesToVector(view_pitch, view_yaw);
	glm::mat4 view = glm::lookAt(eye, eye + forward, vec3(0, 1, 0));
	camera->set_ws_transform(glm::inverse(view));
}
#include "../../Render/Model.h"
void fpsPropPhysics::editor_start() {
	editor_on_change_property();
}
void fpsPropPhysics::editor_on_change_property() {
	if (model)
		editor_set_model(model->get_name(), false);
	else
		editor_set_model("", true);
}
#include "../../Game/Components/MeshComponent.h"
#include "../../Game/Components/PhysicsComponents.h"
void fpsPropPhysics::start() {
	if (!model)
		sys_print(Warning, "fpsPropPhysics without physics\n");
	else
	{
		auto* mesh = get_owner()->create_component<MeshComponent>();
		mesh->set_model(model);
		auto* physics = get_owner()->get_component<PhysicsBody>();
		if (!physics) {
			physics = get_owner()->create_component<MeshColliderComponent>();
		}
		physics->set_body_type(BodyType::Dynamic);
		physics->set_physics_layer(PL::PhysicsObject);
	}
}

void fpsInventoryLogic::update()
{
	// update item transitions
	// update current item
	//		- updates state machine and also viewmodel

}

void fpsFlickeringLightScript::start() {
	auto* light = get_owner()->create_component<PointLightComponent>();
	set_ticking(true);
}

void fpsFlickeringLightScript::update() {
	auto* light = get_owner()->get_component<PointLightComponent>();
	light->color = color;
	light->intensity = evaluate_intsensity();
	light->sync_render_data();
	light->set_radius(radius);
}

float fpsFlickeringLightScript::evaluate_intsensity() {
	const int numoctaves_to_use = glm::min(octaves, 6);
	const float time = eng->get_game_time();
	float sum = 0.0;
	float amplitude = 1.f;
	for (int i = 0; i < numoctaves_to_use; i++) {
		sum += glm::perlin(glm::vec2(time * frequency + offset, 31.7 * i)) * amplitude;
		amplitude *= 0.5f;
	}
	sum = sum * 0.5 + 0.5;
	sum = glm::clamp(sum, 0.f, 1.f);
	return glm::mix(min_intensity, max_intensity, sum);
}
