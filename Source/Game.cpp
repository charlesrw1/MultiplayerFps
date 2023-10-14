#include "Server.h"
#include "CoreTypes.h"
#include "Level.h"
#include "Movement.h"

Game game;

void PlayerDeathUpdate(Entity* ent);
void PlayerUpdate(Entity* ent);
void PlayerUpdateAnimations(Entity* ent);
void PlayerSpawn(Entity* ent);
void PlayerItemUpdate(Entity* ent, MoveCommand cmd);

void DummyUpdate(Entity* ent);
Entity* CreateDummy();

Entity* CreateGrenade(Entity* from, glm::vec3 org, glm::vec3 vel, int gtype);
void GrenadeUpdate(Entity* ent);

void PostEntUpdate(Entity* ent);

void EntTakeDamage(Entity* ent, Entity* from, int amt);

Entity* ServerEntForIndex(int index)
{
	ASSERT(index >= 0 && index < MAX_GAME_ENTS);
	return &game.ents[index];
}

void Game::GetPlayerSpawnPoisiton(Entity* ent)
{
	if (level->spawns.size() > 0) {
		ent->position = level->spawns[0].position;
		ent->rotation.y = level->spawns[0].angle;
	}
	else {
		ent->position = glm::vec3(0);
		ent->rotation = glm::vec3(0);
	}
}
Entity* Game::InitNewEnt(EntType type, int index)
{
	Entity* ent = &ents[index];
	ASSERT(ent->type == Ent_Free);
	ent->type = type;
	ent->index = index;
	ent->ducking = false;
	ent->model = nullptr;
	ent->position = glm::vec3(0.f);
	ent->velocity = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->scale = 1.f;

	ent->id = next_id++;

	ent->alive = false;
	ent->health = 0;
	ent->on_ground = false;

	return ent;
}

void Game::SpawnNewClient(int client)
{
	Entity* ent = InitNewEnt(Ent_Player, client);
	PlayerSpawn(ent);
	num_ents++;
	printf("spawned client %d into game\n", client);
}

void Game::OnClientLeave(int client)
{
	Entity* ent = &ents[client];
	RemoveEntity(ent);
	printf("remove client %d from game\n", client);
}

Entity* Game::MakeNewEntity(EntType type)
{
	int slot = MAX_CLIENTS;
	for (; slot < MAX_GAME_ENTS; slot++) {
		if (ents[slot].type == Ent_Free)
			break;
	}
	if (slot == MAX_GAME_ENTS)
		return nullptr;
	Entity* ent = InitNewEnt(type, slot);
	num_ents++;
	printf("spawning ent in slot %d\n", slot);
	return ent;
}

void Game::Init()
{
	ents.resize(MAX_GAME_ENTS);
	num_ents = 0;
	level = nullptr;

}

void Game::ClearState()
{
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		ents[i].type = Ent_Free;
	}
	num_ents = 0;
	phys.ClearObjs();
	if(level)
		FreeLevel(level);
	level = nullptr;
	next_id = 0;

}
bool Game::DoNewMap(const char* mapname)
{
	if (level) {
		ClearState();
	}
	level = LoadLevelFile(mapname);
	if (!level)
		Fatalf("level not loaded\n");

	BuildPhysicsWorld(0.f);

	return true;
}

void Game::ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org)
{
	printf("Shooting bullets\n");
	Ray r;
	r.dir = dir;
	r.pos = org;
	RayHit hit;
	//TraceRayAgainstLevel(level, r, &hit, false);

	phys.TraceRay(r, &hit, GetEntIndex(from), Pf_All);

	//RayWorldIntersect(r, &hit, GetEntIndex(from), Pf_All);

	// >>>
	CreateGrenade(from, org + dir * 0.1f, dir * 18.f, 0);
	// <<<

	if (hit.hit_world)
		return;

	Entity* ent = ents.data() + hit.ent_id;
	EntTakeDamage(ent, from, 26);
	

	if (hit.dist >= 0.f) {
		rays.PushLine(org, hit.pos, COLOR_WHITE);
		rays.AddSphere(hit.pos, 0.1f, 5, 6, COLOR_BLACK);
	}
}
#if 0
void Game::RayWorldIntersect(Ray r, RayHit* out, int skipent, PhysFilterFlags filter)
{
	TraceRayAgainstLevel(level, r, out, true);
	if (out->dist >= 0.f)
		out->hit_world = true;
	if (filter & (Pf_Players|Pf_Nonplayers)) {
		for (int i = 0; i < ents.size(); i++) {
			if (ents[i].type == Ent_Free) continue;
			Entity* ent = ents.data() + i;
			if (ent->type != Ent_Player && ent->type != Ent_Dummy)
				continue;
			if (i == skipent)
				continue;

			Bounds b;
			b.bmin = vec3(-CHAR_HITBOX_RADIUS, 0, -CHAR_HITBOX_RADIUS);
			b.bmax = vec3(CHAR_HITBOX_RADIUS, CHAR_STANDING_HB_HEIGHT, CHAR_HITBOX_RADIUS);
			if (ent->ducking) {
				b.bmax.y = CHAR_CROUCING_HB_HEIGHT;
			}
			b.bmin += ent->position;
			b.bmax += ent->position;


			float t_out = -1.f;
			bool has_hit = b.intersect(r, t_out);

			if (has_hit) {
				out->dist = t_out;
				out->ent_id = i;
				out->hit_world = false;
				out->pos = r.at(t_out);
			}
		}
	}
}
#endif

#if 0
void Game::PhysWorldTrace(PhysContainer obj, GeomContact* contact, int skipent, PhysFilterFlags filter)
{
	contact->found = false;
	contact->penetration_depth = -INFINITY;
	TraceAgainstLevel(level, contact, obj, true, true);
	if (!(filter & (Pf_Players|Pf_Nonplayers)))
		return;
	for (int i = 0; i < ents.size(); i++) {
		if (ents[i].type == Ent_Free || i == skipent) {
			continue;
		}
		if (ents[i].type == Ent_Grenade)
			continue;
		GeomContact c;
		CylinderCylinderIntersect(obj.cap.radius, obj.cap.base, obj.cap.tip.y - obj.cap.base.y, 
			CHAR_HITBOX_RADIUS, ents[i].position, CHAR_STANDING_HB_HEIGHT, &c);
		if (c.found && c.penetration_depth > contact->penetration_depth)
			*contact = c;
	}
}
#endif

void PlayerSpawn(Entity* ent)
{
	ASSERT(ent->type == Ent_Player);
	ent->SetModel(Mod_PlayerCT);
	ent->anim.ResetLayers();

	if (ent->model) {
		int idle = ent->model->animations->FindClipFromName("act_idle");
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->ducking = false;
	ent->health = 100;
	ent->alive = true;
	game.GetPlayerSpawnPoisiton(ent);
}

Entity* CreateDummy()
{
	Entity* ent = game.MakeNewEntity(Ent_Dummy);
	ent->position = glm::vec3(0.f);
	ent->rotation = glm::vec3(0.f);
	ent->alive = true;
	ent->health = 100;
	ent->SetModel(Mod_PlayerCT);
	if (ent->model)
		ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_run"));

	return ent;
}

void EntTakeDamage(Entity* ent, Entity* from, int amt)
{
	if (!ent->alive)
		return;
	ent->health -= amt;
	if (ent->health <= 0) {
		ent->alive = false;
		ent->death_time = server.simtime + 3.0;

		ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_die"));
		ent->anim.dont_loop = true;
		printf("died!\n");
	}
}


void ServerGameShootCallback(int entindex, bool altfire)
{
	Entity* ent = game.EntForIndex(entindex);
	glm::vec3 shoot_vec = AnglesToVector(ent->view_angles.x, ent->view_angles.y);

	game.ShootBullets(ent, shoot_vec,
		ent->position + glm::vec3(0, STANDING_EYE_OFFSET, 0));
	
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
		anim.Init(model);
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

	ps.items = wpns;
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

	wpns = ps->items;
	in_jump = ps->in_jump;
}
EntityState Entity::ToEntState() const
{
	EntityState es{};
	es.angles = rotation;
	es.ducking = ducking;
	es.leganim = anim.leganim;
	es.leganim_frame = anim.leganim_frame;
	es.mainanim = anim.mainanim;
	es.mainanim_frame = anim.mainanim_frame;
	es.position = position;
	es.type = type;

	es.model_idx = model_index;

	return es;
}
#include "MeshBuilder.h"
#include "Config.h"
void Game::ExecutePlayerMove(Entity* ent, MoveCommand cmd)
{
	double oldtime = server.simtime;
	server.simtime = cmd.tick * core.tick_interval;

	ent->view_angles = cmd.view_angles;
	MeshBuilder mb;
	//phys_debug.Begin();
	PlayerMovement move;
	move.cmd = cmd;
	move.deltat = core.tick_interval;
	move.phys_debug = &mb;
	move.phys = &phys;
	move.fire_weapon = ServerGameShootCallback;
	move.play_sound = ServerPlaySoundCallback;
	move.set_viewmodel_animation = ServerViewmodelCallback;

	move.player = ent->ToPlayerState();
	move.entindex = GetEntIndex(ent);
	move.max_ground_speed = cfg.GetF("max_ground_speed");
	move.simtime = server.simtime;
	move.Run();

	ent->FromPlayerState(&move.player);

	server.simtime = oldtime;

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
	if (ent->death_time < server.simtime) {
		ent->health = 100;
		ent->alive = true;
		game.GetPlayerSpawnPoisiton(ent);
	}
}

void PlayerUpdate(Entity* ent)
{
	if (ent->alive)
		PlayerUpdateAnimations(ent);
	else
		PlayerDeathUpdate(ent);

	if (ent->position.y < -50 && ent->alive) {
		ent->alive = false;
		ent->death_time = server.simtime + 0.5f;
	}
}
void DummyUpdate(Entity* ent)
{
	//ent->position.y = sin(GetTime()) * 2.f + 2.f;
	ent->position.x = 0.f;
	if (!ent->alive)
		game.RemoveEntity(ent);
}

Entity* CreateGrenade(Entity* thrower, glm::vec3 org, glm::vec3 start_vel, int grenade_type)
{
	ASSERT(thrower);
	Game* g = &game;
	Entity* e = g->MakeNewEntity(Ent_Grenade);

	e->SetModel(Mod_Grenade_HE);
	e->owner_index = thrower->index;
	e->position = org;
	e->velocity = start_vel;
	e->sub_type = grenade_type;
	e->alive = true;
	e->death_time = server.simtime + 5.f;
	return e;
}

void RunProjectilePhysics(Entity* ent)
{
	Game* g = &game;
	// update physics, detonate if ready
	float dt = core.tick_interval;
	ent->velocity.y -= game.gravity * dt;// gravity
	glm::vec3 next_position = ent->position + ent->velocity * dt;
	float len = glm::length(ent->velocity * dt);
	RayHit rh;
	Ray r;
	r.dir = (ent->velocity * dt) / len;
	r.pos = ent->position;
	g->phys.TraceRay(r, &rh, ent->owner_index, Pf_All);
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
	Game* g = &game;
	RunProjectilePhysics(ent);
	// spin grenade based on velocity
	float dt = core.tick_interval;

	float vel = glm::length(ent->velocity);

	ent->rotation.x += 0.7 * dt * vel;
	ent->rotation.y -= 1.3 * dt * vel;

	if (ent->death_time < server.simtime) {
		printf("BOOM\n");
		g->RemoveEntity(ent);
	}
}

void PostEntUpdate(Entity* ent) {
	ent->anim.AdvanceFrame(core.tick_interval);
}

void Game::BuildPhysicsWorld(float time)
{
	phys.ClearObjs();
	phys.AddLevel(level);

	for (int i = 0; i < ents.size(); i++) {
		Entity& ce = ents[i];
		if (ce.type != Ent_Player) continue;

		CharacterShape cs;
		cs.a = &ce.anim;
		cs.m = ce.model;
		cs.org = ce.position;
		cs.radius = CHAR_HITBOX_RADIUS;
		cs.height = (!ce.ducking) ? CHAR_STANDING_HB_HEIGHT : CHAR_CROUCING_HB_HEIGHT;
		PhysicsObject po;
		po.shape = PhysicsObject::Character;
		po.character = cs;
		po.userindex = i;
		po.player = true;

		phys.AddObj(po);
	}
}


void Game::Update()
{
	BuildPhysicsWorld(0.f);

	double dt = core.tick_interval;
	for (int i = 0; i < ents.size(); i++) {
		Entity* e = &ents[i];
		if (e->type == Ent_Free)
			continue;

		switch (e->type) {
		case Ent_Player:
			PlayerUpdate(e);
			break;
		case Ent_Dummy:
			DummyUpdate(e);
			break;
		case Ent_Grenade:
			GrenadeUpdate(e);
			break;
		}
		PostEntUpdate(e);
	}
}

void Game::RemoveEntity(Entity* ent)
{
	ent->type = Ent_Free;
	ent->alive = false;
	ent->model = nullptr;
	ent->anim.Clear();

	num_ents--;
}

void Game::KillEnt(Entity* ent)
{
	ent->alive = false;
	ent->death_time = server.simtime + 5.0;
	ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_die"));
	ent->anim.dont_loop = true;
}
