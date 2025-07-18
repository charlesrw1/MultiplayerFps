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
#include "Input/InputAction.h"

#include "UI/GUISystemPublic.h"
#include "UI/Widgets/Layouts.h"


#include <SDL2/SDL_events.h>

#include "Game/GameModes/MainMenuMode.h"

#include "CharacterController.h"

#include "BikeEntity.h"







#include "Game/Components/BillboardComponent.h"
#include "Game/Components/ArrowComponent.h"




//
//	PLAYER MOVEMENT CODE
//


//static float fall_speed_threshold = -0.05f;
//static float grnd_speed_threshold = 0.6f;
//
//static float ground_friction = 8.2;
//static float air_friction = 0.01;
//static float ground_accel = 6;
//static float ground_accel_crouch = 4;
//static float air_accel = 3;
//static float jumpimpulse = 5.f;
//static float max_ground_speed = 5.7;
//static float max_sprint_speed = 8.0;
//static float sprint_accel = 8;
//static float max_air_speed = 2;

//void move_variables_menu()
//{
//	ImGui::SliderFloat("ground_friction", &ground_friction, 0, 20);
//	ImGui::SliderFloat("air_friction", &air_friction, 0, 10);
//	ImGui::SliderFloat("ground_accel", &ground_accel, 1, 10);
//	ImGui::SliderFloat("air_accel", &air_accel, 0, 10);
//	ImGui::SliderFloat("max_ground_speed", &max_ground_speed, 2, 20);
//	ImGui::SliderFloat("max_air_speed", &max_air_speed, 0, 10);
//	ImGui::SliderFloat("jumpimpulse", &jumpimpulse, 0, 20);
//}

using glm::vec3;
using glm::vec2;
using glm::dot;
using glm::cross;




float lensquared_noy(vec3 v)
{
	return v.x * v.x + v.z * v.z;
}



//static AddToDebugMenu addmovevars("move vars", move_variables_menu);
#include "Sound/SoundPublic.h"


template<typename T>
T neg_modulo(T x, T mod_)
{
	return glm::mod(glm::mod(x, mod_) + mod_, mod_);
}
static float modulo_lerp_(float start, float end, float mod, float t)
{
	return neg_modulo(t - start, mod) / neg_modulo(end - start, mod);
}




void Player::find_a_spawn_point()
{
	//InlineVec<PlayerSpawnPoint*, 16> points;
	//if (!eng->get_level()->find_all_entities_of_class(points))
	//	sys_print(Error, "no spawn points");
	//else {
	//	auto pos = points[0]->get_ws_position();
	//
	//	set_ws_position(pos);
	//}
}

glm::vec3 Player::calc_eye_position()
{
	float view_height = 0.0;// (is_crouching) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
	return get_ws_position() + vec3(0, view_height, 0);
}

float bike_view_damp = 0.01;
void bike_view_menu()
{
	ImGui::InputFloat("bike_view_damp", &bike_view_damp);
}
ADD_TO_DEBUG_MENU(bike_view_menu);

void Player::get_view(glm::mat4& viewMat, float& fov)
{
	if (g_thirdperson.get_bool()) {

		auto dir = bike->bike_direction;
		auto pos = get_ws_position();
		auto camera_pos = glm::vec3(pos.x, 6.0, pos.z - 6);
		auto dir_mat = glm::lookAt(glm::vec3(0), glm::vec3(dir.x,-0.1f,dir.z), glm::vec3(0, 1, 0));
		auto dir_quat = glm::quat(dir_mat);
		this->view_quat = damp_dt_independent(dir_quat, this->view_quat, bike_view_damp, eng->get_dt());
		auto actual_dir = glm::inverse(this->view_quat) * glm::vec3(0, 0, 1);

		vec3 front = dir;// AnglesToVector(view_angles.x, view_angles.y);
		vec3 side = normalize(cross(front, vec3(0, 1, 0)));
		camera_pos = get_ws_position() + vec3(0, 3.0, 0) - front * 2.5f + side * 0.f;

		this->view_pos = damp_dt_independent(camera_pos, this->view_pos, bike_view_damp, eng->get_dt());

		//viewMat = glm::lookAt(camera_pos, vec3(pos.x,0,pos.z), glm::vec3(0, 1, 0));

		viewMat = glm::lookAt(this->view_pos, this->view_pos - actual_dir, glm::vec3(0, 1, 0));

		//org = camera_pos;
		//ang = view_angles;
		fov = g_fov.get_float();
	}
	else
	{
		vec3 cam_position = calc_eye_position();
		vec3 front = AnglesToVector(view_angles.x, view_angles.y);

		viewMat = glm::lookAt(cam_position, cam_position + front, glm::vec3(0, 1, 0));
		//org = cam_position;
		//ang = view_angles;
		fov = g_fov.get_float();
	}
}


glm::vec3 GetRecoilAmtTriangle(glm::vec3 maxrecoil, float t, float peakt)
{
	float p = (1 / (peakt - 1));

	if (t < peakt)
		return maxrecoil * (1 / peakt) * t;
	else
		return maxrecoil * (p * t - p);

}


static void update_viewmodel(glm::vec3 view_angles, bool crouching, glm::vec3& viewmodel_offsets)
{
#if 0
	Entity& e = *eng->get_local_player();
	glm::vec3 velocity = e.get_velocity();
	glm::vec3 view_front = AnglesToVector(view_angles.x, view_angles.y);
	view_front.y = 0;
	glm::vec3 side_grnd = glm::normalize(glm::cross(view_front, vec3(0, 1, 0)));
	float spd_side = dot(side_grnd, velocity);
	float side_ofs_ideal = -spd_side / 200.f;
	glm::clamp(side_ofs_ideal, -0.007f, 0.007f);
	float spd_front = dot(view_front, velocity);
	float front_ofs_ideal = spd_front / 200.f;
	glm::clamp(front_ofs_ideal, -0.007f, 0.007f);
	float up_spd = velocity.y;
	float up_ofs_ideal = -up_spd / 200.f;
	glm::clamp(up_ofs_ideal, -0.007f, 0.007f);

	if (crouching)
		up_ofs_ideal += 0.04;

	viewmodel_offsets = damp(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.01f, eng->get_frame_time() * 100.f);

#endif
}
#if 0
void ViewmodelComponent::update()
 {
	static bool first = true;
	if (first) {
		first = false;
		Debug_Interface::get()->add_hook("vm", vm_menu);
	}

	update_viewmodel(player->view_angles, player->is_crouching, viewmodel_offsets);
	glm::vec3 velocty = player->get_velocity();
	bool crouching = player->is_crouching;
	float speed = glm::length(glm::vec2(velocty.x, velocty.z));
	float speed_mult = glm::smoothstep(0.f, 2.5f, speed);
	float x = cos(GetTime()*move_a)*move_b*speed_mult;
	float y = pow(cos(GetTime() * move_c ),4.f) * move_d*speed_mult;
	glm::vec3 target_pos = vec3(x, y, 0.f);

	glm::vec2 face_dir = glm::vec2(cos(HALFPI-player->rotation.y), sin(HALFPI-player->rotation.y));
	glm::vec2 side = glm::vec2(-face_dir.y, face_dir.x);

	vec2 grnd_accel = vec2(player->esimated_accel.x, player->esimated_accel.z);
	vec2 grnd_vel = vec2(velocty.x, velocty.z);
	vec2 rel_move_dir = glm::vec2(glm::dot(face_dir, grnd_accel), glm::dot(side, grnd_accel));
	vec2 rel_vel_dir = vec2(dot(face_dir, grnd_vel), dot(side, grnd_vel));
	float len = glm::length(rel_move_dir); 
	if (len >= 0.0001f)
	{
		target_pos += vec3(rel_move_dir.y, 0.f, rel_move_dir.x) / len * glm::smoothstep(0.f, 2.5f, len) * move_f;
	}
	
	//target_pos.y += glm::smoothstep(0.f,4.f, player->esimated_accel.y) * move_f*4.f;

	glm::quat target_rot = glm::quat(1, 0, 0, 0);
	len = glm::length(rel_vel_dir);
//	if (len >= 0.0001f) {
		float s = glm::smoothstep(-3.f, 3.f,rel_move_dir.y*0.1f) -0.5f;
		target_rot *= glm::quat(vec3(s*0.7, 1.f, 0.f), vec3(0.f, 1.f, 0.f));
//	}

	vec3 view_dir = AnglesToVector(player->view_angles.x,player->view_angles.y);
	vec3 sidedir = normalize(cross(view_dir,vec3(0,1,0)));
	vec3 up = cross(view_dir, sidedir);
	float deltax = dot(view_dir - last_view_dir, sidedir);
	float deltay = dot(view_dir - last_view_dir, up);
	last_view_dir = view_dir;

	target_rot *= glm::quat(glm::vec3(deltax*move_e, deltay*move_e, 1.f), vec3(0, 0, 1));


	lastoffset = damp_dt_independent(target_pos, lastoffset, move_g, eng->tick_interval);
	lastrot = damp_dt_independent(target_rot, lastrot, move_g*0.1, eng->tick_interval);

	//if(animator.m.finished || animator.m.anim == -1)
	//	animator.set_anim("ak47_idle", false);
	

	//animator.update(eng->tick_interval);

	//animator.AdvanceFrame(eng->tick_interval);
	//animator.SetupBones();
	//animator.ConcatWithInvPose();
	// find position/rotation of hands in current animation relative to the gun bone
	// rotate/translate weapon bone of viewmodel according to procedural gen
	// ik hands back to correct position relative to gun bone
}

// player update
//		for all buffered commands
//			run movement code
//			run update code
//			if fire_bullets and is server and not local player:
//				for all lagcomped objects
//					rollback to state
//				run raycast
//				restore objects

// server/host side game tick logic
//		for all objects
//			update()
//		update physicsworld data
//		simulate_physics()
//		for all objects
//			postphysics()
//		
//		for all animated objects
//			update animations
//			pre_ik_update()
//			update ik and finalize
//		
//		for all lagcomped objects
//			save positon/hitbox data

// client high level
//		send move commands
//		recieve state snapshot
//		predict+update world
//		interpolate entities

// client side update game logic
//		for all objects
//			client update
#endif

#if 0
class PlayerHUD : public GUIFullscreen
{
public:

	void on_enter_pause_menu() {
		scoreWidget->hidden = true;

		paused_menu->hidden = false;
		pause_menu_outline->hidden = false;

		eng->set_game_focused(false);

		p->inputPtr->disable_mapping("game");

		is_in_pause = true;
	}
	void on_leave_pause_menu() {
		scoreWidget->hidden = false;

		paused_menu->hidden = true;
		pause_menu_outline->hidden = true;

		eng->set_game_focused(true);

		p->inputPtr->enable_mapping("game");

		is_in_pause = false;
	}

	static GUIButtonWithSound* create_button(const char* textstr, const SoundFile* s) {
		GUIButtonWithSound* button = new GUIButtonWithSound(s);
		button->padding = { 5,5,5,5 };
		button->w_alignment = GuiAlignment::Fill;

		GUIText* text = new GUIText;
		text->text = textstr;
		text->color = COLOR_WHITE;
		text->ls_position = { 0,0 };
		text->w_alignment = GuiAlignment::Center;
		text->h_alignment = GuiAlignment::Center;
		text->padding = { 10,10,10,10 };


		button->add_this(text);

		return button;
	}



	GUIVerticalBox* create_menu() {
		GUIVerticalBox* vbox = new GUIVerticalBox;
		vbox->ls_position = { 0,0 };
		vbox->ls_sz = { 200,500 };
		vbox->pivot_ofs = { 0.5,0.5 };
		vbox->anchor = UIAnchorPos::create_single(0.5, 0.5);

		GUIText* paused_text = new GUIText;
		paused_text->text = "PAUSED";
		paused_text->padding = { 5,30,5,30 };
		paused_text->h_alignment = GuiAlignment::Center;
		paused_text->w_alignment = GuiAlignment::Center;
		paused_text->color = COLOR_WHITE;
		vbox->add_this(paused_text);

		const SoundFile* s = g_assets.find_global_sync<SoundFile>("switch2.wav").get();
		auto b = create_button("TO MAIN MENU", s);
		b->on_selected.add(this, [] {
				Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "map mainMenuMap.tmap");
			});
		vbox->add_this(b);
		b = create_button("CONTINUE", s);
		b->on_selected.add(this, [this] {
				on_leave_pause_menu();
			});
		vbox->add_this(b);

		return vbox;
	}

	bool is_in_pause = false;

	PlayerHUD(Player* p) : p(p){
		recieve_events = true;

		pause_menu_outline = new GUIBox;
		pause_menu_outline->color = { 255,200,200,60 };
		pause_menu_outline->anchor = UIAnchorPos::create_single(0.5, 0.5);
		pause_menu_outline->ls_sz = { 200,1500 };
		pause_menu_outline->pivot_ofs = { 0.5,0.5 };
		pause_menu_outline->hidden = true;
		add_this(pause_menu_outline);
		paused_menu = create_menu();
		add_this(paused_menu);

		p->score_update_delegate.add(this, &PlayerHUD::on_set_score);

		scoreWidget = new GUIText;
		scoreWidget->anchor.positions[0][1] = 128;
		scoreWidget->anchor.positions[1][0] = 128;
		scoreWidget->anchor.positions[0][0] = 128;
		scoreWidget->anchor.positions[1][1] = 128;
		scoreWidget->pivot_ofs = { 0.5,0.5 };
		scoreWidget->use_desired_size = true;

		add_this(scoreWidget);

		boxWidget = new GUIBox;
		boxWidget->ls_position = { 0,0 };
		boxWidget->ls_sz = { 200,200 };
		boxWidget->color = COLOR_CYAN;
		add_this(boxWidget);

		eng->get_gui()->add_gui_panel_to_root(this);

		eng->get_gui()->set_focus_to_this(this);

		on_leave_pause_menu();
	}
	~PlayerHUD() {
		unlink_and_release_from_parent();
	}

	void on_set_score(int count) {
		scoreWidget->text = "Score: " + std::to_string(count);
	}
	void on_pressed(int x, int y, int b) override {
		
	}

	void toggle_menu_mode() {
		if (is_in_pause)
			on_leave_pause_menu();
		else
			on_enter_pause_menu();
	}

	void on_key_down(const SDL_KeyboardEvent& k) override {
		
	}

	GUIVerticalBox* paused_menu = nullptr;
	GUIBox* pause_menu_outline = nullptr;

	GUIText* scoreWidget = nullptr;
	GUIBox* boxWidget = nullptr;

	Player* p = nullptr;
};
#endif


void Player::on_jump_callback()
{
	static int i = 0;
	if(is_on_ground())
		velocity.y += 5.0;
	else if(wall_jump_cooldown<=0.0){
		glm::vec2 stick = {};// glm::vec2(cmd.forward_move, cmd.lateral_move);
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
			const float test_len = ccontroller->capsule_radius+0.4;
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

void Player::update()
{
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
		view_angles.x -= off.y;	// pitch
		view_angles.y += off.x;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	//set_ws_position(bike->get_ws_position());
}

void Player::on_foot_update()
{
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
		view_angles.x -= off.y;	// pitch
		view_angles.y += off.x;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	{
		auto move = moveAction;
		float length = glm::length(move);
		if (length > 1.0)
			move /= length;

		//cmd.forward_move = move.y;
		//cmd.lateral_move = move.x;
		//printf("%f %f %f\n", move.x, move.y,length);

	}

	if(Input::was_con_button_pressed(SDL_CONTROLLER_BUTTON_A))
		on_jump_callback();


	const bool is_sprinting = Input::is_con_button_down(SDL_CONTROLLER_BUTTON_X);

	float friction_value = 0.0;// (is_on_ground()) ? ground_friction : air_friction;
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

	vec2 inputvec = {};// vec2(cmd.forward_move, cmd.lateral_move);
	float inputlen = length(inputvec);
	//if (inputlen > 0.00001)
	//	inputvec = inputvec / inputlen;
	//if (inputlen > 1)
	//	inputlen = 1;

	vec3 look_front = AnglesToVector(view_angles.x, view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

	const bool player_on_ground =  is_on_ground();
	float acceleation_val = 0.5;// (player_on_ground) ?
		//((is_sprinting) ? sprint_accel : ground_accel) :
	//	air_accel;
	//acceleation_val = (is_crouching) ? ground_accel_crouch : acceleation_val;


	float maxspeed_val = 1.0;// (player_on_ground) ?
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
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	velocity = vec3(xz_velocity.x, velocity.y, xz_velocity.z);
	
	velocity.y -= 10.0 * eng->get_dt();

	int flags = 0;

	glm::vec3 out_vel;
	ccontroller->move(velocity*(float)eng->get_dt(), eng->get_dt(), 0.001f, flags, out_vel);
	
	velocity = out_vel;
	action = (flags & CCCF_BELOW) ? Action_State::Idle : Action_State::Falling;

	get_owner()->set_ws_position(ccontroller->get_character_pos());

	if (flags & CCCF_BELOW)
		Debug::add_box(ccontroller->get_character_pos(), glm::vec3(0.5), COLOR_RED, 0);
	if (flags & CCCF_ABOVE)
		Debug::add_box(ccontroller->get_character_pos() + glm::vec3(0, ccontroller->capsule_height, 0), glm::vec3(0.5), COLOR_GREEN, 0.5);

	auto line_start = ccontroller->get_character_pos() + glm::vec3(0, 0.5, 0);
	Debug::add_line(line_start, line_start + velocity * 3.0f, COLOR_CYAN,0);

	Debug::add_line(line_start, line_start + wishdir * 2.0f, COLOR_RED, 0);
}

#include "BikeEntity.h"

 void Player::start()  {

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

	 {
		 auto scope = eng->get_level()->spawn_entity_class_deferred<BikeEntity>(bike);
		 bike->get_owner()->set_ws_position(get_ws_position());
	 }
	 get_owner()->set_ws_position(glm::vec3(0, 0, 0.5));
	 get_owner()->parent_to(bike->get_owner());
}
 void Player::stop() {
 }

 Player::~Player() {
 }

 Player::Player() {
	 return;
	 //player_mesh = construct_sub_component<MeshComponent>("CharMesh");
	 //player_capsule = construct_sub_component<CapsuleComponent>("CharCapsule");
	 //spotlight = construct_sub_component<SpotLightComponent>("Flashlight");
	 //health = construct_sub_component<HealthComponent>("PlayerHealth");

	// auto playerMod = g_assets.find_assetptr_unsafe<Model>("SWAT_model.cmdl");
	 //player_mesh->set_model(playerMod);
	// player_mesh->set_animation_graph("ik_test.ag");
	// player_mesh->set_is_visible(false);
	//
	// player_capsule->set_is_trigger(true);
	// player_capsule->set_send_overlap(true);
	// player_capsule->set_is_enable(false);
	// player_capsule->set_is_static(false);
	// player_capsule->height = 1.7;
	// player_capsule->radius = 0.3;

	 set_ticking(true);
 }

 vector<Component*> GameplayStatic::find_components(const ClassTypeInfo* info) {
	 assert(info && info->is_a(Component::StaticType));
	 double now = GetTime();
	 auto& all = eng->get_level()->get_all_objects();
	 vector<Component*> out;
	 for (auto e : all)
		 if (e->get_type().is_a(*info))
			 out.push_back((Component*)e);
	 double end = GetTime();
	 printf("find_components_of_class: took %f\n", float(end - now));
	 return out;
 }
 Entity* GameplayStatic::find_by_name(string name) {
	 return eng->get_level()->find_initial_entity_by_name(name);
 }
#include "UI/UILoader.h"
static int GameplayStatic_debug_text_start = 10;
 void GameplayStatic::reset_debug_text_height()
 {
	 GameplayStatic_debug_text_start = 10;
 }

 void GameplayStatic::debug_text(string text)
 {
	 auto font = g_assets.find_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	 auto draw_text = [&](const char* s) {
		 string str = s;
		 TextShape shape;
		 Rect2d size = GuiHelpers::calc_text_size(std::string_view(str), font);
		 glm::ivec2 ofs = GuiHelpers::calc_layout({ -100,-10 }, guiAnchor::Center, UiSystem::inst->get_vp_rect());

		 shape.rect.x = ofs.x;
		 shape.rect.y = ofs.y + size.h + GameplayStatic_debug_text_start;
		 shape.font = font;
		 shape.color = COLOR_WHITE;
		 shape.with_drop_shadow = true;
		 shape.drop_shadow_ofs = 1;
		 shape.text = str;
		 UiSystem::inst->window.draw(shape);
		 GameplayStatic_debug_text_start += size.h;
	 };
	 draw_text(text.c_str());
 }

 Entity* GameplayStatic::spawn_prefab(PrefabAsset* prefab)
 {
	 return eng->get_level()->spawn_prefab(prefab);
 }
 int GameplayStatic::get_collision_mask_for_physics_layer(PL physics_layer) {
	 return (int)::get_collision_mask_for_physics_layer(physics_layer);
 }
 Entity* GameplayStatic::spawn_entity()
 {
	 return eng->get_level()->spawn_entity();
 }
 HitResult GameplayStatic::cast_ray(glm::vec3 start, glm::vec3 end, int channel_mask, PhysicsBody* ignore_this) {
	 HitResult out;
	 world_query_result res;
	 TraceIgnoreVec ignore;
	 if (ignore_this)
		 ignore.push_back(ignore_this);

	 g_physics.trace_ray(res, start, end, &ignore, channel_mask);
	 out.hit = res.component != nullptr;
	 if (res.component) {
		 out.pos = res.hit_pos;
		 out.what = res.component->get_owner();
	 }
	 return out;
 }
 std::vector<obj<Entity>> GameplayStatic::sphere_overlap(glm::vec3 center, float radius, int channel_mask)
 {
	 std::vector<obj<Entity>> outVec;
	 overlap_query_result res;
	 g_physics.sphere_is_overlapped(res, radius, center, channel_mask);
	 for (int i = 0; i < res.overlaps.size(); i++) {
		 outVec.push_back(res.overlaps[i]->get_owner());
	 }
	 return outVec;
 }
 void GameplayStatic::debug_sphere(glm::vec3 center, float r, float life, const lColor& color)
 {
	 Debug::add_sphere(center, r, color.to_color32(), life);
 }