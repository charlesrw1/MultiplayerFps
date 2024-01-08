#include "Server.h"
#include "Types.h"
#include "Level.h"
#include "Movement.h"
#include "Game_Engine.h"
#include "Net.h"

void PlayerDeathUpdate(Entity* ent);
void player_update(Entity* ent);
void player_spawn(Entity* ent);
void PlayerUpdateAnimations(Entity* ent);
void PlayerItemUpdate(Entity* ent, Move_Command cmd);
void dummy_update(Entity* ent);

Entity* CreateGrenade(Entity* from, glm::vec3 org, glm::vec3 vel, int gtype);
void GrenadeUpdate(Entity* ent);

void PostEntUpdate(Entity* ent);

void EntTakeDamage(Entity* ent, Entity* from, int amt);

Entity* ServerEntForIndex(int index)
{
	ASSERT(index >= 0 && index < MAX_GAME_ENTS);
	return &engine.ents[index];
}

void GetPlayerSpawnPoisiton(Entity* ent)
{
	if (engine.level->spawns.size() > 0) {
		ent->position = engine.level->spawns[0].position;
		ent->rotation.y = engine.level->spawns[0].angle;
	}
	else {
		ent->position = glm::vec3(0);
		ent->rotation = glm::vec3(0);
	}
}

void Game_Engine::client_leave(int slot)
{
	printf("removing client %d from game\n", slot);
	Entity& ent = ents[slot];
	free_entity(&ent);
}

void Game_Engine::make_client(int slot)
{
	printf("spawning client %d into game\n", slot);
	Entity& e = ents[slot];
	if (e.active()) printf("player slot %d already in use?\n", slot);
	e = Entity();
	e.index = slot;
	
	player_spawn(&e);
}

Entity* Game_Engine::new_entity()
{
	int slot = MAX_CLIENTS;
	for (; slot < MAX_GAME_ENTS; slot++) {
		if (ents[slot].type == Ent_Free)
			break;
	}
	if (slot == MAX_GAME_ENTS) return nullptr;
	num_entities++;
	Entity& e = ents[slot];
	e = Entity();
	e.type = Ent_InUse;	// hack
	e.index = slot;
	return &e;
}

void Game_Engine::free_entity(Entity* ent)
{
	*ent = Entity();
}


void ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org)
{
	printf("Shooting bullets\n");
	Ray r;
	r.dir = dir;
	r.pos = org;
	RayHit hit;
	//TraceRayAgainstLevel(level, r, &hit, false);

	engine.phys.TraceRay(r, &hit, from->index, Pf_All);

	//RayWorldIntersect(r, &hit, GetEntIndex(from), Pf_All);

	// >>>
	CreateGrenade(from, org + dir * 0.1f, dir * 18.f, 0);
	// <<<

	if (hit.hit_world)
		return;

	Entity* ent = engine.ents + hit.ent_id;
	EntTakeDamage(ent, from, 26);
	

	//if (hit.dist >= 0.f) {
	//	rays.PushLine(org, hit.pos, COLOR_WHITE);
	//	rays.AddSphere(hit.pos, 0.1f, 5, 6, COLOR_BLACK);
	//}
}

void player_spawn(Entity* ent)
{
	ent->type = Ent_Player;
	ent->model_index = Mod_PlayerCT;
	ent->SetModel(Mod_PlayerCT);
	ent->anim.ResetLayers();

	if (ent->model) {
		int idle = ent->model->animations->FindClipFromName("act_idle");
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->ducking = false;
	ent->health = 100;
	ent->alive = true;
	GetPlayerSpawnPoisiton(ent);

	ent->update = player_update;
}

Entity* dummy_spawn()
{
	Entity* ent = engine.new_entity();
	ent->type = Ent_Dummy;
	ent->position = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->alive = true;
	ent->health = 100;
	ent->SetModel(Mod_PlayerCT);
	if (ent->model)
		ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_run"));

	ent->update = dummy_update;

	return ent;
}

void EntTakeDamage(Entity* ent, Entity* from, int amt)
{
	if (!ent->alive)
		return;
	ent->health -= amt;
	if (ent->health <= 0) {
		ent->alive = false;
		ent->death_time = engine.time + 3.0;

		ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_die"));
		ent->anim.dont_loop = true;
		printf("died!\n");
	}
}


void ServerGameShootCallback(int entindex, bool altfire)
{
	Entity& ent = engine.ents[entindex];
	glm::vec3 shoot_vec = AnglesToVector(ent.view_angles.x, ent.view_angles.y);

	ShootBullets(&ent, shoot_vec,
		ent.position + glm::vec3(0, STANDING_EYE_OFFSET, 0));
	
}
void ServerPlaySoundCallback(vec3 org, int snd_idx)
{
	printf("play sound: %d\n", snd_idx);
}
void ServerViewmodelCallback(const char* str) { /* null */ }




void Entity::SetModel(GameModels m) {
	model_index = m;
	model = media.gamemodels.at(m);
	if (model && model->bones.size() > 0)
		anim.set_model(model);
}

void Entity::from_entity_state(EntityState& es)
{
	type = (EntType)es.type;
	position = es.position;
	rotation = es.angles;
	model_index = es.model_idx;
	
	item = es.item;
	solid = es.solid;

	if (model_index >= 0 && model_index < media.gamemodels.size())
		model = media.gamemodels.at(model_index);
	
	if(model&&model->bones.size()>0)
		anim.set_model(model);

	anim.leganim = es.leganim;
	anim.leganim_frame = es.leganim_frame;
	anim.mainanim = es.mainanim;
	anim.mainanim_frame = es.mainanim_frame;
}

PlayerState Entity::ToPlayerState() const
{
	PlayerState ps{};
	ps.position = position;
	ps.angles = rotation;
	ps.ducking = ducking;
	ps.on_ground = on_ground;
	ps.velocity = velocity;
	ps.alive = alive;

	ps.items = items;
	ps.in_jump = in_jump;
	return ps;
}
void Entity::FromPlayerState(PlayerState* ps)
{
	position = ps->position;
	rotation = ps->angles;
	ducking = ps->ducking;
	on_ground = ps->on_ground;
	velocity = ps->velocity;
	alive = ps->alive;

	items = ps->items;
	in_jump = ps->in_jump;
}
EntityState Entity::to_entity_state()
{
	EntityState es;
	es.type = type;
	es.position = position;
	es.angles = rotation;
	es.model_idx = model_index;
	es.solid = solid;
	es.leganim = anim.leganim;
	es.leganim_frame = anim.leganim_frame;
	es.mainanim = anim.mainanim;
	es.mainanim_frame = anim.mainanim_frame;

	return es;
}


#include "MeshBuilder.h"
#include "Config.h"
void ExecutePlayerMove(Entity* ent, Move_Command cmd)
{
	double oldtime = engine.time;
	engine.time = cmd.tick * engine.tick_interval;

	ent->view_angles = cmd.view_angles;
	MeshBuilder mb;
	//phys_debug.Begin();
	PlayerMovement move;
	move.cmd = cmd;
	move.deltat = engine.tick_interval;
	move.phys_debug = &mb;
	move.phys = &engine.phys;
	move.fire_weapon = ServerGameShootCallback;
	move.play_sound = ServerPlaySoundCallback;
	move.set_viewmodel_animation = ServerViewmodelCallback;

	move.player = ent->ToPlayerState();
	move.entindex = ent->index;
	move.max_ground_speed = cfg.find_var("max_ground_speed")->real;
	move.simtime = engine.time;
	move.Run();

	ent->FromPlayerState(&move.player);

	engine.time = oldtime;

	//phys_debug.End();
}

const float fall_speed_threshold = -0.05f;
const float grnd_speed_threshold = 0.025f;

void PlayerUpdateAnimations(Entity* ent)
{
	glm::vec3 ent_face_dir = AnglesToVector(ent->view_angles.x, ent->view_angles.y);

	auto playeranims = ent->model->animations.get();
	float groundspeed = glm::length(glm::vec2(ent->velocity.x, ent->velocity.z));
	bool falling = ent->velocity.y < fall_speed_threshold;
	ent->anim.dont_loop = false;
	int leg_anim = 0;
	float speed = 1.f;

	const char* newanim = "null";
	if (ent->in_jump) {
		newanim = "act_jump";
	}
	else if (groundspeed > grnd_speed_threshold) {
		if (ent->ducking)
			newanim = "act_crouch_walk";
		else {
			newanim = "act_run";
			speed = ((groundspeed-grnd_speed_threshold) /6.f) + 1.f;

			if (dot(ent_face_dir, ent->velocity) < -0.25) {
				speed = -speed;
			}

		}
	}
	else {
		if (ent->ducking)
			newanim = "act_crouch_idle";
		else
			newanim = "act_idle";
	}

	// pick out upper body animations here
	// shooting, reloading, etc.

	leg_anim = ent->model->animations->FindClipFromName(newanim);

	if (leg_anim != ent->anim.leganim) {
		ent->anim.SetLegAnim(leg_anim);
	}
	ent->anim.SetLegAnimSpeed(speed);
}

void PlayerDeathUpdate(Entity* ent)
{
	if (ent->death_time < engine.time) {
		ent->health = 100;
		ent->alive = true;
		GetPlayerSpawnPoisiton(ent);
	}
}

void player_update(Entity* ent)
{
	if (ent->alive)
		PlayerUpdateAnimations(ent);
	else
		PlayerDeathUpdate(ent);

	if (ent->position.y < -50 && ent->alive) {
		ent->alive = false;
		ent->death_time = engine.time + 0.5f;
	}
}
void dummy_update(Entity* ent)
{
	//ent->position.y = sin(GetTime()) * 2.f + 2.f;
	ent->position.x = 0.f;
	if (!ent->alive)
		engine.free_entity(ent);
}

Entity* CreateGrenade(Entity* thrower, glm::vec3 org, glm::vec3 start_vel, int grenade_type)
{
	ASSERT(thrower);
	Entity* e = engine.new_entity();
	e->type = Ent_Grenade;

	e->SetModel(Mod_Grenade_HE);
	e->owner_index = thrower->index;
	e->position = org;
	e->velocity = start_vel;
	e->sub_type = grenade_type;
	e->alive = true;
	e->death_time = engine.time + 5.f;

	e->update = GrenadeUpdate;
	return e;
}

void RunProjectilePhysics(Entity* ent)
{
	// update physics, detonate if ready
	float dt = engine.tick_interval;
	ent->velocity.y -= 12.f * dt;// gravity
	glm::vec3 next_position = ent->position + ent->velocity * dt;
	float len = glm::length(ent->velocity * dt);
	RayHit rh;
	Ray r;
	r.dir = (ent->velocity * dt) / len;
	r.pos = ent->position;
	engine.phys.TraceRay(r, &rh, ent->owner_index, Pf_All);
	//g->RayWorldIntersect(r, &rh, ent->owner_index, Pf_All);
	if (rh.hit_world && rh.dist < len) {
		ent->position = r.at(rh.dist) + rh.normal * 0.01f;
		ent->velocity = glm::reflect(ent->velocity, rh.normal);
		ent->velocity *= 0.6f;
	}
	else {
		ent->position = next_position;
	}
}


void GrenadeUpdate(Entity* ent)
{
	RunProjectilePhysics(ent);
	// spin grenade based on velocity
	float dt = engine.tick_interval;

	float vel = glm::length(ent->velocity);

	ent->rotation.x += 0.7 * dt * vel;
	ent->rotation.y -= 1.3 * dt * vel;

	if (ent->death_time < engine.time) {
		printf("BOOM\n");
		engine.free_entity(ent);
	}
}


#if 0
void Game::RemoveEntity(Entity* ent)
{
	ent->type = Ent_Free;
	ent->alive = false;
	ent->model = nullptr;
	ent->anim.Clear();

	engine.num_entities--;
}
#endif

void KillEnt(Entity* ent)
{
	ent->alive = false;
	ent->death_time = engine.time + 5.0;
	ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_die"));
	ent->anim.dont_loop = true;
}
