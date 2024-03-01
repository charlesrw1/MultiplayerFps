#pragma once
#include "glm/glm.hpp"
#include "Model.h"
#include "Types.h"
#include "Level.h"
#include <SDL2/SDL.h>
#include <memory>

enum EdObjType
{
	EDOBJ_EMPTY,
	EDOBJ_MODEL,
	EDOBJ_DECAL,
	EDOBJ_LIGHT,
	EDOBJ_SOUND,
	EDOBJ_GAMEOBJ,
	EDOBJ_PARTICLE,
	EDOBJ_CUBEMAPS,
};
class EditorDoc;
class EditorNode
{
public:
	EditorNode(EditorDoc* doc) : doc(doc) {}

	void make_from_existing(int index);

	virtual void scene_draw() {}
	virtual void imgui_tick() {}
	virtual void on_remove();
	virtual void on_create();

	void save_out_to_level();

	void on_transform_change() {}
	void on_dict_value_change() {}

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	
	bool use_sphere_collision = true;
	glm::vec3 collision_bounds = glm::vec3(0.5f);
	Texture* sprite = nullptr;
	Model* model = nullptr;

	EditorDoc* doc;

	EdObjType obj = EDOBJ_EMPTY;
	int dict_index = -1;
	Dict entity_dict;
	int _varying_obj_index = -1;
	
	const char* get_name() {
		return entity_dict.get_string("name", "no_name");
	}
};

// some considerations:
// raycasting
// can hook into physics world and add your own physics objects
// easy

// engine systems might be assuming we are playing the game (have a player spawned)
// can either fake a player or set a flag, setting a flag might be better to avoid weird behavior where its asusmed we are spawned

class Command
{
public:
	virtual ~Command() = 0;
	virtual void execute() = 0;
	virtual void undo() = 0;
};

class UndoRedoSystem
{
public:
	UndoRedoSystem() {
		hist.resize(HIST_SIZE);
	}
	void add_command(Command* c) {
		if (hist[index]) {
			delete hist[index];
		}
		hist[index] = c;
		index += 1;
		index %= HIST_SIZE;

		c->execute();
	}
	void undo() {
		index -= 1;
		if (index < 0) index = HIST_SIZE - 1;
		if (hist[index]) {
			hist[index]->undo();
			delete hist[index];
			hist[index] = nullptr;
		}
		else {
			sys_print("nothing to undo\n");
		}
	}

	const int HIST_SIZE = 128;
	int index = 0;
	std::vector<Command*> hist;
};

enum TransformType
{
	TRANSLATION,
	ROTATION,
	SCALE
};

class EditorDoc;
class AssetBrowser
{
public:
	AssetBrowser(EditorDoc* doc) : doc(doc) {}
	void init();
	void handle_input(const SDL_Event& event);
	void open(bool setkeyboardfocus);
	void close();
	void update();
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

	bool drawing_model = false;
	glm::vec3 model_position = glm::vec3(0.f);
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

	void on_add_or_remove_node(int ent_dict_index, EdObjType type, int varying_index, bool is_removal);

	enum ToolMode {
		NONE,

		SPAWN_OBJ,

		FOLIAGE_PAINT,

		TRANSLATION,
		ROTATION,
		SCALING,
	}mode = NONE;

	EditorNode* selected_node;
	
	UndoRedoSystem command_mgr;
	bool assets_open = false;
	AssetBrowser assets;
	View_Setup vs_setup;
	Level* leveldoc = nullptr;
	Fly_Camera camera;
	std::vector<std::shared_ptr<EditorNode>> nodes;
	std::vector<std::string> ent_files;
};