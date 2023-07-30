#ifndef GAMETYPE_H
#define GAMETYPE_H
#include <SDL2/SDL.h>
#include "ClientNServer.h"
#include "Entity.h"
#include "Util.h"

const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;
const int MAX_GAME_ENTS = 256;

class FlyCamera
{
public:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	void UpdateFromInput(const bool keys[], int mouse_dx, int mouse_dy, int scroll);
	void UpdateVectors(); 
	glm::mat4 GetViewMatrix() const;
};

struct ViewSetup
{
	glm::vec3 vieworigin;
	glm::vec3 viewfront;
	float viewfov;
	glm::mat4 view_mat;
	glm::mat4 proj_mat;
	glm::mat4 viewproj;
	int x, y, width, height;
};

class ViewMgr
{
public:
	void Init();
	void Update();
	const ViewSetup& GetSceneView() {
		return setup;
	}

	bool third_person = true;
	bool debug_fly = false;

	float z_near = 0.01f;
	float z_far = 100.f;
	float fov = glm::radians(70.f);

	FlyCamera fly_cam;
	ViewSetup setup;
};

struct Entity
{
	EntType type = EntType::INVALID;
	bool active = false;
	glm::vec3 position = glm::vec3(0);
	glm::vec3 rotation = glm::vec3(0);
	glm::vec3 scale = glm::vec3(1.f);
	const Model* model = nullptr;
	Animator animator;
	Capsule collider;

	glm::vec3 velocity = glm::vec3(0);
};

class Game
{
public:
	Game() {
		memset(spawnids, 0, sizeof(spawnids));
	}
	int SpawnEnt(EntType type);
	Entity* GetByIndex(int index) {
		if (index >= MAX_GAME_ENTS || index < 0 || !ents[index].active)
			return nullptr;
		return &ents[index];
	}
	Entity* GetPlayer() {
		Entity* e = GetByIndex(0);
		ASSERT(!e || e->type == EntType::PLAYER);
		return e;
	}

	Entity ents[MAX_GAME_ENTS];
	short spawnids[MAX_GAME_ENTS];
	int num_ents = 0;
	int first_free_network = 0;
	int first_free_local = 0;

	Level* level = nullptr;
};

class Core
{
public:
	SDL_Window* window = nullptr;
	SDL_GLContext context = nullptr;
	int vid_width = DEFAULT_WIDTH;
	int vid_height = DEFAULT_HEIGHT;

	struct InputState
	{
		bool keyboard[SDL_NUM_SCANCODES];
		int mouse_delta_x = 0;
		int mouse_delta_y = 0;
		int scroll_delta = 0;
	};
	InputState input;
	Game game;
	ViewMgr view;
	Client client;
	Server server;
};
extern Core core;


#endif // !GAMETYPE_H
