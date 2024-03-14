#include "Player.h"
#include "Util.h"
#include "Physics.h"
#include "MeshBuilder.h"
#include "Animation.h"
#include "Game_Engine.h"
#include "imgui.h"

//
//	PLAYER MOVEMENT CODE
//

static const Capsule standing_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_STANDING_HB_HEIGHT,0) };
static const Capsule crouch_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_CROUCING_HB_HEIGHT,0) };

static float fall_speed_threshold = -0.05f;
static float grnd_speed_threshold = 0.6f;


static float move_speed_player = 0.1f;
static bool gravity = true;
static bool col_response = true;
static int col_iters = 2;
static bool col_closest = true;
static bool new_physics = true;

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

static Auto_Config_Var min_stair("phys.min_stair", 0.25f);
static Auto_Config_Var max_stair("phys.max_stair", 0.5f);
static Auto_Config_Var stair_rad("phys.stair_radius", 0.01f);
static Auto_Config_Var phys_gravity("phys.gravity", 16.f);
static Auto_Config_Var debug_fly("dbg.fly", 0, 0,"Let player fly around");

// hacky way to move up stairs
void check_perch(Entity& player, bool& dont_add_grav)
{
	dont_add_grav = false;

	glm::vec2 dirs[8] = { vec2(1,1),vec2(1,-1),vec2(-1,-1),vec2(-1,1),
		vec2(SQRT2,SQRT2),vec2(SQRT2,-SQRT2), vec2(-SQRT2,-SQRT2), vec2(-SQRT2,SQRT2) };
	for (int i = 0; i < 8; i++) {
		float height = CHAR_HITBOX_RADIUS;
		Ray ray;
		ray.pos = player.position + vec3(dirs[i].x, 0, dirs[i].y) * (CHAR_HITBOX_RADIUS+ stair_rad.real()) + vec3(0, height, 0);
		ray.dir = vec3(0, -1, 0);

		RayHit rh;
		rh = eng->phys.trace_ray(ray, player.selfid, PF_WORLD);

		// perched on ledge
		if (rh.hit_world) {
			if (((height - rh.dist) >= min_stair.real()) && (height - rh.dist) <= max_stair.real() && rh.normal.y > 0.98) {
				player.position.y = player.position.y + (height - rh.dist);
				player.state |= PMS_GROUND;
				player.state &= ~PMS_JUMPING;
				player.velocity.y = 0;
				sys_print("perched\n");
				dont_add_grav = true;
				return;
			}
			else if (abs(height - rh.dist) < 0.005) {
				// stay in perched state
				player.state |= PMS_GROUND;
				dont_add_grav = true;
			}
		}
	}
}



void check_ground_state(Entity& player, Move_Command command, bool& dont_add_grav)
{

	if (player.velocity.y > 2.f) {
		player.state &= ~PMS_GROUND;
		return;
	}

	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);
	result = eng->phys.trace_shape(Trace_Shape(where, CHAR_HITBOX_RADIUS), player.selfid, PF_ALL);

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
		check_perch(player, dont_add_grav);

	if (player.state & PMS_GROUND) {
		player.state &= ~PMS_JUMPING;
		player.in_air_time = 0.f;
	}
	else if (command.first_sim)
		player.in_air_time += eng->tick_interval;
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
				auto gc = player_physics_trace_character(player.selfid, false, player.position+vec3(0,0.001,0));

				if (!gc.found) {
					player.state &= ~PMS_CROUCHING;
				}
			}
			else {
				vec3 end = player.position - vec3(0, diff, 0) + vec3(0,0.001,0);
				auto gc = player_physics_trace_character(player.selfid, false, end);

				if (!gc.found) {
					// one more check
					Ray r;
					r.pos = player.position + glm::vec3(0, CHAR_CROUCING_HB_HEIGHT, 0);
					r.dir = vec3(0, -1, 0);
					auto tc = eng->phys.trace_ray(r, player.selfid, PF_ALL);
					if (tc.dist < 0 || tc.dist >= CHAR_STANDING_HB_HEIGHT+0.01) {
						player.state &= ~PMS_CROUCHING;
						player.position.y -= diff;
						sys_print("passed");
					}
				}
			}
		}
	}
}

GeomContact player_physics_trace_character(int index, bool crouching, vec3 end)
{
	float height = (crouching) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT;
	return eng->phys.trace_shape(Trace_Shape(end, CHAR_HITBOX_RADIUS, height), index, PF_ALL);
}

void player_physics_move(Entity& player)
{
	vec3 orig_velocity = player.velocity;
	vec3 orig_position = player.position;

	int num_bumps = 4;

	float move_time = eng->tick_interval;

	for (int i = 0; i < num_bumps; i++)
	{
		vec3 end = player.position + player.velocity * (move_time/num_bumps);

		GeomContact trace = player_physics_trace_character(player.selfid, player.state & PMS_CROUCHING, end);

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

void player_physics_ground_move(Entity& player, Move_Command command, bool dont_add_grav)
{
	if (!(player.flags & EF_DEAD)) {
		check_jump(player, command);
		check_duck(player, command);
	}


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
	vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

	float acceleation_val = (player.state & PMS_GROUND) ? ground_accel : air_accel;
	acceleation_val = (player.state & PMS_CROUCHING) ? ground_accel_crouch : acceleation_val;
	float maxspeed_val = (player.state & PMS_GROUND) ? max_ground_speed : max_air_speed;

	vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(player.velocity.x, 0, player.velocity.z);

	float wishspeed = inputlen * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed * eng->tick_interval;
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	player.velocity = vec3(xz_velocity.x, player.velocity.y, xz_velocity.z);

	if (!dont_add_grav) {
		player.velocity.y -= phys_gravity.real() * eng->tick_interval;
	}
	

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

		command.button_mask = 0;
	}
	p->view_angles = command.view_angles;

	vec3 pre_update_velocity = p->velocity;
	
	// Friction
	float friction_value = (p->state & PMS_GROUND) ? ground_friction : air_friction;
	float speed = length(p->velocity);
	if (speed >= 0.0001) {
		float dropamt = friction_value * speed * eng->tick_interval;
		float newspd = speed - dropamt;
		if (newspd < 0)
			newspd = 0;
		float factor = newspd / speed;
		p->velocity.x *= factor;
		p->velocity.z *= factor;
	}

	bool dont_add_grav = false;
	check_ground_state(*p, command,dont_add_grav);
	if (eng->is_host && debug_fly.integer()) {
		p->state = PMS_JUMPING;

		vec2 inputvec = vec2(command.forward_move, command.lateral_move);
		float inputlen = length(inputvec);
		if (inputlen > 0.00001)
			inputvec = inputvec / inputlen;
		if (inputlen > 1)
			inputlen = 1;

		vec3 look_front = AnglesToVector(command.view_angles.x, command.view_angles.y);
		look_front = normalize(look_front);
		vec3 look_side = -normalize(cross(look_front, vec3(0, 1, 0)));

		vec3 wishdir = (look_front * inputvec.x + look_side * inputvec.y);

		p->position += wishdir * 12.0f*(float)eng->tick_interval;

	}
	else
		player_physics_ground_move(*p, command, dont_add_grav);
	player_physics_check_nans(*p);

}


// if in the middle of an animation, dont interrupt it
void player_animation_update(Entity* ent)
{
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
			bool backwards = dot(ent_face_dir, ent->velocity) < -0.25;
			if (abs(d) >= 0.5) { // 60 degrees from look
				if (backwards) next_leg_anim = "act_run_back";
				else next_leg_anim = "act_run";
			}
			else {
				if (left) next_leg_anim = "act_strafe_left";
				else next_leg_anim = "act_strafe";
			}

			leg_speed = ((groundspeed - grnd_speed_threshold) / 6.f) + 1.f;

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

		ent->anim.set_leg_anim(next_leg_anim, false, 0.15);
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



Game_Item_Stats stats[Game_Inventory::NUM_GAME_ITEMS] = {
	{"unequip","","",ITEM_CAT_MELEE},
	{"m16", "m16.glb","",ITEM_CAT_RIFLE, 0, 13.0, 2.0, 0.1,0.1,10,30,90,0.5},
	{"ak47","ak47.glb","",ITEM_CAT_RIFLE, 0, 10.0, 2.0, 0.1, 0.1, 12, 30, 90, 0.5},
	{"m24", "m24.glb", "", ITEM_CAT_BOLT_ACTION, 0, 0.7, 3.0, 0.1, 0.1, 75, 0, 30, 0.0},
	{"knife", "knife.glb", "", ITEM_CAT_MELEE, 0, 2.0},
	{"bomb", "bomb.glb", "", ITEM_CAT_BOMB},
	{"grenade","grenade_he.glb", "", ITEM_CAT_THROWABLE, 0}
};
Game_Item_Stats* get_item_stats()
{
	return stats;
}

static float fire_time = 0.15f;
static float reload_time = 1.9f;


void change_to_item(Entity& p, int next_item)
{
	ASSERT(next_item >= 0 && next_item < Game_Inventory::NUM_GAME_ITEMS);
	ASSERT(p.inv.active_item >= 0 && p.inv.active_item < Game_Inventory::NUM_GAME_ITEMS);

	if (next_item == p.inv.active_item)
		return;
	if (next_item == p.inv.pending_item)
		return;
	if (p.inv.state == ITEM_RAISING) {
		Game_Item_Stats& next = get_item_stats()[next_item];
		p.inv.active_item = next_item;
		p.inv.state = ITEM_RAISING;
		p.inv.timer = next.draw_time;
		p.inv.pending_item = -1;
		return;
	}

	Game_Item_Stats& cur = get_item_stats()[p.inv.active_item];
	p.inv.pending_item = next_item;
	p.inv.state = ITEM_LOWERING;
	p.inv.timer = cur.holster_time;
}

void player_item_update(Entity* p, Move_Command command, bool is_local)
{
	Game_Inventory& inv = p->inv;
	bool is_simulated_client = !eng->is_host;

	if (is_simulated_client && p->inv.tick_for_staging != -1) {
		int delta = eng->tick - p->inv.tick_for_staging;
		if (delta > 30) { // 30 ticks since the inventory synced up, assume there was packet loss or something
			p->inv.active_item = p->inv.staging_item;
			ASSERT(p->inv.active_item >= 0 && p->inv.active_item < Game_Inventory::NUM_GAME_ITEMS);
			p->inv.ammo[p->inv.active_item] = p->inv.staging_ammo;
			p->inv.clip[p->inv.active_item] = p->inv.staging_clip;
		}
	}


	if (inv.active_item < 0 || inv.active_item >= Game_Inventory::NUM_GAME_ITEMS) {
		sys_print("invalid item");
		inv.active_item = Game_Inventory::UNEQUIP;
		inv.state = ITEM_IDLE;
		inv.timer = 0.f;
	}

	if (inv.pending_item != -1 && inv.state != ITEM_LOWERING) {
		sys_print("pending but lowering?\n");
		inv.pending_item = -1;
	}

	// check swaps
	if (command.button_mask & BUTTON_ITEM_PREV) {
		int next_item = inv.active_item - 1;
		if (next_item < 0) next_item = Game_Inventory::NUM_GAME_ITEMS - 1;
		change_to_item(*p, next_item);
	}
	else if (command.button_mask & BUTTON_ITEM_NEXT) {
		int next_item = (inv.active_item + 1) % Game_Inventory::NUM_GAME_ITEMS;
		change_to_item(*p, next_item);
	}

	if (is_local) {
		if (eng->local.viewmodel == nullptr) {
			eng->local.viewmodel = FindOrLoadModel("m16_fp.glb");
			eng->local.viewmodel_animator.set_model(eng->local.viewmodel);
		}
	}

	// tick timer
	if (inv.timer > 0)
		inv.timer -= eng->tick_interval;

	vec3 look_vec = AnglesToVector(command.view_angles.x, command.view_angles.y);
	bool wants_shoot = command.button_mask & BUTTON_FIRE1;
	bool wants_reload = command.button_mask & BUTTON_RELOAD;
	Game_Item_Stats& item_stats = get_item_stats()[inv.active_item];
	int cat = item_stats.category;

	if (inv.clip[inv.active_item] <= 0) wants_reload = true;

	switch (inv.state)
	{
	case ITEM_IDLE:
		if (wants_shoot) {
			if (cat == ITEM_CAT_RIFLE && inv.clip[inv.active_item] <= 0) {
				// empty sound
			}
			else if (cat == ITEM_CAT_RIFLE || cat == ITEM_CAT_MELEE || cat == ITEM_CAT_BOLT_ACTION) {
				inv.timer = 1.0 / item_stats.fire_rate;
				inv.state = ITEM_IN_FIRE;

				if (item_stats.category == ITEM_CAT_RIFLE) {
					p->anim.set_anim("act_shoot", true);
					eng->fire_bullet(p, look_vec, p->position + vec3(0, STANDING_EYE_OFFSET, 0));
				}
				else if (item_stats.category == ITEM_CAT_BOLT_ACTION) {
					p->anim.set_anim("act_shoot_sniper", true);
					eng->fire_bullet(p, look_vec, p->position + vec3(0, STANDING_EYE_OFFSET, 0));
				}
				else if (item_stats.category == ITEM_CAT_MELEE) {
					p->anim.set_anim("act_knife_attack", true);
				}
				p->anim.m.loop = false;


				if (is_local) {
					eng->local.viewmodel_animator.set_anim("act_shoot", true);
					eng->local.viewmodel_animator.m.loop = false;
				}

				break;
			}
			else if (cat == ITEM_CAT_BOMB || cat == ITEM_CAT_THROWABLE) {
				// todo
			}
		}
		else if (wants_reload && cat == ITEM_CAT_RIFLE && inv.ammo[inv.active_item] > 0) {
			inv.timer = item_stats.reload_time;
			inv.state = ITEM_RELOAD;

			p->anim.set_anim("act_reload", true);
			p->anim.m.loop = false;
			p->anim.m.speed = 0.8f;

			if (is_local) {
				eng->local.viewmodel_animator.set_anim("act_reload", true);
				eng->local.viewmodel_animator.m.loop = false;
				eng->local.viewmodel_animator.m.speed = 2.5f;

			}
		}
		else {
			if (cat == ITEM_CAT_RIFLE) {
				p->anim.set_anim("act_idle", false);
			}
			else if (cat == ITEM_CAT_BOLT_ACTION) {
				p->anim.set_anim("act_idle_sniper", false);
			}
			else if (cat == ITEM_CAT_MELEE) {
				p->anim.set_anim("act_idle_knife", true);
			}
			else if (cat == ITEM_CAT_BOMB || cat == ITEM_CAT_THROWABLE) {
				p->anim.set_anim("act_idle_item", true);
			}
			p->anim.m.loop = true;

		}
			break;
	case ITEM_LOWERING:
		if (inv.timer <= 0) {
			ASSERT(inv.pending_item >= 0 && inv.pending_item < Game_Inventory::NUM_GAME_ITEMS);
			Game_Item_Stats& next = get_item_stats()[inv.pending_item];
			inv.active_item = inv.pending_item;
			inv.pending_item = -1;
			inv.state = ITEM_RAISING;
			inv.timer = next.draw_time;
			sys_print("changed to raising: %d\n", inv.active_item);
		}
		break;
	case ITEM_RAISING:
		if (inv.timer <= 0) {
			inv.state = ITEM_IDLE;
			sys_print("changed to: %d\n", inv.active_item);
		}
		break;
	case ITEM_RELOAD:
		if (inv.timer <= 0) {
			inv.ammo[inv.active_item] += inv.clip[inv.active_item];
			inv.clip[inv.active_item] = 0;
			int reload = glm::min(item_stats.clip_size, inv.ammo[inv.active_item]);
			inv.clip[inv.active_item] += reload;
			inv.ammo[inv.active_item] -= reload;

			sys_print("reloaded %d %d\n", inv.active_item, inv.clip[inv.active_item]);

			inv.state = ITEM_IDLE;
		}
		break;
	case ITEM_IN_FIRE:
		if (inv.timer <= 0) inv.state = ITEM_IDLE;
		break;
	case ITEM_USING:
		break;
	}
}

// item code and animation updates
void player_post_physics(Entity* p, Move_Command command, bool is_local)
{
	// animation is being controlled server side
	if (!(p->flags & EF_FORCED_ANIMATION)) {
		player_item_update(p, command, is_local);
		player_animation_update(p);
	}
}