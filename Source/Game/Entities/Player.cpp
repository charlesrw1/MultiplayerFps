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


#include "MiscEditors/DataClass.h"
#include "Framework/ClassTypePtr.h"
#include "Render/Texture.h"

#include "Input/InputSystem.h"
#include "Input/InputAction.h"

#include "UI/GUISystemPublic.h"
#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Visuals.h"
#include "UI/Widgets/Interactables.h"

#include <SDL2/SDL_events.h>

#include "Game/GameModes/MainMenuMode.h"

#include "CharacterController.h"

#include "BikeEntity.h"


CLASS_IMPL(Player);



#include "Game/Components/BillboardComponent.h"
#include "Game/Components/ArrowComponent.h"


CLASS_H(PlayerSpawnPoint, Entity)
public:
	PlayerSpawnPoint() {

		if (eng->is_editor_level())
		{
			auto b = construct_sub_component<BillboardComponent>("Billboard");
			b->set_texture(default_asset_load<Texture>("icon/_nearest/player_start.png"));
			b->dont_serialize_or_edit = true;	// editor only item, dont serialize
			
			auto a_obj = construct_sub_entity<Entity>("arrow-obj");
			a_obj->construct_sub_component<ArrowComponent>("arrow-comp");
			a_obj->set_ls_transform({}, {}, glm::vec3(0.3));
			a_obj->dont_serialize_or_edit = true;
		}
	}

	static const PropertyInfoList* get_props() {
		START_PROPS(PlayerSpawnPoint)
			REG_INT(team, PROP_DEFAULT, "0"),
		END_PROPS(PlayerSpawnPoint)
	};
	int team = 0;
};

CLASS_IMPL(PlayerSpawnPoint);

CLASS_H(ReferenceComponent, EntityComponent)
public:
	static const PropertyInfoList* get_props() {
		START_PROPS(ReferenceComponent)
			REG_ENTITY_PTR(my_reference, PROP_DEFAULT),
		END_PROPS(ReferenceComponent)
	};

	EntityPtr<Entity> my_reference;
};
CLASS_IMPL(ReferenceComponent);


//
//	PLAYER MOVEMENT CODE
//

static float fall_speed_threshold = -0.05f;
static float grnd_speed_threshold = 0.6f;

static float ground_friction = 8.2;
static float air_friction = 0.01;
static float ground_accel = 6;
static float ground_accel_crouch = 4;
static float air_accel = 3;
static float jumpimpulse = 5.f;
static float max_ground_speed = 5.7;
static float max_sprint_speed = 8.0;
static float sprint_accel = 8;
static float max_air_speed = 2;

void move_variables_menu()
{
	ImGui::SliderFloat("ground_friction", &ground_friction, 0, 20);
	ImGui::SliderFloat("air_friction", &air_friction, 0, 10);
	ImGui::SliderFloat("ground_accel", &ground_accel, 1, 10);
	ImGui::SliderFloat("air_accel", &air_accel, 0, 10);
	ImGui::SliderFloat("max_ground_speed", &max_ground_speed, 2, 20);
	ImGui::SliderFloat("max_air_speed", &max_air_speed, 0, 10);
	ImGui::SliderFloat("jumpimpulse", &jumpimpulse, 0, 20);
}

using glm::vec3;
using glm::vec2;
using glm::dot;
using glm::cross;




float lensquared_noy(vec3 v)
{
	return v.x * v.x + v.z * v.z;
}



static AddToDebugMenu addmovevars("move vars", move_variables_menu);
#include "Sound/SoundPublic.h"


CLASS_H(PlayerWeaponData,ClassBase )
public:
	
	static const PropertyInfoList* get_props() {
		START_PROPS(PlayerWeaponData)
			REG_CLASSTYPE_PTR(entityToSpawn, PROP_DEFAULT),
			REG_ASSET_PTR(model, PROP_DEFAULT),
			REG_ASSET_PTR(sound, PROP_DEFAULT),
			REG_FLOAT(damage,PROP_DEFAULT,"1.0"),
			REG_FLOAT(fire_rate,PROP_DEFAULT,"200.0"),
			REG_INT(mag_size,PROP_DEFAULT,"30"),
			REG_INT(max_reserve_ammo,PROP_DEFAULT,"90"),
			REG_ASSET_PTR(ui_texture, PROP_DEFAULT)
		END_PROPS(PlayerWeaponData)
	}

	ClassTypePtr<Entity> entityToSpawn;

	AssetPtr<Texture> ui_texture;
	AssetPtr<Model> model;
	AssetPtr<SoundFile> sound;
	float damage = 1.f;
	float fire_rate = 200.f;
	int mag_size = 30;
	int max_reserve_ammo = 300;
};
CLASS_IMPL(PlayerWeaponData);

CLASS_H(AllPlayerItems,ClassBase)
public:

	static const PropertyInfoList* get_props() {
		MAKE_VECTORCALLBACK_ATOM(AssetPtr<DataClass>, items);
		START_PROPS(AllPlayerItems)
			REG_STDVECTOR(items,PROP_DEFAULT)
		END_PROPS(AllPlayerItems)
	}

	std::vector<AssetPtr<DataClass>> items;
};
CLASS_IMPL(AllPlayerItems);


#if 0
ViewmodelComponent::~ViewmodelComponent()
{
	idraw->remove_obj(viewmodel_handle);

	printf("deleted viewmodel\n");
}

ViewmodelComponent::ViewmodelComponent(Player* p) 
{
	this->player = p;

	model = mods.find_or_load("arms.glb");
	//animator.set_model(model);

	viewmodel_handle = idraw->register_obj();

	printf("created viewmodel %d\n", viewmodel_handle);

	lastoffset = vec3(0.f);
	lastrot = glm::quat(1.f, 0, 0, 0);
}

float move_a = 8.f;
float move_b = .1f;
float move_c = 8.f;
float move_d = .1f;
float move_e = 1.f;
float move_f = 0.05f;
float move_g = 0.005f;

void vm_menu()
{
	ImGui::DragFloat("a", &move_a, 0.01);
	ImGui::DragFloat("b", &move_b, 0.01);
	ImGui::DragFloat("c", &move_c, 0.01);
	ImGui::DragFloat("d", &move_d, 0.01);
	ImGui::DragFloat("e", &move_e, 0.01);
	ImGui::DragFloat("f", &move_f, 0.01);
	ImGui::DragFloat("g", &move_g, 0.01);
}

#include "glm/gtx/euler_angles.hpp"

void ViewmodelComponent::update_visuals()
{
	Render_Object proxy;

	proxy.viewmodel_layer = true;
	proxy.visible = true;
	//proxy.animator = &animator;
	proxy.model = model;
	if (!g_thirdperson.get_bool())
	{
		glm::mat4 model2 = glm::translate(glm::mat4(1), vec3(0.18, -0.18, -0.25) + viewmodel_offsets + viewmodel_recoil_ofs);
		model2 = glm::scale(model2, glm::vec3(vm_scale.x));


		model2 = glm::translate(model2, vm_offset);
		model2 = model2 * glm::eulerAngleY(PI + PI / 128.f);

		proxy.transform = model2;

	}
	else {
		proxy.visible = false;
	}
	idraw->update_obj(viewmodel_handle, proxy);
}
#endif
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
	InlineVec<PlayerSpawnPoint*, 16> points;
	if (!eng->get_level()->find_all_entities_of_class(points))
		sys_print(Error, "no spawn points");
	else {
		auto pos = points[0]->get_ws_position();

		set_ws_position(pos);
	}
}

glm::vec3 Player::calc_eye_position()
{
	float view_height = (is_crouching) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
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
	

	//animator.tick_tree_new(eng->tick_interval);

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

		const SoundFile* s = GetAssets().find_global_sync<SoundFile>("switch2.wav").get();
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

CLASS_H(HealthComponent, EntityComponent)
public:

	MulticastDelegate<Entity* /* inflictor*/, int/* damage */> on_take_damage;
	MulticastDelegate<> on_death;

	int healthCounter = 0;
	int maxHealth = 100;

	void reset() {}

	void set_to_full() {}

	void take_damage() {
		on_take_damage.invoke(nullptr, 0);
	}

	static const PropertyInfoList* get_props() {
		START_PROPS(HealthComponent)
			REG_INT(maxHealth,PROP_DEFAULT,"100")
		END_PROPS(HealthComponent)
	}
};
CLASS_IMPL(HealthComponent);


void Player::on_jump_callback()
{
	static int i = 0;
	if(is_on_ground())
		velocity.y += 5.0;
	else if(wall_jump_cooldown<=0.0){
		glm::vec2 stick = glm::vec2(cmd.forward_move, cmd.lateral_move);
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
	auto moveAction = inputPtr->get("game/move");
	auto lookAction = inputPtr->get("game/look");
	auto accelAction = inputPtr->get("game/accelerate");
	bike->forward_strength = accelAction->get_value<float>();
	bike->turn_strength = moveAction->get_value<glm::vec2>().x;
	{
		auto off = lookAction->get_value<glm::vec2>();
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


	auto moveAction = inputPtr->get("game/move");
	auto lookAction = inputPtr->get("game/look");
	auto jumpAction = inputPtr->get("game/jump");
	is_crouching = inputPtr->get("game/crouch")->get_value<bool>();
	{
		auto off = lookAction->get_value<glm::vec2>();
		view_angles.x -= off.y;	// pitch
		view_angles.y += off.x;	// yaw
		view_angles.x = glm::clamp(view_angles.x, -HALFPI + 0.01f, HALFPI - 0.01f);
		view_angles.y = fmod(view_angles.y, TWOPI);
	}
	{
		auto move = moveAction->get_value<glm::vec2>();
		float length = glm::length(move);
		if (length > 1.0)
			move /= length;

		cmd.forward_move = move.y;
		cmd.lateral_move = move.x;
		//printf("%f %f %f\n", move.x, move.y,length);

	}
	if (jumpAction->get_value<bool>())
		on_jump_callback();

	const bool is_sprinting = inputPtr->get("game/sprint")->get_value<bool>();

	float friction_value = (is_on_ground()) ? ground_friction : air_friction;
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

	vec2 inputvec = vec2(cmd.forward_move, cmd.lateral_move);
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
	float acceleation_val = (player_on_ground) ? 
		((is_sprinting) ? sprint_accel : ground_accel) :
		air_accel;
	acceleation_val = (is_crouching) ? ground_accel_crouch : acceleation_val;


	float maxspeed_val = (player_on_ground) ? 
		((is_sprinting) ? max_sprint_speed : max_ground_speed) :
		max_air_speed;

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

	uint32_t flags = 0;

	glm::vec3 out_vel;
	ccontroller->move(velocity*(float)eng->get_dt(), eng->get_dt(), 0.001f, flags, out_vel);
	
	velocity = out_vel;
	action = (flags & CCCF_BELOW) ? Action_State::Idle : Action_State::Falling;

	set_ws_position(ccontroller->get_character_pos());

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


	 eng->set_game_focused(true);

	 inputPtr = GetGInput().register_input_user(0);
//
//	 {
//		 std::vector<const InputDevice*> devices;
//		 GetGInput().get_connected_devices(devices);
//		 int deviceIdx = 0;
//		 for (; deviceIdx < devices.size(); deviceIdx++) {
//			 if (devices[deviceIdx]->type == InputDeviceType::Controller) {
//				 inputPtr->assign_device(devices[deviceIdx]->selfHandle);
//				 break;
//			 }
//		 }
//		 if (deviceIdx == devices.size())
//			 inputPtr->assign_device(GetGInput().get_keyboard_device_handle());
//
//		 GetGInput().device_connected.add(this, [&](handle<InputDevice> handle)
//			 {
//
//				 inputPtr->assign_device(handle);
//			 });
//
//		 inputPtr->on_lost_device.add(this, [&]()
//			 {
//				inputPtr->assign_device(GetGInput().get_keyboard_device_handle());
//			 });
//	 }
//
	 inputPtr->enable_mapping("game");
	 inputPtr->enable_mapping("ui");

	 auto jumpAction = inputPtr->get("game/jump");

	// inputPtr->get("ui/menu")->bind_start_function([this] {
	//		 if(hud)
	//			 hud->toggle_menu_mode();
	//	 });

	 assert(jumpAction);


	 Player::find_a_spawn_point();

	 ccontroller = std::make_unique<CharacterController>(player_capsule);
	 ccontroller->set_position(get_ws_position());
	 ccontroller->capsule_height = player_capsule->height;
	 ccontroller->capsule_radius = player_capsule->radius;


	 hud = std::make_unique<PlayerHUD>(this);
	 score_update_delegate.invoke(10);

	 {
		 auto scope = eng->get_level()->spawn_entity_class_deferred<BikeEntity>(bike);
		 bike->set_ws_position(get_ws_position());
	 }
	 set_ws_position(glm::vec3(0, 0, 0.5));
	 parent_to_entity(bike);
}
 void Player::end() {
	 GameInputSystem::get().free_input_user(inputPtr);

	 GetGInput().device_connected.remove(this);
 }

 Player::~Player() {
 }

 Player::Player() {
	 player_mesh = construct_sub_component<MeshComponent>("CharMesh");
	 player_capsule = construct_sub_component<CapsuleComponent>("CharCapsule");
	 spotlight = construct_sub_component<SpotLightComponent>("Flashlight");
	 health = construct_sub_component<HealthComponent>("PlayerHealth");

	 auto playerMod = GetAssets().find_assetptr_unsafe<Model>("SWAT_model.cmdl");
	 player_mesh->set_model(playerMod);
	 player_mesh->set_animation_graph("ik_test.ag");
	 player_mesh->visible = false;

	 player_capsule->set_is_trigger(true);
	 player_capsule->set_send_overlap(true);
	 player_capsule->set_is_enable(false);
	 player_capsule->set_is_static(false);
	 player_capsule->height = 1.7;
	 player_capsule->radius = 0.3;

	 set_ticking(true);
 }

CLASS_H(CoverPositionMarker,Entity)
public:
	CoverPositionMarker() {
		if (eng->is_editor_level()) {
			auto billboard = construct_sub_component<BillboardComponent>("EditorBillboard");
			billboard->set_texture(GetAssets().find_global_sync<Texture>("icon/_nearest/blue_poi.png").get());
			billboard->dont_serialize_or_edit = true;

			auto arrowobj = construct_sub_entity<Entity>("ArrowObj");
			arrowobj->construct_sub_component<ArrowComponent>("Arrow");
			arrowobj->dont_serialize_or_edit = true;
			arrowobj->set_ws_transform({}, {}, glm::vec3(0.2));
		}
	}
};
CLASS_IMPL(CoverPositionMarker);