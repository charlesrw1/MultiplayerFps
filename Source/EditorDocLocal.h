#pragma once
#include "EditorDocPublic.h"

#include "glm/glm.hpp"
#include "Model.h"
#include "Types.h"
#include "Level.h"
#include <SDL2/SDL.h>
#include <memory>
#include "Physics.h"
#include "RenderObj.h"
#include <stdexcept>

#include "Framework/Factory.h"
#include "Framework/PropertyEd.h"
#include "Framework/ReflectionRegisterDefines.h"

class ConnectionList : public IArrayHeader
{
	using IArrayHeader::IArrayHeader;
	virtual bool imgui_draw_header(int index);
	virtual void imgui_draw_closed_body(int index);
	virtual bool has_delete_all() { return false; }
	virtual bool can_edit_array() { return true; }
};

class SchemaProperty
{
public:
	std::string type;
	std::string name;
	std::string hint;	// default values etc
	std::string tooltip;
	bool dont_expose = false;
};

struct string_and_tooltip
{
	std::string str;
	std::string tooltip;
};

class ObjectConnections
{
public:
	std::vector<string_and_tooltip> signals;
	std::vector<string_and_tooltip> events;
};

class EditorNode;
struct SignalProperty
{
	EditorNode* self = nullptr;	// hack
	std::string signal_name;
	std::string target_name;
	std::string event_name;
	std::string parameter_override;
	float delay = 0.0;
	bool fire_multiple_times = false;

	static PropertyInfoList* get_props() {
		START_PROPS(SignalProperty)
			REG_STDSTRING_CUSTOM_TYPE(signal_name,PROP_DEFAULT,"LevelEd_SignalName"),
			REG_STDSTRING_CUSTOM_TYPE(target_name, PROP_DEFAULT, "LevelEd_TargetName"),
			REG_STDSTRING_CUSTOM_TYPE(event_name, PROP_DEFAULT, "LevelEd_EventName"),
			REG_FLOAT(delay,PROP_DEFAULT,"0.0"),
			REG_BOOL(fire_multiple_times,PROP_DEFAULT,"0"),
			REG_STDSTRING(parameter_override, PROP_DEFAULT, ""),
		END_PROPS(SignalProperty)
	}
};

class ObjectSchema
{
public:
	std::string name;
	std::string tooltip;
	bool display_in_editor = true;
	std::vector<SchemaProperty> properties;
	ObjectConnections connections;

	std::string edimage;
	std::string edmodel;
	Color32 edcolor;

	SchemaProperty* find_property_name(const std::string& name) {
		for (int i = 0; i < properties.size(); i++) {
			if (properties[i].name == name)
				return &properties[i];
		}
		return nullptr;
	}

	void inherit_from(const ObjectSchema* other) {

		for (int i = 0; i < other->properties.size(); i++) {
			if (find_property_name(other->properties[i].name) != nullptr)
				throw std::runtime_error("tried inheriting property but already exists " + other->properties[i].name + " " + name + " " + other->name);
			properties.push_back(other->properties[i]);
		}
		for (int i = 0; i < other->connections.signals.size(); i++) {	
			connections.signals.push_back(other->connections.signals[i]);
		}
		for (int i = 0; i < other->connections.events.size(); i++) {
			connections.events.push_back(other->connections.events[i]);
		}
	}
};

class EditorSchemaManager
{
public:
	bool load(const char* file);
	const ObjectSchema* find_schema(const std::string& name) {
		auto find = all_schema_list.find(name);
		if (find != all_schema_list.end())
			return &find->second;
		return nullptr;
	}
	std::unordered_map<std::string,ObjectSchema> all_schema_list;
};

class EditorDoc;
class EditorNode
{
public:
	EditorNode(EditorDoc* doc) : doc(doc) {}
	~EditorNode();

	void hide();
	void show();

	virtual void scene_draw() {}
	virtual void imgui_tick() {}
	
	void init_on_new_espawn();

	void on_create_from_dict(int index, int varying_index, Dict* d);

	void on_transform_change() {}
	void on_dict_value_change() {}

	const char* get_schema_name() const {
		return template_class ? template_class->name.c_str() : "unknown schema";
	}

	uint64_t uid = 0;

	glm::vec3 position{};
	glm::quat rotation;
	glm::vec3 scale=glm::vec3(1.f);
	glm::mat4 get_transform();
	
	bool use_sphere_collision = true;
	glm::vec3 collision_bounds = glm::vec3(0.5f);
	Material* sprite_texture = nullptr;
	Model* model = nullptr;

	EditorDoc* doc;

	handle<Render_Object> render_handle;
	handle<Render_Light> render_light;
	handle<Render_Decal> render_decal;
	
	Dict& get_dict();

	Color32 get_object_color() { return COLOR_WHITE; }
	const char* get_name() {
		return get_dict().get_string("name", "no_name");
	}
	void save_transform_to_dict() {
		get_dict().set_vec3("position", position);
		glm::vec3 a = glm::eulerAngles(rotation);
		get_dict().set_vec3("rotation", a);
		get_dict().set_vec3("scale", scale);
	}
	void update_from_dict();


	void init_from_schema(const ObjectSchema* t) {
		template_class = t;
		dictionary = {};	// empty dict means deafult values
	}
private:
	bool node_is_hidden = false;
	Dict dictionary;
	std::vector<SignalProperty> signals;
	const ObjectSchema* template_class = nullptr;
	friend class EdPropertyGrid;
};

class DecalNode
{

};

class SplineNode
{

};

class RegionShape
{

};

// for triggers, defining regions, or editor geometry

class LightNode
{

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
	virtual ~Command() {}
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

enum class EdResType
{
	Model = 1,
	Sound = 2,
	Material = 4,
	AnimationTree = 8,
	Particle = 16,
	Schema = 32,
	Prefab = 64,
};

class EdResourceBase
{
public:
	// Model
	// Sound
	// Material
	// AnimationTree
	// Particle
	// Schema
	// Prefab
	virtual const char* get_type_name() const= 0;
	virtual std::string get_full_path() const { return name; }
	virtual std::string get_asset_name() const { return name; }
	virtual void check_for_update() { }
	virtual bool is_asset_out_of_date() const { return false; }
	virtual Texture* get_thumbnail() const { return nullptr; }
	virtual EdResType get_type() const = 0;
	virtual Color32 get_asset_color() const { return COLOR_WHITE; }
protected:
	std::string name;
};

class AssetBrowser
{
public:
	// indexes filesystem for resources
	void init();
	void update();
	void imgui_draw();

	//void increment_index(int amt);
	//void update_remap();
	//Model* get_model();
	//void clear_index() { selected_real_index = -1; }

	//int last_index = -1;
	//int selected_real_index = -1;
	//bool set_keyboard_focus = false;	
	//struct EdModel {
	//	std::string name = {};
	//	Model* m = nullptr;
	//	uint32_t thumbnail_tex = 0;
	//	bool havent_loaded = true;
	//};
	//char asset_name_filter[256];
	//bool filter_match_case = false;
	//
	//bool drawing_model = false;
	//glm::vec3 model_position = glm::vec3(0.f);
	//std::vector<EdModel> edmodels;
	//std::vector<int> remap;

	void clear_filter() {
		filter_type_mask = -1;
	}
	void filter_all() {
		filter_type_mask = 0;
	}
	void unset_filter(EdResType type) {
		filter_type_mask |= (uint32_t)type;
	}
	void set_filter(EdResType type) {
		filter_type_mask &= ~((uint32_t)type);
	}
	bool should_type_show(EdResType type) const {
		return filter_type_mask & (uint32_t)type;
	}

	EdResourceBase* search_for_file(const std::string& filepath);

	enum class Mode
	{
		Rows,
		Grid,
	};

	Mode mode = Mode::Grid;
	char asset_name_filter[256];
	bool filter_match_case = false;
	uint32_t filter_type_mask = -1;

	EdResourceBase* selected_resoruce = nullptr;
	// indexes everything

	std::unordered_map<std::string, EdResourceBase*> path_to_resource;
	std::vector<unique_ptr<EdResourceBase>> all_resources;

	void append_new_resource(EdResourceBase* r) {
		path_to_resource.insert({ r->get_full_path(),r });
		all_resources.push_back(std::unique_ptr<EdResourceBase>(r));
	}
};

class ObjectOutliner
{
public:
	void draw();
	void free_reference(const EditorNode* node) {
		if (selected == node) {
			selected = nullptr;
		}
	}
	void set_selected(EditorNode* node) {
		selected = node;
	}
	EditorNode* get_selected() {
		return selected;
	}
private:
	void draw_table_R( EditorNode* node, int depth);

	EditorNode* selected = nullptr;
};

class ObjectPlacerTool
{
public:
	void draw();
};

class DragAndDropAsset
{
public:
};
#include "Framework/StdVectorReflection.h"
class EdPropertyGrid
{
public:
	EdPropertyGrid() : callback_for_signals(SignalProperty::get_props()) {}
	void set(EditorNode* node);
	void clear() {
		set(nullptr);
	}
	void refresh_props() {
		EditorNode* n = node;
		set(nullptr);
		set(n);
	}
	void draw();
private:
	// currently editing this
	EditorNode* node = nullptr;
	struct display_grid {
		PropertyInfoList list;
		std::vector<PropertyInfo> props;
	};
	display_grid props_from_node;
	display_grid connection_props_from_node;
	// regular properties
	PropertyGrid grid;
	// signals/events
	PropertyGrid connection_grid;
	StdVectorCallback<SignalProperty> callback_for_signals;

};

class Model;
class EditorDoc : public IEditorTool
{
public:
	EditorDoc() {}
	virtual void init() {}
	virtual void open(const char* levelname) override;
	virtual void close() override;
	virtual bool handle_event(const SDL_Event& event) override;
	virtual void tick(float dt) override;
	virtual void overlay_draw() override;
	virtual void imgui_draw() override;
	virtual void draw_frame() override;
	virtual const View_Setup& get_vs() override;
	virtual const char* get_name() override {
		return "";
	}

	const char* get_full_output_path() {
		return "temp/path/to/map";
	}

	RayHit cast_ray_into_world(Ray* out_ray);

	void save_doc();
	
	enum ToolMode {
		TOOL_NONE,	// can select objects in viewport
		TOOL_SPAWN_MODEL,	// clicking spawns selected model
		TOOL_SPAWN_OBJ,
		TOOL_TRANSFORM,	// translation/rotation/scale tool
	}mode = TOOL_NONE;

	bool local_transform = false;
	TransformType transform_tool_type;
	int axis_bit_mask = 0;
	glm::vec3 transform_tool_origin;
	void transform_tool_update();
	void enter_transform_tool(TransformType type);
	void leave_transform_tool(bool apply_delta);

	std::shared_ptr<EditorNode> selected_node;

	// dont want to deal with patching up another index
	int get_node_index(EditorNode* node) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i].get() == node) return i;
		}
		ASSERT(0);
	}

	EditorNode* create_node_from_dict(const Dict& d);
	EditorNode* spawn_from_schema_type(const char* schema_name);

	UndoRedoSystem command_mgr;
	AssetBrowser assets;
	View_Setup vs_setup;
	EdPropertyGrid prop_editor;
	MapLoadFile editing_map;
	EditorSchemaManager ed_schema;
	ObjectOutliner outliner;

	User_Camera camera;
	std::vector<std::shared_ptr<EditorNode>> nodes;
	std::vector<std::string> ent_files;

	// Inherited via IEditorTool
	virtual void on_change_focus(editor_focus_state newstate) override;
private:
	uint64_t id_start = 0;
};