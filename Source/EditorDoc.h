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
	EDOBJ_GAMEOBJ,
	EDOBJ_PARTICLE,
};

class EditorNode
{
public:
	virtual void scene_draw() {}
	virtual void imgui_tick() {}
	virtual void on_remove() {}
	virtual void on_create() {}

	EdObjType obj = EDOBJ_MODEL;
	int index = -1;

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	
	bool use_sphere_collision = true;
	glm::vec3 collision_bounds = glm::vec3(0.5f);
	Texture* sprite = nullptr;
	Model* model = nullptr;

	// -1 for blender nodes
	int file_index = -1;
};

// some considerations:
// raycasting
// can hook into physics world and add your own physics objects
// easy

// engine systems might be assuming we are playing the game (have a player spawned)
// can either fake a player or set a flag, setting a flag might be better to avoid weird behavior where its asusmed we are spawned


class EditorDoc;
class AssetBrowser
{
public:
	AssetBrowser(EditorDoc* doc) : doc(doc) {}
	void init();
	void handle_input(const SDL_Event& event);
	void open(bool setkeyboardfocus);
	void close();
	void imgui_draw();
	void increment_index(int amt);
	void update_remap();
	Model* get_model();
	void clear_index() { selected_real_index = -1; }

	int selected_real_index = -1;
	bool set_keyboard_focus = false;	
	struct EdModel {
		std::string name = {};
		Model* m = nullptr;
		uint32_t thumbnail_tex = 0;
		bool havent_loaded = true;
	};
	char asset_name_filter[256];
	bool filter_match_case = false;

	std::vector<EdModel> edmodels;
	std::vector<int> remap;

	EditorDoc* doc;
};

class Model;
class EditorDoc
{
public:
	EditorDoc() : assets(this) {}
	void open_doc(const char* levelname);
	void save_doc();
	void close_doc();

	bool handle_event(const SDL_Event& event);
	void update();

	void scene_draw_callback();
	void overlays_draw();
	void imgui_draw();
	const View_Setup& get_vs();

	enum ToolMode {
		NONE,

		SPAWN_OBJ,

		FOLIAGE_PAINT,

		TRANSLATION,
		ROTATION,
		SCALING,
	}mode = NONE;

	EditorNode* selected_node;

	bool assets_open = false;
	AssetBrowser assets;

	Level* leveldoc = nullptr;
	Fly_Camera camera;
	std::vector<EditorNode*> nodes;
	std::vector<std::string> ent_files;
};