#include "Server.h"
#include "CoreTypes.h"
#include "Level.h"


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
	if (ent->model) {
		int idle = ent->model->animations->FindClipFromName("act_idle");
	}
	//server.sv_game.GetPlayerSpawnPoisiton(ent);
	ent->ducking = false;
}

void DummySpawn(Entity* ent)
{

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
	return true;
}