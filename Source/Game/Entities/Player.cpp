#include "Player.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"
#include "Framework/Config.h"

#include "GameEnginePublic.h"
#include "imgui.h"

#include "Render/DrawPublic.h"

#include "Debug.h"

#include "Assets/AssetDatabase.h"

#include "Level.h"

#include "Physics/Physics2.h"
#include "Physics/ChannelsAndPresets.h"

#include "Framework/ClassTypePtr.h"
#include "Render/Texture.h"

#include "Input/InputSystem.h"

#include "UI/GUISystemPublic.h"
#include "UI/Widgets/Layouts.h"

#include <SDL3/SDL_events.h>

#include "Game/GameModes/MainMenuMode.h"

#include "CharacterController.h"

#include "BikeEntity.h"

#include "Game/Components/BillboardComponent.h"
#include "Game/Components/ArrowComponent.h"

//
//	PLAYER MOVEMENT CODE
//

// static float fall_speed_threshold = -0.05f;
// static float grnd_speed_threshold = 0.6f;
//
// static float ground_friction = 8.2;
// static float air_friction = 0.01;
// static float ground_accel = 6;
// static float ground_accel_crouch = 4;
// static float air_accel = 3;
// static float jumpimpulse = 5.f;
// static float max_ground_speed = 5.7;
// static float max_sprint_speed = 8.0;
// static float sprint_accel = 8;
// static float max_air_speed = 2;

// void move_variables_menu()
//{
//	ImGui::SliderFloat("ground_friction", &ground_friction, 0, 20);
//	ImGui::SliderFloat("air_friction", &air_friction, 0, 10);
//	ImGui::SliderFloat("ground_accel", &ground_accel, 1, 10);
//	ImGui::SliderFloat("air_accel", &air_accel, 0, 10);
//	ImGui::SliderFloat("max_ground_speed", &max_ground_speed, 2, 20);
//	ImGui::SliderFloat("max_air_speed", &max_air_speed, 0, 10);
//	ImGui::SliderFloat("jumpimpulse", &jumpimpulse, 0, 20);
//}

using glm::cross;
using glm::dot;
using glm::vec2;
using glm::vec3;

float lensquared_noy(vec3 v) {
	return v.x * v.x + v.z * v.z;
}

// static AddToDebugMenu addmovevars("move vars", move_variables_menu);
#include "Sound/SoundPublic.h"

template <typename T> T neg_modulo(T x, T mod_) {
	return glm::mod(glm::mod(x, mod_) + mod_, mod_);
}
static float modulo_lerp_(float start, float end, float mod, float t) {
	return neg_modulo(t - start, mod) / neg_modulo(end - start, mod);
}

void Player::find_a_spawn_point() {
	// InlineVec<PlayerSpawnPoint*, 16> points;
	// if (!eng->get_level()->find_all_entities_of_class(points))
	//	sys_print(Error, "no spawn points");
	// else {
	//	auto pos = points[0]->get_ws_position();
	//
	//	set_ws_position(pos);
	//}
}

glm::vec3 Player::calc_eye_position() {
	ASSERT(get_owner());
	float view_height = 0.0; // (is_crouching) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
	return get_ws_position() + vec3(0, view_height, 0);
}

float bike_view_damp = 0.01;
void bike_view_menu() {
	ImGui::InputFloat("bike_view_damp", &bike_view_damp);
}
ADD_TO_DEBUG_MENU(bike_view_menu);

void Player::get_view(glm::mat4& viewMat, float& fov) {
	ASSERT(get_owner());
#if 0
	if (g_thirdperson.get_bool()) {

		auto dir = bike->bike_direction;
		auto pos = get_ws_position();
		auto camera_pos = glm::vec3(pos.x, 6.0, pos.z - 6);
		auto dir_mat = glm::lookAt(glm::vec3(0), glm::vec3(dir.x, -0.1f, dir.z), glm::vec3(0, 1, 0));
		auto dir_quat = glm::quat(dir_mat);
		this->view_quat = damp_dt_independent(dir_quat, this->view_quat, bike_view_damp, eng->get_dt());
		auto actual_dir = glm::inverse(this->view_quat) * glm::vec3(0, 0, 1);

		vec3 front = dir; // AnglesToVector(view_angles.x, view_angles.y);
		vec3 side = normalize(cross(front, vec3(0, 1, 0)));
		camera_pos = get_ws_position() + vec3(0, 3.0, 0) - front * 2.5f + side * 0.f;

		this->view_pos = damp_dt_independent(camera_pos, this->view_pos, bike_view_damp, eng->get_dt());

		// viewMat = glm::lookAt(camera_pos, vec3(pos.x,0,pos.z), glm::vec3(0, 1, 0));

		viewMat = glm::lookAt(this->view_pos, this->view_pos - actual_dir, glm::vec3(0, 1, 0));

		// org = camera_pos;
		// ang = view_angles;
		fov = g_fov.get_float();
	} else {
		vec3 cam_position = calc_eye_position();
		vec3 front = AnglesToVector(view_angles.x, view_angles.y);

		viewMat = glm::lookAt(cam_position, cam_position + front, glm::vec3(0, 1, 0));
		// org = cam_position;
		// ang = view_angles;
		fov = g_fov.get_float();
	}
#endif
}

glm::vec3 GetRecoilAmtTriangle(glm::vec3 maxrecoil, float t, float peakt) {
	float p = (1 / (peakt - 1));

	if (t < peakt)
		return maxrecoil * (1 / peakt) * t;
	else
		return maxrecoil * (p * t - p);
}


void Player::on_jump_callback() {
	ASSERT(ccontroller);
	static int i = 0;
	if (is_on_ground())
		velocity.y += 5.0;
	else if (wall_jump_cooldown <= 0.0) {
		glm::vec2 stick = {}; // glm::vec2(cmd.forward_move, cmd.lateral_move);
		if (glm::length(stick) > 0.7) {

			vec3 look_front = AnglesToVector(view_angles.x, view_angles.y);
			look_front.y = 0;
			look_front = normalize(look_front);
			vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

			vec3 wishdir = (look_front * stick.x + look_side * stick.y);
			wishdir = vec3(wishdir.x, 0.f, wishdir.z);

			if (dot(wishdir, velocity) >= 0)
				return;

			world_query_result wqr;
			auto pos = get_ws_position() + glm::vec3(0, 0.7, 0);
			const float test_len = ccontroller->capsule_radius + 0.4;
			bool good = g_physics.trace_ray(wqr, pos, pos - wishdir * test_len, nullptr, UINT32_MAX);
			if (good) {
				velocity = wqr.hit_normal * 8.0f;
				velocity.y = 4.5;
				sys_print(Debug, "wall jump\n");

				wall_jump_cooldown = 0.2;
			}
		}
	}
}

void Player::update() {
#if 0
	vec2 moveAction = {};
	moveAction.x = Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX);
	moveAction.y = Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY);
	vec2 lookAction = {};
	lookAction.x = Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX);
	lookAction.y = Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY);
	float accelAction = Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

	bike->forward_strength = accelAction;
	bike->turn_strength = moveAction.x;
	{
		auto off = lookAction;
		view_angles.x -= off.y; // pitch
		view_angles.y += off.x; // yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	// set_ws_position(bike->get_ws_position());
#endif
}

void Player::on_foot_update() {
	ASSERT(ccontroller);
	if (wall_jump_cooldown > 0)
		wall_jump_cooldown -= eng->get_dt();

	vec2 moveAction = {};
	moveAction.x = Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTX);
	moveAction.y = Input::get_con_axis(SDL_CONTROLLER_AXIS_LEFTY);
	vec2 lookAction = {};
	lookAction.x = Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTX);
	lookAction.y = Input::get_con_axis(SDL_CONTROLLER_AXIS_RIGHTY);
	float accelAction = Input::get_con_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT);

	//

	is_crouching = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_Y);
	{
		auto off = lookAction;
		view_angles.x -= off.y; // pitch
		view_angles.y += off.x; // yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	{
		auto move = moveAction;
		float length = glm::length(move);
		if (length > 1.0)
			move /= length;

		// cmd.forward_move = move.y;
		// cmd.lateral_move = move.x;
		// printf("%f %f %f\n", move.x, move.y,length);
	}

	if (Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_A))
		on_jump_callback();

	const bool is_sprinting = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	float friction_value = 0.0; // (is_on_ground()) ? ground_friction : air_friction;
	float speed = glm::length(velocity);

	if (speed >= 0.0001) {
		float dropamt = friction_value * speed * eng->get_dt();
		float newspd = speed - dropamt;
		if (newspd < 0)
			newspd = 0;
		float factor = newspd / speed;
		velocity.x *= factor;
		velocity.z *= factor;
	}

	vec2 inputvec = {}; // vec2(cmd.forward_move, cmd.lateral_move);
	float inputlen = length(inputvec);
	// if (inputlen > 0.00001)
	//	inputvec = inputvec / inputlen;
	// if (inputlen > 1)
	//	inputlen = 1;

	vec3 look_front = AnglesToVector(view_angles.x, view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

	const bool player_on_ground = is_on_ground();
	float acceleation_val = 0.5; // (player_on_ground) ?
								 //((is_sprinting) ? sprint_accel : ground_accel) :
	//	air_accel;
	// acceleation_val = (is_crouching) ? ground_accel_crouch : acceleation_val;

	float maxspeed_val = 1.0; // (player_on_ground) ?
	//	((is_sprinting) ? max_sprint_speed : max_ground_speed) :
	//	max_air_speed;

	vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(velocity.x, 0, velocity.z);

	float wishspeed = inputlen * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed * eng->get_dt();
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	// if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	velocity = vec3(xz_velocity.x, velocity.y, xz_velocity.z);

	velocity.y -= 10.0 * eng->get_dt();

	int flags = 0;

	glm::vec3 out_vel;
	ccontroller->move(velocity * (float)eng->get_dt(), eng->get_dt(), 0.001f, flags, out_vel);

	velocity = out_vel;
	action = (flags & CCCF_BELOW) ? Action_State::Idle : Action_State::Falling;

	get_owner()->set_ws_position(ccontroller->get_character_pos());

	if (flags & CCCF_BELOW)
		Debug::add_box(ccontroller->get_character_pos(), glm::vec3(0.5), COLOR_RED, 0);
	if (flags & CCCF_ABOVE)
		Debug::add_box(ccontroller->get_character_pos() + glm::vec3(0, ccontroller->capsule_height, 0), glm::vec3(0.5),
					   COLOR_GREEN, 0.5);

	auto line_start = ccontroller->get_character_pos() + glm::vec3(0, 0.5, 0);
	Debug::add_line(line_start, line_start + velocity * 3.0f, COLOR_CYAN, 0);

	Debug::add_line(line_start, line_start + wishdir * 2.0f, COLOR_RED, 0);
}

#include "BikeEntity.h"

void Player::start() {
	ASSERT(get_owner());

	UiSystem::inst->set_game_capture_mouse(true);

	//
	//	 {
	//		 std::vector<const InputDevice*> devices;
	//		 g_inputSys.get_connected_devices(devices);
	//		 int deviceIdx = 0;
	//		 for (; deviceIdx < devices.size(); deviceIdx++) {
	//			 if (devices[deviceIdx]->type == InputDeviceType::Controller) {
	//				 inputPtr->assign_device(devices[deviceIdx]->selfHandle);
	//				 break;
	//			 }
	//		 }
	//		 if (deviceIdx == devices.size())
	//			 inputPtr->assign_device(g_inputSys.get_keyboard_device_handle());
	//
	//		 g_inputSys.device_connected.add(this, [&](handle<InputDevice> handle)
	//			 {
	//
	//				 inputPtr->assign_device(handle);
	//			 });
	//
	//		 inputPtr->on_lost_device.add(this, [&]()
	//			 {
	//				inputPtr->assign_device(g_inputSys.get_keyboard_device_handle());
	//			 });
	//	 }
	//

	// inputPtr->get("ui/menu")->bind_start_function([this] {
	//		 if(hud)
	//			 hud->toggle_menu_mode();
	//	 });

	Player::find_a_spawn_point();

	ccontroller = std::make_unique<CharacterController>(player_capsule);
	ccontroller->set_position(get_ws_position());
	ccontroller->capsule_height = player_capsule->height;
	ccontroller->capsule_radius = player_capsule->radius;

	score_update_delegate.invoke(10);

	//{
	//	 auto scope = eng->get_level()->spawn_entity<BikeEntity>(bike);
	//	 bike->get_owner()->set_ws_position(get_ws_position());
	//}
	// get_owner()->set_ws_position(glm::vec3(0, 0, 0.5));
	// get_owner()->parent_to(bike->get_owner());
}
void Player::stop() {}

Player::~Player() {}

Player::Player() {
	return;
	// player_mesh = construct_sub_component<MeshComponent>("CharMesh");
	// player_capsule = construct_sub_component<CapsuleComponent>("CharCapsule");
	// spotlight = construct_sub_component<SpotLightComponent>("Flashlight");
	// health = construct_sub_component<HealthComponent>("PlayerHealth");

	// auto playerMod = g_assets.find_assetptr_unsafe<Model>("SWAT_model.cmdl");
	// player_mesh->set_model(playerMod);
	// player_mesh->set_animation_graph("ik_test.ag");
	// player_mesh->set_is_visible(false);
	//
	// player_capsule->set_is_trigger(true);
	// player_capsule->set_send_overlap(true);
	// player_capsule->set_is_enable(false);
	// player_capsule->set_body_type(BodyType::Kinematic);
	// player_capsule->height = 1.7;
	// player_capsule->radius = 0.3;

	set_ticking(true);
}

// GameplayStatic methods -> Player_GameplayStatic.cpp
// CameraPathFollower / CamPathFollowerLua -> Player_CameraPath.cpp
