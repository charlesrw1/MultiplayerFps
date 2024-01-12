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
static float gravityamt = 16;
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
	ImGui::SliderFloat("gravityamt", &gravityamt, 0, 30);
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

void player_physics_check_ground(Entity& player, vec3 pre_update_velocity, Move_Command command)
{

	if (player.velocity.y > 2.f) {
		player.state &= ~PMS_GROUND;
		return;
	}
	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);

	engine.phys.TraceSphere(SphereShape(where, CHAR_HITBOX_RADIUS), &result, player.index, PF_ALL);

	//TraceSphere(level, where, radius, &result, true, true);
	if (!result.found)
		player.state &= ~PMS_GROUND;
	else if (result.surf_normal.y < 0.3)
		player.state &= ~PMS_GROUND;
	else {
		if (command.first_sim && !(player.state & PMS_GROUND) && pre_update_velocity.y < -2.f) {
			player.anim.set_leg_anim("act_land", false);
 			player.anim.loop_legs = false;
		}
		player.state |= PMS_GROUND;
		//phys_debug.AddSphere(where, radius, 8, 6, COLOR_BLUE);
	}

	if (player.state & PMS_GROUND)
		player.state &= ~PMS_JUMPING;

	if ((player.state & PMS_GROUND))
		player.in_air_time = 0.f;
	else if (command.first_sim)
		player.in_air_time += engine.tick_interval;
}

void player_physics_check_jump(Entity& player, Move_Command command)
{
	if ((player.state & PMS_GROUND) && command.button_mask & BUTTON_JUMP) {
		printf("jump\n");
		player.velocity.y += jumpimpulse;
		player.state &= ~PMS_GROUND;
		player.state |= PMS_JUMPING;

		if (command.first_sim) {
			player.anim.set_leg_anim("act_jump_start", true);
			player.anim.loop_legs = false;
		}
	}
}
void player_physics_check_duck(Entity& player, Move_Command cmd)
{
	if (cmd.button_mask & BUTTON_DUCK) {
		if (player.state & PMS_GROUND) {
			player.state |= PMS_CROUCHING;
		}
		else if (!(player.state & PMS_GROUND) && !(player.state & PMS_CROUCHING)) {
			const Capsule& st = standing_capsule;
			const Capsule& cr = crouch_capsule;
			// Move legs of player up
			player.position.y += st.tip.y - cr.tip.y;
			player.state |= PMS_CROUCHING;
		}
	}
	else if (!(cmd.button_mask & BUTTON_DUCK) && (player.state & PMS_CROUCHING)) {
		int steps = 2;
		float step = 0.f;
		float sphere_radius = 0.f;
		vec3 offset = vec3(0.f);
		vec3 a, b, c, d;
		standing_capsule.GetSphereCenters(a, b);
		crouch_capsule.GetSphereCenters(c, d);
		float len = b.y - d.y;
		sphere_radius = crouch_capsule.radius - 0.05;
		if (player.state & PMS_GROUND) {
			step = len / (float)steps;
			offset = d;
		}
		else {
			steps = 3;
			// testing downwards to ground
			step = -(len + 0.1) / (float)steps;
			offset = c;
		}
		int i = 0;
		for (; i < steps; i++) {
			GeomContact res;
			vec3 where = player.position + offset + vec3(0, (i + 1) * step, 0);

			engine.phys.TraceSphere(SphereShape(where, sphere_radius), &res, player.index, PF_ALL);
		}
		if (i == steps) {
			player.state &= ~PMS_CROUCHING;
			if (!(player.state & PMS_GROUND)) {
				player.position.y -= len;
			}
		}
	}
}

void player_physics_move(Entity& player)
{

	CharacterShape character;
	character.height = (player.state & PMS_CROUCHING) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT;
	character.org = glm::vec3(0.f);
	character.m = nullptr;
	character.a = nullptr;
	character.radius = CHAR_HITBOX_RADIUS;

	vec3 delta = player.velocity * (float)engine.tick_interval;

	vec3 position = player.position;
	vec3 step = delta / (float)col_iters;
	for (int i = 0; i < col_iters; i++)
	{
		position += step;
		GeomContact trace;

		character.org = position;

		engine.phys.TraceCharacter(character, &trace, player.index, PF_ALL);
		if (trace.found)
		{
			vec3 penetration_velocity = dot(player.velocity, trace.penetration_normal) * trace.penetration_normal;
			vec3 slide_velocity = player.velocity - penetration_velocity;
			position += trace.penetration_normal * trace.penetration_depth;////trace.0.001f + slide_velocity * dt;
			player.velocity = slide_velocity;
		}
	}
	// Gets player out of surfaces
	for (int i = 0; i < 2; i++) {
		GeomContact res;

		character.org = position;
		engine.phys.TraceCharacter(character, &res, player.index, PF_ALL);

		if (res.found) {
			position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
		}
	}
	player.position = position;
}

void player_physics_ground_move(Entity& player, Move_Command command)
{
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

	player_physics_check_jump(player, command);
	player_physics_check_duck(player, command);
	if (!(player.state & PMS_GROUND))
		player.velocity.y -= gravityamt * engine.tick_interval;
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


	player_physics_ground_move(*p, command);
	player_physics_check_ground(*p, pre_update_velocity, command);
	player_physics_check_nans(*p);

	//RunItemCode();
}


// if in the middle of an animation, dont interrupt it

void player_animation_update(Entity* ent)
{
	// animation is being controlled server side
	if (ent->flags & EF_FORCED_ANIMATION)
		return;

	// upper body
	if (ent->anim.loop || ent->anim.finished)
	{	
		// for now, only one
		ent->anim.set_anim("act_idle", false);
		ent->anim.loop = true;
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
	if (ent->anim.leg_anim == -1)
		ent->anim.legs_finished = true;

	if (!ent->anim.loop_legs && !ent->anim.legs_finished)
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
		ent->anim.set_leg_anim(next_leg_anim, false);
		ent->anim.loop_legs = loop_legs;
		ent->anim.leg_play_speed = leg_speed;
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
static float reload_time = 1.f;

void player_item_update(Entity* p, Move_Command command)
{
	vec3 look_vec = AnglesToVector(command.view_angles.x, command.view_angles.y);

	bool wants_shoot = command.button_mask & BUTTON_FIRE1;
	bool wants_reload = command.button_mask & BUTTON_RELOAD;

	Item_State& w = p->items;
	if (!(w.active_item >= 0 && w.active_item < Item_State::MAX_ITEMS)) {
		printf("invalid active_item\n");
		return;
	}

	if (w.timer > 0)
		w.timer -= engine.tick_interval;

	switch (w.state)
	{
	case ITEM_IDLE:
		if (wants_shoot) {
			w.timer = fire_time;
			w.state = ITEM_IN_FIRE;

			p->anim.set_anim("act_shoot", true);
			p->anim.loop = false;

			engine.fire_bullet(p, look_vec, p->position + vec3(0, STANDING_EYE_OFFSET, 0));
		}
		if (wants_reload) {
			w.timer = reload_time;
			w.state = ITEM_RELOAD;

			p->anim.set_anim("act_reload", true);
			p->anim.loop = false;
			p->anim.play_speed = 0.7f;
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
void player_post_physics(Entity* p, Move_Command command)
{
	player_item_update(p, command);
	player_animation_update(p);
}