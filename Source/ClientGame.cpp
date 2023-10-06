#include "Client.h"
#include "Util.h"
#include "CoreTypes.h"
#include "GlmInclude.h"
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
	for (int i = 0; i < MAX_GAME_ENTS; i++) {
		entities[i].active = false;
		entities[i].ClearState();
	}
	if(level)
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

void ClientGame::UpdateViewModelOffsets()
{
	PlayerState* lastp = &client.lastpredicted;
	glm::vec3 view_front = AnglesToVector(client.view_angles.x, client.view_angles.y);
	view_front.y = 0;
	glm::vec3 side_grnd = glm::normalize(glm::cross(view_front, vec3(0, 1, 0)));
	float spd_side = dot(side_grnd, lastp->velocity);
	float side_ofs_ideal = -spd_side / 200.f;
	glm::clamp(side_ofs_ideal, -0.005f, 0.005f);
	float spd_front = dot(view_front, lastp->velocity);
	float front_ofs_ideal = spd_front / 200.f;
	glm::clamp(front_ofs_ideal, -0.007f, 0.007f);
	float up_spd = lastp->velocity.y;
	float up_ofs_ideal = -up_spd / 200.f;
	glm::clamp(up_ofs_ideal, -0.007f, 0.007f);

	if (lastp->ducking)
		up_ofs_ideal += 0.04;

	viewmodel_offsets = glm::mix(viewmodel_offsets, vec3(side_ofs_ideal, up_ofs_ideal, front_ofs_ideal), 0.4f);
}