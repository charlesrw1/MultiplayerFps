#include "Movement.h"
#include "Util.h"
#include "Physics.h"
#include "MeshBuilder.h"
#include "Animation.h"
#include "GameData.h"

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

using glm::vec3;
using glm::vec2;
using glm::dot;
using glm::cross;

void PlayerMovement::TriggerEvent(EntityEvent ev, int parm)
{
	if (num_events < MAX_EVENTS) {
		triggered_events[num_events] = ev;
		trig_event_parms[num_events] = parm;
		num_events++;
	}
	else {
		printf("playermovement: event overflow\n");
	}
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
	if (player.on_ground && cmd.button_mask & CmdBtn_Jump) {
		printf("jump\n");
		player.velocity.y += jumpimpulse;
		player.on_ground = false;

		player.in_jump = true;
	}
}
void PlayerMovement::CheckDuck()
{
	if (cmd.button_mask & CmdBtn_Duck) {
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
	else if (!(cmd.button_mask & CmdBtn_Duck) && player.ducking) {
		int steps = 2;
		float step = 0.f;
		float sphere_radius = 0.f;
		vec3 offset = vec3(0.f);
		vec3 a, b, c, d;
		standing_capsule.GetSphereCenters(a, b);
		crouch_capsule.GetSphereCenters(c, d);
		float len = b.y - d.y;
		sphere_radius = crouch_capsule.radius;
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

			PhysContainer obj;
			obj.type = PhysContainer::SphereType;
			obj.sph.origin = where;
			obj.sph.radius = sphere_radius;

			trace_callback(&res, obj, true, false, ignore_ent);

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
	Capsule cap = (player.ducking) ? crouch_capsule : standing_capsule;
	PhysContainer obj;
	obj.type = PhysContainer::CapsuleType;
	obj.cap = cap;

	vec3 position = player.position;
	vec3 step = delta / (float)col_iters;
	for (int i = 0; i < col_iters; i++)
	{
		position += step;
		GeomContact trace;
		obj.cap = cap;
		obj.cap.tip += position;
		obj.cap.base += position;
	
		trace_callback(&trace, obj, col_closest, false, ignore_ent);

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
		obj.cap = cap;
		obj.cap.tip += position;
		obj.cap.base += position;

		trace_callback(&res, obj, col_closest, false, ignore_ent);
		//TraceCapsule(level, position, cap, &res, col_closest);
		if (res.found) {
			position += res.penetration_normal * (res.penetration_depth);////trace.0.001f + slide_velocity * dt;
		}
	}
	player.position = position;
}

void PlayerMovement::CheckGroundState()
{
	if (player.velocity.y > 2.f) {
		player.on_ground = false;
		return;
	}
	GeomContact result;
	vec3 where = player.position - vec3(0, 0.005 - standing_capsule.radius, 0);
	PhysContainer obj;
	obj.type = PhysContainer::SphereType;
	obj.sph.origin = where;
	obj.sph.radius = standing_capsule.radius;

	trace_callback(&result, obj, true, true, ignore_ent);

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
	num_events = 0;

	player = in_state;
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

}

//
//	WEAPON CONTROLLER CODE
//
void WeaponController::TriggerEvent(EntityEvent ev, int parm)
{
	if (num_events < MAX_EVENTS) {
		events[num_events] = ev;
		event_parms[num_events] = parm;
		num_events++;
	}
	else {
		printf("weapon controller: events full\n");
	}
}
void WeaponController::Run()
{
	num_events = 0;
	shoot_vec = AnglesToVector(cmd.view_angles.x, cmd.view_angles.y);

	bool wants_shoot = cmd.button_mask & CmdBtn_Misc1;
	bool wants_reload = cmd.button_mask & CmdBtn_Misc2;

	// unarmed
	//if (ent->gun_id == -1)
	//	return;
	//
	//if (ent->reloading && ent->gun_timer < server.time) {
	//	ent->reloading = false;
	//	int amt = glm::min((short)10, ent->ammo[ent->gun_id]);
	//	ent->clip[ent->gun_id] += amt;
	//	ent->ammo[ent->gun_id] -= amt;
	//	printf("reloaded\n");
	//}
	//

	WpnState* w = &state.weapons;

	if (w->reloading || w->gun_timer >= simtime)
		return;

	if (wants_shoot) {// && ent->clip[ent->gun_id] > 0) {
		//server.sv_game.ShootBullets(ent,
		//	AnglesToVector(cmd.view_angles.x, cmd.view_angles.y),
		//	ent->position + glm::vec3(0, STANDING_EYE_OFFSET, 0));
		TriggerEvent(Ev_FirePrimary);
		w->gun_timer = simtime + 0.1f;
		w->clip[w->gun_id]--;
	}
	else if ((w->clip[w->gun_id] <= 0 || wants_reload) && w->ammo[w->gun_id] > 0) {
		TriggerEvent(Ev_Reload);
		w->reloading = true;
		w->gun_timer += 1.0f;
	}
}