#include "Client.h"
#include "Util.h"
#include "CoreTypes.h"

bool Client::IsInGame() const
{
	return GetConState() == Spawned;
}

ClientEntity* Client::GetLocalPlayer()
{
	ASSERT(IsInGame());
	int index = GetPlayerNum();
	return &cl_game.entities[index];
}

void ClientGame::Init()
{
	entities.resize(MAX_GAME_ENTS);
	level = nullptr;
}
void ClientGame::ClearState()
{
	for (int i = 0; i < MAX_GAME_ENTS; i++)
		entities[i].active = false;
	FreeLevel(level);
	level = nullptr;
}
void ClientGame::NewMap(const char* mapname)
{
	if (level) {
		printf("Client: freeing level\n");
		FreeLevel(level);
		level = nullptr;
	}
	level = LoadLevelFile(mapname);
}