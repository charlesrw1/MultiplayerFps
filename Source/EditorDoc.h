#pragma once
#include "glm/glm.hpp"
#include "Model.h"
#include "Types.h"
#include "Level.h"
#include <SDL2/SDL.h>

enum EdObjType
{
	EDOBJ_MODEL,
	EDOBJ_DECAL,
	EDOBJ_LIGHT,
	EDOBJ_SOUND,
	EDOBJ_ENTITY,
	EDOBJ_PARTICLE,
};

class EditorNode
{
	virtual void draw() = 0;
	virtual void ui_tick() {
	}
	virtual void on_remove() = 0;
	virtual void on_create() = 0;

	EdObjType obj = EDOBJ_MODEL;
	int index = -1;


	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	
	bool use_sphere_collision = true;
	glm::vec3 collision_bounds = glm::vec3(0.5f);
	Texture* sprite = nullptr;
	Model* model = nullptr;

	// for blender nodes
	bool read_only = false;
};

// some considerations:
// raycasting
// can hook into physics world and add your own physics objects
// easy

// engine systems might be assuming we are playing the game (have a player spawned)
// can either fake a player or set a flag, setting a flag might be better to avoid weird behavior where its asusmed we are spawned

class EditorDoc
{
public:
	void open_doc(const char* levelname);
	void save_doc();
	void close_doc();

	void handle_event(const SDL_Event& event);
	void update();

	void scene_draw_callback();

	void draw_ui();

	enum ToolMode {
		NONE,

		SPAWN_OBJ,

		FOLIAGE_PAINT,

		TRANSLATION,
		ROTATION,
		SCALING,
	}mode = NONE;

	EditorNode* selected_node;



	Level* leveldoc = nullptr;
	Fly_Camera camera;
	std::vector<EditorNode*> nodes;
};