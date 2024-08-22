#include "Player.h"
#include "Framework/Util.h"
#include "Framework/MeshBuilder.h"

#include "GameEnginePublic.h"
#include "imgui.h"

#include "Render/DrawPublic.h"

#include "Physics/Physics2.h"

#include "Debug.h"

#include "Framework/Dict.h"

#include "Framework/Config.h"

#include "CameraPoint.h"

#include "Assets/AssetDatabase.h"

#include "Level.h"

CLASS_H(PlayerNull, PlayerBase)
public:

	void start() override {
		camera = eng->get_level()->find_first_of<CameraPoint>();
	}

	void get_view(
		glm::mat4& view,
		float& fov
	) override {
		if (!camera)
			return;

		fov = 70.f;
		view = glm::inverse(camera->get_ws_transform());

		//front = AnglesToVector(euler.x, euler.y);
	}
	void set_input_command(Move_Command cmd) override {}

	// for main menu
	CameraPoint* camera = nullptr;
};
CLASS_IMPL(PlayerNull);

CLASS_IMPL(PlayerBase);
CLASS_IMPL(Player);
CLASS_IMPL(PlayerSpawnPoint);

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

enum Player_Item_State
{
	ITEM_STATE_IDLE,
	ITEM_STATE_SHOOT,
	ITEM_STATE_RELOAD,
	ITEM_STATE_HOLSTER,
	ITEM_STATE_EQUIP,
};

// state machine: (there isnt really one, its just one)
/*
	idle
	running
	mid-air

	client_prediction
	physics sim all inputs from last authoritative state
	only output effects for last state (this can be a global state to simplify code)
	run shared item code which updates view model and local effects
	update character third person animations

server_side/listen
	physics sim inputs
	output effects for all sims
	run shared item code
	run think routine
	update character animations

physics (operates on player_state)
animations, think, item (operates on entity)


"taunt 2"
sends to server, server sets your animation and flag that says you are in taunt
server sends your animation state back

local animation is not predicted, your character sets its own animation locally
server sends animation in entity_state which is ignored unless flag is set

Your character dies on server, server functions set your animation to death one with server animation override flag


*/

int character_state = 0;
int item_state = 0;
bool character_state_changed = false;
bool item_state_changed = false;
bool finished = false;
bool force_animation = false;

static ConfigVar min_stair("phys.min_stair", "0.25",CVAR_FLOAT);
static ConfigVar max_stair("phys.max_stair", "0.5",CVAR_FLOAT);
static ConfigVar stair_rad("phys.stair_radius", "0.01",CVAR_FLOAT);
static ConfigVar phys_gravity("phys.gravity", "16.0",CVAR_FLOAT,-20,20);
static ConfigVar debug_fly("dbg.fly", "0", CVAR_BOOL);

#if 0
// hacky way to move up stairs
bool Player::check_perch()
{
	glm::vec2 dirs[8] = { vec2(1,1),vec2(1,-1),vec2(-1,-1),vec2(-1,1),
		vec2(SQRT2,SQRT2),vec2(SQRT2,-SQRT2), vec2(-SQRT2,-SQRT2), vec2(-SQRT2,SQRT2) };
	for (int i = 0; i < 8; i++) {
		float height = CHAR_HITBOX_RADIUS;
		Ray ray;
		ray.pos = position + vec3(dirs[i].x, 0, dirs[i].y) * (CHAR_HITBOX_RADIUS+ stair_rad.get_float()) + vec3(0, height, 0);
		ray.dir = vec3(0, -1, 0);

		RayHit rh;
		//rh = eng->phys.trace_ray(ray, selfid, PF_WORLD);

		// perched on ledge
		if (rh.hit_world) {
			if (((height - rh.dist) >= min_stair.get_float()) && (height - rh.dist) <= max_stair.get_float() && rh.normal.y > 0.98) {
				position.y = position.y + (height - rh.dist);
				velocity.y = 0;
				return true;
			}
			else if (abs(height - rh.dist) < 0.005) {
				// stay in perched state
				return true;
			}
		}
	}
	return false;
}

#endif
Action_State Player::get_ground_state_based_on_speed(float s) const
{
	if (s > 0.1)
		return Action_State::Moving;
	else
		return Action_State::Idle;
}





void sweep_move(glm::vec3& velocity, glm::vec3& position)
{
	world_query_result res;
	float move_time = eng->get_tick_interval();
	float length = glm::length(velocity) * move_time;

	if (length < 0.00001)
		return;

	glm::vec3 dir = glm::normalize(velocity);
	bool found = g_physics->sweep_sphere(res, 1.0, position, dir, length, {});
	if (found) {
		position = position + dir * length * res.fraction+res.hit_normal*0.01f;
		velocity = glm::vec3(0.0);
	}
	else {
		position = position + dir * length;
	}

}

void Player::slide_move()
{
	vec3 orig_velocity = velocity;
	vec3 orig_position = position;

	int num_bumps = 4;

	float move_time = eng->get_tick_interval();

	// lateral
	glm::vec3 lateral_velocity = glm::vec3(velocity.x, 0, velocity.z);
	sweep_move(lateral_velocity, position);

	// down
	glm::vec3 down_velocity = glm::vec3(0, velocity.y, 0);
	sweep_move(down_velocity, position);

	velocity = glm::vec3(lateral_velocity.x, down_velocity.y, lateral_velocity.z);
	return;

	world_query_result res;
	glm::vec3 dir = glm::normalize(velocity);
	float length = glm::length(velocity) * move_time;
	bool found = g_physics->sweep_sphere(res, 1.0, position, dir, length,{});
	if (found) {
		position = position + dir * length * res.fraction;
		velocity = glm::vec3(0.0);
	}
	else {
		position = position + dir * length;
	}
	return;

	for (int i = 0; i < num_bumps; i++)
	{
		if (glm::length(velocity) < 0.0001)
			return;

		vec3 end = position + velocity * (move_time/num_bumps);



		world_query_result res;
		bool found = g_physics->sweep_sphere(res, 5.0, position, glm::normalize(velocity), glm::length(velocity) * (move_time / num_bumps), {});

		if (found) {
			vec3 penetration_velocity = dot(velocity, res.hit_normal) * res.hit_normal;
			vec3 slide_velocity = velocity - penetration_velocity;
			
			vec3 neg_vunit = -glm::normalize(velocity);
			float d = dot(res.hit_normal, neg_vunit);
			
			//if (abs(d) > 0.001f) {
			//	float push_out_depth = trace.penetration_depth / d;
			//	player.position = end + push_out_depth * neg_vunit;
			//}
			//else {
			position = res.hit_pos;
			//}
			velocity = slide_velocity;
			if (dot(velocity, orig_velocity) < 0) {
				sys_print("opposite velocity\n");
			//	velocity = vec3(0.f);
				break;
			}
		}
		else {
			position = end;
		}
	}
}

float lensquared_noy(vec3 v)
{
	return v.x * v.x + v.z * v.z;
}

#if 0
void Player::ground_move()
{
	state_time += eng->get_tick_interval();

	bool dont_add_grav = false;
	Action_State last = action;
	action = Action_State::Moving;// update_state(glm::length(velocity), dont_add_grav);

	if (last != action) {
		state_time = 0.0;
	}

	vec3 prevel = velocity;

	vec2 inputvec = vec2(cmd.forward_move, cmd.lateral_move);
	float inputlen = length(inputvec);
	if (inputlen > 0.00001)
		inputvec = inputvec / inputlen;
	if (inputlen > 1)
		inputlen = 1;

	vec3 look_front = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

	const bool player_on_ground = is_on_ground();
	float acceleation_val = (player_on_ground) ? ground_accel : air_accel;
	acceleation_val = (is_crouching) ? ground_accel_crouch : acceleation_val;
	float maxspeed_val = (player_on_ground) ? max_ground_speed : max_air_speed;

	vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(velocity.x, 0, velocity.z);

	float wishspeed = inputlen * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed * eng->get_tick_interval();
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	velocity = vec3(xz_velocity.x, velocity.y, xz_velocity.z);

	if (!dont_add_grav) {
		velocity.y -= phys_gravity.get_float() * eng->get_tick_interval();
	}
	
	slide_move();
}
#endif

void player_physics_check_nans(Player& player)
{
	if (player.position.x != player.position.x || player.position.y != player.position.y ||
		player.position.z != player.position.z)
	{
		printf("origin nan found in PlayerPhysics\n");
		player.position = vec3(0);
	}
	if (player.velocity.x != player.velocity.x
		|| player.velocity.y != player.velocity.y || player.velocity.z != player.velocity.z)
	{
		printf("velocity nan found in PlayerPhysics\n");
		player.velocity = vec3(0);
	}
}

void draw_entity_info()
{
	static int current_index = 0;
	Entity* e = nullptr;
	if (!e)
		return;
	glm::vec3 vel = e->get_velocity();
	ImGui::InputFloat3("position", &e->position.x);
	ImGui::InputFloat3("velocity", &vel.x);
	ImGui::InputFloat3("acceleration", &e->esimated_accel.x);

	static bool predict_brake = true;
	ImGui::Checkbox("Predict stop pos", &predict_brake);
	Player* p = e->cast_to<Player>();
	if (predict_brake && p) {

		if (glm::abs(p->cmd.forward_move) < 0.000001 && glm::abs(p->cmd.lateral_move) < 0.000001) {
			glm::vec3 pos = e->position;
			glm::vec3 v = vel;
			v.y = 0.0;
			// assume accel = 0
			int iter = 0;
			float dt = 0.025;	// use a big dt
			while (dot(v, v) > 0.0001*0.0001 && iter < 100) {
				float speed = glm::length(v);

				if (speed < 0.3)
					break;

				float dropamt = ground_friction * speed * dt;
				float newspd = speed - dropamt;
				if (newspd < 0)
					newspd = 0;
				float factor = newspd / speed;
				v *= factor;
				pos += v * (float)dt;
				iter++;
			}
			ImGui::Text("%d", iter);
			ImGui::Text("%f %f %f", pos.x, pos.y, pos.z);
			Debug::add_sphere(pos, 0.5, COLOR_BLUE, -1.0, false);
		}
	}

}

static AddToDebugMenu addmovevars("move vars", move_variables_menu);
static AddToDebugMenu adddrawentinfo("entity physics info", draw_entity_info);
#include "Sound/SoundPublic.h"
void Player::move()
{
	glm::vec3 last_velocity = velocity;

	// fixme:
	view_angles = cmd.view_angles;

	// Friction
	float friction_value = (is_on_ground()) ? ground_friction : air_friction;
	float speed = glm::length(velocity);

	if (speed >= 0.0001) {
		float dropamt = friction_value * speed * eng->get_tick_interval();
		float newspd = speed - dropamt;
		if (newspd < 0)
			newspd = 0;
		float factor = newspd / speed;
		velocity.x *= factor;
		velocity.z *= factor;
	}

	{
		action = Action_State::Falling;

		vec2 inputvec = vec2(cmd.forward_move, cmd.lateral_move);
		float inputlen = length(inputvec);
		if (inputlen > 0.00001)
			inputvec = inputvec / inputlen;
		if (inputlen > 1)
			inputlen = 1;

		vec3 look_front = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);
		look_front = normalize(look_front);
		vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

		vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);

		//position += wishdir * 12.0f*(float)eng->tick_interval;
		velocity = wishdir * 12.0f;
		slide_move();
	}
	
	player_physics_check_nans(*this);


	auto pos = position;
	auto height = CHAR_STANDING_HB_HEIGHT;
	auto width = CHAR_HITBOX_RADIUS;
	//Debug::add_sphere(pos + vec3(0, width, 0), width, COLOR_PINK, -1.f);
	//Debug::add_sphere(pos + vec3(0, height- width, 0), width, COLOR_PINK,-1.f);


	esimated_accel = (velocity - last_velocity) / (float)eng->get_tick_interval();

	distTraveledSinceLastFootstep += speed * (float)eng->get_tick_interval();

	if (distTraveledSinceLastFootstep >= 5.0) {
		const SoundFile* s = GetAssets().find_global_sync<SoundFile>("footstep_jack_01.wav").get();
		isound->play_sound(s);
		distTraveledSinceLastFootstep = 0.0;
	}
}


#include "MiscEditors/DataClass.h"
#include "Framework/ClassTypePtr.h"
#include "Render/Texture.h"
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
template<typename T>
T neg_modulo(T x, T mod_)
{
	return glm::mod(glm::mod(x, mod_) + mod_, mod_);
}
static float modulo_lerp_(float start, float end, float mod, float t)
{
	return neg_modulo(t - start,mod) / neg_modulo(end - start, mod);
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



void Player::find_a_spawn_point()
{
	InlineVec<PlayerSpawnPoint*, 16> points;
	if (!eng->get_level()->find_all_entities_of_class(points))
		sys_print("!!! no spawn points");
	else {
		position = points[0]->get_ws_position();
	}
}

void Player::set_input_command(Move_Command newcmd) {
	this->cmd = newcmd;
}


void Player::update()
{
	if (Entity::has_flag(EntityFlags::Dead)) {
		cmd.forward_move = 0;
		cmd.lateral_move = 0;
		cmd.up_move = 0;
		cmd.button_mask = 0;
	}

	move();

	set_ws_transform(position, glm::quat(rotation), scale);
}

glm::vec3 Player::calc_eye_position()
{
	float view_height = (is_crouching) ? CROUCH_EYE_OFFSET : STANDING_EYE_OFFSET;
	return position + vec3(0, view_height, 0);
}

void Player::get_view(glm::mat4& viewMat, float& fov)
{
	if (g_thirdperson.get_bool()) {

		vec3 front = AnglesToVector(view_angles.x, view_angles.y);
		vec3 side = normalize(cross(front, vec3(0, 1, 0)));
		vec3 camera_pos = position + vec3(0, STANDING_EYE_OFFSET, 0) - front * 2.5f + side * 0.8f;

		viewMat = glm::lookAt(camera_pos, camera_pos + front, glm::vec3(0, 1, 0));

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

#include "UI/GUISystemPublic.h"
#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Visuals.h"
#include "UI/Widgets/Interactables.h"

#include <SDL2/SDL_events.h>

#include "Game/GameModes/MainMenuMode.h"

class PlayerHUD : public GUIFullscreen
{
public:
	void on_enter_pause_menu() {
		scoreWidget->hidden = true;

		paused_menu->hidden = false;
		pause_menu_outline->hidden = false;

		eng->set_game_focused(false);

		is_in_pause = true;
	}
	void on_leave_pause_menu() {
		scoreWidget->hidden = false;

		paused_menu->hidden = true;
		pause_menu_outline->hidden = true;

		eng->set_game_focused(true);

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


	void to_main_menu() {
		Cmd_Manager::get()->execute(Cmd_Execute_Mode::APPEND, "map mainMenuMap.txt");
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
		b->on_selected.add(this, &PlayerHUD::to_main_menu);
		vbox->add_this(b);
		b = create_button("CONTINUE", s);
		b->on_selected.add(this, &PlayerHUD::on_leave_pause_menu);
		vbox->add_this(b);

		return vbox;
	}

	bool is_in_pause = false;

	PlayerHUD(Player* p) {
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
	void on_key_down(const SDL_KeyboardEvent& k) override {
		if (k.keysym.scancode == SDL_SCANCODE_ESCAPE) {
			if (is_in_pause)
				on_leave_pause_menu();
			else
				on_enter_pause_menu();
		}
	}

	GUIVerticalBox* paused_menu = nullptr;
	GUIBox* pause_menu_outline = nullptr;

	GUIText* scoreWidget = nullptr;
	GUIBox* boxWidget = nullptr;
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

#include "Input/InputSystem.h"
#include "Input/InputAction.h"

 void Player::start()  {
	 hud = std::make_unique<PlayerHUD>(this);

	 score_update_delegate.invoke(10);

	 eng->set_game_focused(true);

	 inputPtr = GameInputSystem::get().register_input_user(0);
}
 void Player::end() {
	 GameInputSystem::get().free_input_user(inputPtr);
 }

 Player::~Player() {
 }
#include "Physics/ChannelsAndPresets.h"
 Player::Player() {
	 player_mesh = create_sub_component<MeshComponent>("CharMesh");
	 player_capsule = create_sub_component<CapsuleComponent>("CharCapsule");
	 viewmodel_mesh = create_sub_component<MeshComponent>("ViewmodelMesh");
	 spotlight = create_sub_component<SpotLightComponent>("Flashlight");
	 health = create_sub_component<HealthComponent>("PlayerHealth");
	 root_component = player_capsule;

	 player_mesh->attach_to_parent(player_capsule, {});
	 viewmodel_mesh->attach_to_parent(player_mesh, {});
	 spotlight->attach_to_parent(root_component, {});

	 auto playerMod = GetAssets().find_assetptr_unsafe<Model>("SWAT_model.cmdl");
	 player_mesh->set_model(playerMod);

	 player_capsule->physicsPreset.ptr = &PP_Character::StaticType;
	 player_capsule->isTrigger = false;
	 player_capsule->sendOverlap = true;

	 tickEnabled = true;
 }

