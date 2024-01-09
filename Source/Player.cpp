#include "Movement.h"
#include "Util.h"
#include "Physics.h"
#include "MeshBuilder.h"
#include "Animation.h"
#include "GameData.h"
#include "Game_Engine.h"


//
//	PLAYER MOVEMENT CODE
//

static const Capsule standing_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_STANDING_HB_HEIGHT,0) };
static const Capsule crouch_capsule = { CHAR_HITBOX_RADIUS,vec3(0.f),vec3(0,CHAR_CROUCING_HB_HEIGHT,0) };


static float move_speed_player = 0.1f;
static bool gravity = true;
static bool col_response = true;
static int col_iters = 2;
static bool col_closest = true;
static bool new_physics = true;

static float ground_friction = 10;
static float air_friction = 0.01;
static float gravityamt = 16;
static float ground_accel = 6;
static float ground_accel_crouch = 4;
static float air_accel = 3;
static float minspeed = 1;
static float maxspeed = 30;
static float jumpimpulse = 5.f;
static float max_ground_speed = 10;
static float max_air_speed = 2;

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

Your character dies on server, server functions set youre animation to death one with server animation override flag


*/

int character_state = 0;
int item_state = 0;
bool character_state_changed = false;
bool item_state_changed = false;
bool finished = false;
bool force_animation = false;

void player_update_()
{
	// run physics for input
	// run item code for input (this will update animations as well)
	// run server-only think code
}
void player_predict_()
{
	// set entity to last player state (only physics vars, item code has special predict update)
	// for inputs since last state
	//    run physics (effects flag = true on current input)
	// run item code for current input
}

void player_set_anim(Entity* p, Player_Item_State state)
{
	// now calculate player state (running, standing, crouched,...)
	// combine them
}

void player_update_animation(Entity* p)
{
	if (finished && !force_animation)
		player_set_anim(p, ITEM_STATE_IDLE);
}

void PlayerMovement::CheckNans()
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

void PlayerMovement::CheckJump()
{
	if (player.on_ground && cmd.button_mask & BUTTON_JUMP) {
		printf("jump\n");
		player.velocity.y += jumpimpulse;
		player.on_ground = false;

		player.in_jump = true;
	}
}
void PlayerMovement::CheckDuck()
{
	if (cmd.button_mask & BUTTON_DUCK) {
		if (player.on_ground) {
			player.ducking = true;
		}
		else if (!player.on_ground && !player.ducking) {
			const Capsule& st = standing_capsule;
			const Capsule& cr = crouch_capsule;
			// Move legs of player up
			player.position.y += st.tip.y - cr.tip.y;
			player.ducking = true;
		}
	}
	else if (!(cmd.button_mask & BUTTON_DUCK) && player.ducking) {
		int steps = 2;
		float step = 0.f;
		float sphere_radius = 0.f;
		vec3 offset = vec3(0.f);
		vec3 a, b, c, d;
		standing_capsule.GetSphereCenters(a, b);
		crouch_capsule.GetSphereCenters(c, d);
		float len = b.y - d.y;
		sphere_radius = crouch_capsule.radius-0.05;
		if (player.on_ground) {
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

			//PhysContainer obj;
			//obj.type = PhysContainer::SphereType;
			//obj.sph.origin = where;
			//obj.sph.radius = sphere_radius;


			phys->TraceSphere(SphereShape(where, sphere_radius), &res, entindex, Pf_All);
			//obj_trace(&res, obj, true, false, entindex);

			//TraceSphere(level, where, sphere_radius, &res, true, false);
			if (res.found) {
				phys_debug->AddSphere(where, sphere_radius, 10, 8, COLOR_RED);
				break;
			}
		}
		if (i == steps) {
			player.ducking = false;
			if (!player.on_ground) {
				player.position.y -= len;
			}
		}
	}
}

void PlayerMovement::ApplyFriction(float friction_val)
{
	float speed = length(player.velocity);
	if (speed < 0.0001)
		return;

	float dropamt = friction_val * speed * deltat;

	float newspd = speed - dropamt;
	if (newspd < 0)
		newspd = 0;
	float factor = newspd / speed;

	player.velocity.x *= factor;
	player.velocity.z *= factor;

}
void PlayerMovement::MoveAndSlide(vec3 delta)
{

	CharacterShape character;
	character.height = (player.ducking)? CHAR_CROUCING_HB_HEIGHT :  CHAR_STANDING_HB_HEIGHT;
	character.org = glm::vec3(0.f);
	character.m = nullptr;
	character.a = nullptr;
	character.radius = CHAR_HITBOX_RADIUS;


	vec3 position = player.position;
	vec3 step = delta / (float)col_iters;
	for (int i = 0; i < col_iters; i++)
	{
		position += step;
		GeomContact trace;

		character.org = position;

		phys->TraceCharacter(character, &trace, entindex, Pf_All);

		//obj_trace(&trace, obj, col_closest, false, entindex);

		//TraceCapsule(level, position, cap, &trace, col_closest);
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
		phys->TraceCharacter(character, &res, entindex, Pf_All);

		//obj_trace(&res, obj, col_closest, false, entindex);
		//TraceCapsule(level, position, cap, &res, col_closest);
		if (res.found) {
			position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
		}
	}
	player.position = position;
}

void player_physics_check_ground(Entity& player)
{
	if (player.velocity.y > 2.f) {
		player.on_ground = false;
		return;
	}
	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);

	engine.phys.TraceSphere(SphereShape(where, CHAR_HITBOX_RADIUS), &result, player.index, Pf_All);

	//TraceSphere(level, where, radius, &result, true, true);
	if (!result.found)
		player.on_ground = false;
	else if (result.surf_normal.y < 0.3)
		player.on_ground = false;
	else {
		player.on_ground = true;
		//phys_debug.AddSphere(where, radius, 8, 6, COLOR_BLUE);
	}

	if (player.on_ground)
		player.in_jump = false;
}
void PlayerMovement::CheckGroundState()
{
	if (player.velocity.y > 2.f) {
		player.on_ground = false;
		return;
	}
	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);

	phys->TraceSphere(SphereShape(where, CHAR_HITBOX_RADIUS), &result, entindex, Pf_All);

	//TraceSphere(level, where, radius, &result, true, true);
	if (!result.found)
		player.on_ground = false;
	else if (result.surf_normal.y < 0.3)
		player.on_ground = false;
	else {
		player.on_ground = true;
		//phys_debug.AddSphere(where, radius, 8, 6, COLOR_BLUE);
	}

	if (player.on_ground)
		player.in_jump = false;
}

void player_physics_check_jump(Entity& player, Move_Command command)
{
	if (player.on_ground && command.button_mask & BUTTON_JUMP) {
		printf("jump\n");
		player.velocity.y += jumpimpulse;
		player.on_ground = false;
		player.in_jump = true;
	}
}
void player_physics_check_duck(Entity& player, Move_Command cmd)
{
	if (cmd.button_mask & BUTTON_DUCK) {
		if (player.on_ground) {
			player.ducking = true;
		}
		else if (!player.on_ground && !player.ducking) {
			const Capsule& st = standing_capsule;
			const Capsule& cr = crouch_capsule;
			// Move legs of player up
			player.position.y += st.tip.y - cr.tip.y;
			player.ducking = true;
		}
	}
	else if (!(cmd.button_mask & BUTTON_DUCK) && player.ducking) {
		int steps = 2;
		float step = 0.f;
		float sphere_radius = 0.f;
		vec3 offset = vec3(0.f);
		vec3 a, b, c, d;
		standing_capsule.GetSphereCenters(a, b);
		crouch_capsule.GetSphereCenters(c, d);
		float len = b.y - d.y;
		sphere_radius = crouch_capsule.radius - 0.05;
		if (player.on_ground) {
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

			engine.phys.TraceSphere(SphereShape(where, sphere_radius), &res, player.index, Pf_All);
		}
		if (i == steps) {
			player.ducking = false;
			if (!player.on_ground) {
				player.position.y -= len;
			}
		}
	}
}

void player_physics_move(Entity& player)
{

	CharacterShape character;
	character.height = (player.ducking) ? CHAR_CROUCING_HB_HEIGHT : CHAR_STANDING_HB_HEIGHT;
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

		engine.phys.TraceCharacter(character, &trace, player.index, Pf_All);
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
		engine.phys.TraceCharacter(character, &res, player.index, Pf_All);

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

	float acceleation_val = (player.on_ground) ? ground_accel : air_accel;
	acceleation_val = (player.ducking) ? ground_accel_crouch : acceleation_val;
	float maxspeed_val = (player.on_ground) ? max_ground_speed : max_air_speed;

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
	if (!player.on_ground)
		player.velocity.y -= gravityamt * engine.tick_interval;
	player_physics_move(player);
	if (player.alive) {
		player.rotation = vec3(0.f);
		player.rotation.y = HALFPI - command.view_angles.y;
	}
}

void PlayerMovement::GroundMove()
{
	float acceleation_val = (player.on_ground) ? ground_accel : air_accel;
	acceleation_val = (player.ducking) ? ground_accel_crouch : acceleation_val;
	float maxspeed_val = (player.on_ground) ? max_ground_speed : max_air_speed;

	vec3 wishdir = (look_front * inp_dir.x + look_side * inp_dir.y);
	wishdir = vec3(wishdir.x, 0.f, wishdir.z);
	vec3 xz_velocity = vec3(player.velocity.x, 0, player.velocity.z);

	float wishspeed = inp_len * maxspeed_val;
	float addspeed = wishspeed - dot(xz_velocity, wishdir);
	addspeed = glm::max(addspeed, 0.f);
	float accelspeed = acceleation_val * wishspeed * deltat;
	accelspeed = glm::min(accelspeed, addspeed);
	xz_velocity += accelspeed * wishdir;

	float len = length(xz_velocity);
	//if (len > maxspeed)
	//	xz_velocity = xz_velocity * (maxspeed / len);
	if (len < 0.3 && accelspeed < 0.0001)
		xz_velocity = vec3(0);
	player.velocity = vec3(xz_velocity.x, player.velocity.y, xz_velocity.z);


	CheckJump();
	CheckDuck();
	if (!player.on_ground)
		player.velocity.y -= gravityamt * deltat;

	vec3 delta = player.velocity * deltat;

	MoveAndSlide(delta);

	if (player.alive) {
		player.angles = vec3(0.f);
		player.angles.y = HALFPI - cmd.view_angles.y;
	}
}
void PlayerMovement::AirMove()
{

}
void PlayerMovement::Run()
{
	if (!player.alive) {
		cmd.forward_move = 0;
		cmd.lateral_move = 0;
		cmd.up_move = 0;
	}
	vec2 inputvec = vec2(cmd.forward_move, cmd.lateral_move);
	inp_len = length(inputvec);
	if (inp_len > 0.00001)
		inp_dir = inputvec / inp_len;
	if (inp_len > 1)
		inp_len = 1;

	phys_debug->PushLine(player.position, player.position + glm::vec3(inputvec.y, 0.2f, inputvec.x), COLOR_WHITE);

	// Store off last values for interpolation
	//p->lastorigin = p->origin;
	//p->lastangles = p->angles;

	//player.viewangles = inp.desired_view_angles;
	look_front = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);
	look_front.y = 0;
	look_front = normalize(look_front);
	look_side = -cross(look_front, vec3(0, 1, 0));

	float fric_val = (player.on_ground) ? ground_friction : air_friction;
	ApplyFriction(fric_val);
	CheckGroundState();	// check ground after applying friction, like quake
	GroundMove();
	CheckNans();

	RunItemCode();
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
	if (!p->alive) {
		command.forward_move = 0;
		command.lateral_move = 0;
		command.up_move = 0;
	}
	
	// Friction
	float friction_value = (p->on_ground) ? ground_friction : air_friction;
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
	player_physics_check_ground(*p);
	player_physics_ground_move(*p, command);
	player_physics_check_nans(*p);

	//RunItemCode();
}


void PlayerMovement::RunItemCode()
{
	vec3 shoot_vec = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);

	bool wants_shoot = cmd.button_mask & BUTTON_FIRE1;
	bool wants_reload = cmd.button_mask & BUTTON_RELOAD;

	Item_State* w = &player.items;
	if (!(w->active_item >= 0 && w->active_item < Item_State::MAX_ITEMS)) {
		printf("invalid active_item\n");
		return;
	}

	if (w->reloading || w->gun_timer >= simtime)
		return;

	if (wants_shoot) {
		if (fire_weapon)
			fire_weapon(entindex, false);

		w->gun_timer = simtime + 0.1f;
		w->clip[w->active_item]--;
		w->state = Item_InFire;

		//view_recoil_add.x = PI / 16.f;
	}
	else if ((w->clip[w->active_item] <= 0 || wants_reload) && w->ammo[w->active_item] > 0) {
		w->reloading = true;
		w->gun_timer += 1.0f;
		w->state = Item_Reload;
	}
	else {
		w->state = ITEM_IDLE;
		w->reloading = false;
	}
}
