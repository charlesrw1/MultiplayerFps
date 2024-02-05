#include "Player.h"
#include "Util.h"
#include "Physics.h"
#include "MeshBuilder.h"
#include "Animation.h"
#include "GameData.h"
#include "Game_Engine.h"

#include "imgui.h"

//
//	PLAYER MOVEMENT CODE
//

static const Capsule standing_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_STANDING_HB_HEIGHT,0) };
static const Capsule crouch_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_CROUCING_HB_HEIGHT,0) };

static float fall_speed_threshold = -0.05f;
static float grnd_speed_threshold = 0.025f;


static float move_speed_player = 0.1f;
static bool gravity = true;
static bool col_response = true;
static int col_iters = 2;
static bool col_closest = true;
static bool new_physics = true;

static float ground_friction = 7.2;
static float air_friction = 0.01;
static float ground_accel = 6;
static float ground_accel_crouch = 4;
static float air_accel = 3;
static float minspeed = 1;
static float maxspeed = 30;
static float jumpimpulse = 5.f;
static float max_ground_speed = 5.7;
static float max_air_speed = 2;

void move_variables_menu()
{
	if (!ImGui::Begin("Move Vars")) {
		ImGui::End();
		return;
	}

	ImGui::SliderFloat("ground_friction", &ground_friction, 0, 20);
	ImGui::SliderFloat("air_friction", &air_friction, 0, 10);
	ImGui::SliderFloat("ground_accel", &ground_accel, 1, 10);
	ImGui::SliderFloat("air_accel", &air_accel, 0, 10);
	ImGui::SliderFloat("minspeed", &minspeed, 0, 10);
	ImGui::SliderFloat("maxspeed", &maxspeed, 5, 35);
	ImGui::SliderFloat("max_ground_speed", &max_ground_speed, 2, 20);
	ImGui::SliderFloat("max_air_speed", &max_air_speed, 0, 10);
	ImGui::SliderFloat("jumpimpulse", &jumpimpulse, 0, 20);


	ImGui::End();
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

// hacky way to move up stairs
void check_perch(Entity& player)
{
	static Config_Var* min_stair = cfg.get_var("phys/min_stair", "0.25");
	static Config_Var* max_stair = cfg.get_var("phys/max_stair", "0.5");
	static Config_Var* stair_rad = cfg.get_var("phys/stair_radius", "0.01");

	glm::vec2 dirs[8] = { vec2(1,1),vec2(1,-1),vec2(-1,-1),vec2(-1,1),
		vec2(SQRT2,SQRT2),vec2(SQRT2,-SQRT2), vec2(-SQRT2,-SQRT2), vec2(-SQRT2,SQRT2) };
	for (int i = 0; i < 8; i++) {
		float height = CHAR_HITBOX_RADIUS;
		Ray ray;
		ray.pos = player.position + vec3(dirs[i].x, 0, dirs[i].y) * (CHAR_HITBOX_RADIUS+ stair_rad->real) + vec3(0, height, 0);
		ray.dir = vec3(0, -1, 0);

		RayHit rh;
		rh = engine.phys.trace_ray(ray, player.index, PF_WORLD);

		// perched on ledge
		if (rh.hit_world) {
			if (((height - rh.dist) >= min_stair->real) && (height - rh.dist) <= max_stair->real && rh.normal.y > 0.9) {
				player.position.y = player.position.y + (height - rh.dist);
				player.state |= PMS_GROUND;
				player.state &= ~PMS_JUMPING;
				player.velocity.y = 0;
				sys_print("perched\n");
				return;
			}
			else if (abs(height - rh.dist) < 0.01) {
				// stay in perched state
				player.state |= PMS_GROUND;
			}
		}
	}
}



void check_ground_state(Entity& player, Move_Command command)
{

	if (player.velocity.y > 2.f) {
		player.state &= ~PMS_GROUND;
		return;
	}

	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);
	result = engine.phys.trace_shape(Trace_Shape(where, CHAR_HITBOX_RADIUS), player.index, PF_ALL);

	if (!result.found || result.surf_normal.y < 0.3)
		player.state &= ~PMS_GROUND;
	else {
		if (command.first_sim && !(player.state & PMS_GROUND) && player.velocity.y < -2.f) {
			player.anim.set_leg_anim("act_land", false);
 			player.anim.legs.loop = false;
		}
		player.state |= PMS_GROUND;	
	}

	if(!(player.state & PMS_JUMPING))
		check_perch(player);

	if (player.state & PMS_GROUND) {
		player.state &= ~PMS_JUMPING;
		player.in_air_time = 0.f;
	}
	else if (command.first_sim)
		player.in_air_time += engine.tick_interval;
}

void check_jump(Entity& player, Move_Command command)
{
	if ((player.state & PMS_GROUND) && command.button_mask & BUTTON_JUMP) {
		printf("jump\n");
		player.velocity.y = jumpimpulse;
		player.state |= PMS_JUMPING;
		player.state &= ~PMS_GROUND;

		if (command.first_sim) {
			player.anim.set_leg_anim("act_jump_start", true);
			player.anim.legs.loop = false;
		}
	}
}

GeomContact player_physics_trace_character(int index, bool crouching, vec3 end);
void check_duck(Entity& player, Move_Command cmd)
{
	const float diff = CHAR_STANDING_HB_HEIGHT - CHAR_CROUCING_HB_HEIGHT;

	if (cmd.button_mask & BUTTON_DUCK) {
		if (!(player.state & PMS_GROUND) && !(player.state & PMS_CROUCHING)) {
			// Move legs of player up
			player.position.y += diff;		}

		player.state |= PMS_CROUCHING;
	}
	else {
		if (player.state & PMS_CROUCHING) {
			// uncrouch

			if (player.state & PMS_GROUND) {
				auto gc = player_physics_trace_character(player.index, false, player.position+vec3(0,0.001,0));

				if (!gc.found)
					player.state &= ~PMS_CROUCHING;
			}
			else {
				vec3 end = player.position - vec3(0, diff, 0) + vec3(0,0.001,0);
				auto gc = player_physics_trace_character(player.index, false, end);

				if (!gc.found) {
					player.state &= ~PMS_CROUCHING;
					player.position.y -= diff;
				}
			}
		}
	}
}

GeomContact player_physics_trace_character(int index, bool crouching, vec3 end)
{
	float height = (crouching) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT;
	return engine.phys.trace_shape(Trace_Shape(end, CHAR_HITBOX_RADIUS, height), index, PF_ALL);
}

void player_physics_move(Entity& player)
{
	vec3 orig_velocity = player.velocity;
	vec3 orig_position = player.position;

	int num_bumps = 4;

	float move_time = engine.tick_interval;

	for (int i = 0; i < num_bumps; i++)
	{
		vec3 end = player.position + player.velocity * (move_time/num_bumps);

		GeomContact trace = player_physics_trace_character(player.index, player.state & PMS_CROUCHING, end);

		if (trace.found) {
			vec3 penetration_velocity = dot(player.velocity, trace.penetration_normal) * trace.penetration_normal;
			vec3 slide_velocity = player.velocity - penetration_velocity;
			
			vec3 neg_vunit = -glm::normalize(player.velocity);
			float d = dot(trace.penetration_normal, neg_vunit);
			
			//if (abs(d) > 0.001f) {
			//	float push_out_depth = trace.penetration_depth / d;
			//	player.position = end + push_out_depth * neg_vunit;
			//}
			//else {
				player.position = end + trace.penetration_normal * trace.penetration_depth;
			//}
			player.velocity = slide_velocity;
			if (dot(player.velocity, orig_velocity) < 0) {
				sys_print("opposite velocity\n");
				player.velocity = vec3(0.f);
				break;
			}
		}
		else {
			player.position = end;
		}
	}
}

float lensquared_noy(vec3 v)
{
	return v.x * v.x + v.z * v.z;
}

void player_physics_ground_move(Entity& player, Move_Command command)
{
	check_jump(player, command);
	check_duck(player, command);


	vec3 prevel = player.velocity;

	vec2 inputvec = vec2(command.forward_move, command.lateral_move);
	float inputlen = length(inputvec);
	if (inputlen > 0.00001)
		inputvec = inputvec / inputlen;
	if (inputlen > 1)
		inputlen = 1;

	vec3 look_front = AnglesToVector(command.view_angles.x, command.view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	vec3 look_side = -cross(look_front, vec3(0, 1, 0));

	float acceleation_val = (player.state & PMS_GROUND) ? ground_accel : air_accel;
	acceleation_val = (player.state & PMS_CROUCHING) ? ground_accel_crouch : acceleation_val;
	float maxspeed_val = (player.state & PMS_GROUND) ? max_ground_speed : max_air_speed;

	vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(player.velocity.x, 0, player.velocity.z);

	float wishspeed = inputlen * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed * engine.tick_interval;
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	player.velocity = vec3(xz_velocity.x, player.velocity.y, xz_velocity.z);

	static Config_Var* phys_gravity = cfg.get_var("phys/gravity", "16.0");


	if (!(player.state & PMS_GROUND)) {
		player.velocity.y -= phys_gravity->real * engine.tick_interval;
		sys_print("gravity applied");
	}
	
	static Config_Var* phys_stair = cfg.get_var("phys/stair", "0.5");

	player_physics_move(player);

	if (!(player.flags & EF_DEAD)) {
		player.rotation = vec3(0.f);
		player.rotation .y = HALFPI - command.view_angles.y;
	}
}


void player_physics_check_nans(Entity& player)
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

void player_physics_update(Entity* p, Move_Command command)
{
	if (p->flags & EF_DEAD) {
		command.forward_move = 0;
		command.lateral_move = 0;
		command.up_move = 0;
	}
	p->view_angles = command.view_angles;

	vec3 pre_update_velocity = p->velocity;
	
	// Friction
	float friction_value = (p->state & PMS_GROUND) ? ground_friction : air_friction;
	float speed = length(p->velocity);
	if (speed >= 0.0001) {
		float dropamt = friction_value * speed * engine.tick_interval;
		float newspd = speed - dropamt;
		if (newspd < 0)
			newspd = 0;
		float factor = newspd / speed;
		p->velocity.x *= factor;
		p->velocity.z *= factor;
	}

	check_ground_state(*p, command);
	player_physics_ground_move(*p, command);
	player_physics_check_nans(*p);

}


// if in the middle of an animation, dont interrupt it
void player_animation_update(Entity* ent)
{
	// animation is being controlled server side
	if (ent->flags & EF_FORCED_ANIMATION)
		return;

	// upper body
	if (ent->anim.m.loop || ent->anim.m.finished)
	{	
		// for now, only one
		ent->anim.set_anim("act_idle", false);
		ent->anim.m.loop = true;
	}

	// lower body
	glm::vec3 ent_face_dir = AnglesToVector(ent->view_angles.x, ent->view_angles.y);
	float groundspeed = glm::length(glm::vec2(ent->velocity.x, ent->velocity.z));
	bool falling = ent->velocity.y < fall_speed_threshold;
	
	int leg_anim = 0;
	float leg_speed = 1.f;
	bool loop_legs = true;
	bool set_legs = true;

	const char* next_leg_anim = "null";
	if (ent->anim.legs.anim == -1)
		ent->anim.legs.finished = true;

	if (!ent->anim.legs.loop && !ent->anim.legs.finished)
		set_legs = false;

	if (ent->state & PMS_JUMPING || (!(ent->state & PMS_GROUND) && ent->in_air_time > 0.3f)) {
		next_leg_anim = "act_falling";
	}
	else if (groundspeed > grnd_speed_threshold) {
		if (ent->state & PMS_CROUCHING)
			next_leg_anim = "act_crouch_walk";
		else {
			vec3 facing_dir = vec3(cos(ent->view_angles.y), 0, sin(ent->view_angles.y));	// which direction are we facing towards
			vec3 grnd_velocity = glm::normalize(vec3(ent->velocity.x, 0, ent->velocity.z));
			float d = dot(facing_dir, grnd_velocity);
			bool left = cross(facing_dir, grnd_velocity).y < 0;
			if (abs(d) >= 0.5) { // 60 degrees from look
				next_leg_anim = "act_run";
			}
			else {
				next_leg_anim = "act_strafe_left_better";
			}

			leg_speed = ((groundspeed - grnd_speed_threshold) / 6.f) + 1.f;

			if (dot(ent_face_dir, ent->velocity) < -0.25) {
				leg_speed = -leg_speed;
			}

		}
	}
	else {
		if (ent->state & PMS_CROUCHING)
			next_leg_anim = "act_crouch_idle";
		else
			next_leg_anim = "act_idle";
	}


	// pick out upper body animations here
	// shooting, reloading, etc.
	if (set_legs) {

		static Config_Var* anim_blend_running = cfg.get_var("abr", "0.15");

		ent->anim.set_leg_anim(next_leg_anim, false, anim_blend_running->real);
		ent->anim.legs.loop = loop_legs;
		ent->anim.legs.speed = leg_speed;
	}
}

void player_fire_weapon()
{
	// play sounds
	// play animation (viewmodel for local player)
	// on server: do raycast and do damage
	// effects (muzzle flash, gun tracer, ...)
	// recoil for local player

}

static float fire_time = 0.1f;
static float reload_time = 1.9f;

void player_item_update(Entity* p, Move_Command command, bool is_local)
{
	vec3 look_vec = AnglesToVector(command.view_angles.x, command.view_angles.y);

	bool wants_shoot = command.button_mask & BUTTON_FIRE1;
	bool wants_reload = command.button_mask & BUTTON_RELOAD;

	Item_State& w = p->items;
	if (!(w.active_item >= 0 && w.active_item < Item_State::MAX_ITEMS)) {
		printf("invalid active_item\n");
		return;
	}

	if (is_local) {
		if (engine.local.viewmodel == nullptr) {
			engine.local.viewmodel = FindOrLoadModel("m16_fp.glb");
			engine.local.viewmodel_animator.set_model(engine.local.viewmodel);
		}
	}

	if (w.timer > 0)
		w.timer -= engine.tick_interval;
	float blend = cfg.get_var("abr", "0.15")->real;
	switch (w.state)
	{
	case ITEM_IDLE:
		if (is_local) {
			engine.local.viewmodel_animator.set_anim("act_idle", false);
		}
		if (wants_shoot) {
			w.timer = fire_time;
			w.state = ITEM_IN_FIRE;

			p->anim.set_anim("act_shoot", true, blend);
			p->anim.m.loop = false;

			engine.fire_bullet(p, look_vec, p->position + vec3(0, STANDING_EYE_OFFSET, 0));

			if (is_local) {
				engine.local.viewmodel_animator.set_anim("act_shoot", true);
				engine.local.viewmodel_animator.m.loop = false;
			}
		}
		if (wants_reload) {
			w.timer = reload_time;
			w.state = ITEM_RELOAD;

			p->anim.set_anim("act_reload", true, blend);
			p->anim.m.loop = false;
			p->anim.m.speed = 0.5f;

			if (is_local) {
				engine.local.viewmodel_animator.set_anim("act_reload", true);
				engine.local.viewmodel_animator.m.loop = false;
				engine.local.viewmodel_animator.m.speed = 2.5f;

			}
		}

		break;
	case ITEM_IN_FIRE:
		if (w.timer <= 0) {
			w.state = ITEM_IDLE;
		}
		break;
	case ITEM_RELOAD:
		if (w.timer <= 0) {
			w.state = ITEM_IDLE;
		}
		break;
	}
}

// item code and animation updates
void player_post_physics(Entity* p, Move_Command command, bool is_local)
{
	player_item_update(p, command, is_local);
	player_animation_update(p);
}