#include "Server.h"
#include "CoreTypes.h"
#include "Level.h"
#include "Movement.h"

Entity* ServerEntForIndex(int index)
{
	ASSERT(index >= 0 && index < MAX_GAME_ENTS);
	return &server.sv_game.ents[index];
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

void PlayerSpawn(Entity* ent)
{
	ASSERT(ent->type == Ent_Player);
	ent->model = FindOrLoadModel("CT.glb");
	ent->anim.Init(ent->model);
	ent->anim.ResetLayers();
	if (ent->model) {
		int idle = ent->model->animations->FindClipFromName("act_idle");
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->ducking = false;
}

void DummySpawn(Entity* ent)
{
	ent->model = FindOrLoadModel("CT.glb");
	ent->anim.Init(ent->model);
	if (ent->model)
		ent->anim.SetLegAnim(ent->model->animations->FindClipFromName("act_run"),1.f);
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
	ent->type = Ent_Free;
	num_ents--;
	printf("remove client %d from game\n", client);
}

int Game::MakeNewEntity(EntType type, glm::vec3 pos, glm::vec3 rot)
{
	if (type == Ent_Player)
		return -1;
	int slot = MAX_CLIENTS;
	for (; slot < MAX_GAME_ENTS; slot++) {
		if (ents[slot].type == Ent_Free)
			break;
	}
	if (slot == MAX_GAME_ENTS)
		return -1;
	Entity* ent = InitNewEnt(type, slot);
	num_ents++;
	printf("spawning ent in slot %d\n", slot);

	switch (type)
	{
	case Ent_Dummy:
		DummySpawn(ent);
		break;
	}
	return slot;
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
	FreeLevel(level);
	level = nullptr;
}
bool Game::DoNewMap(const char* mapname)
{
	if (level) {
		ClearState();
	}
	level = LoadLevelFile(mapname);
	if (!level)
		Fatalf("level not loaded\n");

	MakeNewEntity(Ent_Dummy, glm::vec3(0.f), glm::vec3(0.f));

	return true;
}

void Game::ShootBullets(Entity* from, glm::vec3 dir, glm::vec3 org)
{
	printf("Shooting bullets\n");
	Ray r;
	r.dir = dir;
	r.pos = org;
	RayHit hit;
	TraceRayAgainstLevel(level, r, &hit, false);
	if (hit.dist >= 0.f)
		rays.PushLine(org, hit.pos, COLOR_WHITE);
}

void Game::RayWorldIntersect(Ray r, RayHit* out, int skipent, bool noents)
{
	


}


void PlayerItemUpdate(Entity* ent, MoveCommand cmd)
{
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
	
	if (ent->reloading || ent->gun_timer >= server.simtime)
		return;

	if (wants_shoot){// && ent->clip[ent->gun_id] > 0) {
		server.sv_game.ShootBullets(ent, 
			AnglesToVector(cmd.view_angles.x, cmd.view_angles.y), 
			ent->position+glm::vec3(0,STANDING_EYE_OFFSET,0));
		ent->gun_timer = server.simtime + 0.1f;
		ent->clip[ent->gun_id]--;
	}
	else if ((ent->clip[ent->gun_id] <= 0 || wants_reload) && ent->ammo[ent->gun_id] > 0) {
		ent->reloading = true;
		ent->gun_timer += 1.0f;
	}
}


void Server_TraceCallback(GeomContact* out, PhysContainer obj, bool closest, bool double_sided)
{
	TraceAgainstLevel(server.sv_game.level, out, obj, closest, double_sided);
}


PlayerState Entity::ToPlayerState() const
{
	PlayerState ps{};
	ps.position = position;
	ps.angles = view_angles;
	ps.ducking = ducking;
	ps.on_ground = on_ground;
	ps.velocity = velocity;

	return ps;
}
void Entity::FromPlayerState(PlayerState* ps)
{
	position = ps->position;
	rotation = ps->angles;
	ducking = ps->ducking;
	on_ground = ps->on_ground;
	velocity = ps->velocity;
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

	return es;
}
#include "MeshBuilder.h"
void Game::ExecutePlayerMove(Entity* ent, MoveCommand cmd)
{
	MeshBuilder mb;
	//phys_debug.Begin();
	PlayerMovement move;
	move.cmd = cmd;
	move.deltat = core.tick_interval;
	move.phys_debug = &mb;
	move.trace_callback = Server_TraceCallback;
	move.in_state = ent->ToPlayerState();
	move.Run();
	ent->FromPlayerState(move.GetOutputState());

	double oldtime = server.simtime;
	server.simtime = cmd.tick * core.tick_interval;
	PlayerItemUpdate(ent, cmd);
	server.simtime = oldtime;
	//phys_debug.End();

}