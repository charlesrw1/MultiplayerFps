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
#include "Framework/StdVectorReflection.h"

#include "Physics/Physics2.h"
#include "External/ImGuizmo.h"

extern ConfigVar g_mousesens;

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
	std::string edtype;
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

class PhysicsActor;
class EditorDoc;
class EditorNode
{
public:
	EditorNode()  {}
	~EditorNode();

	virtual void hide();
	virtual void show();

	virtual EditorNode* duplicate();

	virtual void per_frame_tick() {}

	void init_on_new_espawn();

	const char* get_schema_name() const {
		return template_class ? template_class->name.c_str() : "unknown schema";
	}
	glm::mat4 get_transform();

	handle<Render_Object> render_handle;
	PhysicsActor* physics = nullptr;
	
	Dict& get_dict();

	glm::vec3 get_position() {
		return get_dict().get_vec3("position");
	}

	Color32 get_object_color() { return COLOR_WHITE; }
	const char* get_name() {
		return get_dict().get_string("name", "no_name");
	}
	void save_transform_to_dict(glm::vec3 v, glm::quat r, glm::vec3 s) {
		get_dict().set_vec3("position", v);
		get_dict().set_vec4("rotation", glm::vec4(r.w,r.x,r.y,r.z));
		get_dict().set_vec3("scale", s);
	}
	void read_transform_from_dict(glm::vec3& v, glm::quat& r, glm::vec3& s) {
		v = get_dict().get_vec3("position");
		glm::vec4 r_v = get_dict().get_vec4("rotation");
		r = glm::quat(r_v.x, r_v.y, r_v.z, r_v.w);
		s = get_dict().get_vec3("scale",glm::vec3(1));
	}
	void set_model(const std::string& name) {
		get_dict().set_string("model", name.c_str());
		model_is_dirty = true;
	}
	Model* get_rendering_model() {
		if (!model_is_dirty)
			return current_model;
		model_is_dirty = false;
		std::string s = get_dict().get_string("model");
		if (!s.empty()) {
			current_model = mods.find_or_load(s.c_str());
		}
		else if (!template_class)
			current_model = nullptr;
		else if (!template_class->edmodel.empty())
			current_model = mods.find_or_load(template_class->edmodel.c_str());
		else
			current_model = nullptr;
		return current_model;
	}
	uint32_t get_uid() {
		return get_dict().get_int("_editor_uid", 0);
	}
	void set_uid(uint32_t uid) {
		get_dict().set_int("_editor_uid", (int)uid);	// fixme
	}
	Color32 get_rendering_color() {
		return get_dict().get_color("color");
	}
	Material* get_sprite_material();

	// only write these when initially spawning like position/model, dont get queued in command system
	void write_dict_value_spawn();
	void write_dict_value(std::string key, std::string value);

	void init_from_schema(const ObjectSchema* t);

	void set_selected(bool selected) {
		is_selected = selected;
	}
private:
	Dict dictionary;
	std::vector<SignalProperty> signals;
	const ObjectSchema* template_class = nullptr;
	friend class EdPropertyGrid;
	bool is_selected = false;
	bool model_is_dirty = true;
	 Model* current_model = nullptr;
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
	virtual std::string to_string() = 0;
};

class UndoRedoSystem
{
public:
	UndoRedoSystem() {
		hist.resize(HIST_SIZE,nullptr);
	}
	void clear_all() {
		for (int i = 0; i < hist.size(); i++) {
			delete hist[i];
			hist[i] = nullptr;
		}
	}
	void add_command(Command* c) {
		if (hist[index]) {
			delete hist[index];
		}
		hist[index] = c;
		index += 1;
		index %= HIST_SIZE;

		sys_print("``` Executing: %s\n", c->to_string().c_str());

		c->execute();
	}
	void undo() {
		index -= 1;
		if (index < 0) index = HIST_SIZE - 1;
		if (hist[index]) {

			sys_print("``` Undoing: %s\n", hist[index]->to_string().c_str());


			hist[index]->undo();
			delete hist[index];
			hist[index] = nullptr;
		}
		else {
			sys_print("*** nothing to undo\n");
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
private:
	void draw_table_R( EditorNode* node, int depth);
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

	EditorNode* get_node() {
		return node;
	}
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

class OrthoCamera
{
public:
	glm::vec3 position = glm::vec3(0.0);
	float width = 10.0;
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	glm::vec3 side = glm::vec3(0, 0, 1);

	float far = 200.0;
	void set_position_and_front(glm::vec3 position, glm::vec3 front) {
		this->position = position;
		this->front = front;
		if (abs(dot(front, glm::vec3(0, 1, 0))) > 0.999) {
			up = glm::vec3(1, 0, 0);
		}
		else
			up = glm::vec3(0, 1, 0);
		side = cross(up, front);
	}

	void scroll_callback(int amt) {
		width += (width * 0.5) * amt;
		if (abs(width) < 0.000001)
			width = 0.0001;
	}
	void update_from_input(const bool keys[], int mouse_dx, int mouse_dy, float aspectratio) {
		position += side * (g_mousesens.get_float() * mouse_dx) * width;
		position += up * (g_mousesens.get_float() * mouse_dy) * width * aspectratio;
	}
	glm::mat4 get_view_matrix() const {
		return glm::lookAt(position, position+front, up);
	}
	glm::mat4 get_proj_matrix(float aspect_ratio) const {
		return glm::ortho(-width, width, -width*aspect_ratio, width * aspect_ratio,0.001f, 100.0f);
	}
};
using SharedNodeList = std::vector<std::shared_ptr<EditorNode>>;
class SelectionState
{
public:
	bool has_any_selected() const { return num_selected() > 0; }
	int num_selected() const { return selected_nodes.size(); }
	const SharedNodeList& get_selection() const { return selected_nodes; }
	void add_to_selection(const std::shared_ptr<EditorNode>& node) {
		if (get_index(node.get())==-1) {
			selected_nodes.push_back(node);
			selection_dirty = true;
		}
		node->set_selected(true);
	}
	void remove_from_selection(EditorNode* node) {
		int index = get_index(node);
		if (index != -1) {
			selected_nodes.erase(selected_nodes.begin() + index);
			selection_dirty = true;
		}
		node->set_selected(false);
	}
	void clear_all_selected() {
		for (int i = 0; i < selected_nodes.size(); i++) {
			selected_nodes[i]->set_selected(false);
			selected_nodes[i]->show();	// update the model
		}
		selected_nodes.clear();
		selection_dirty = true;
	}
	void set_select_only_this(const std::shared_ptr<EditorNode>& node) {
		clear_all_selected();
		add_to_selection(node);
	}
	bool is_selection_dirty() const { return selection_dirty; }
	void clear_selection_dirty() { selection_dirty = false; }
	bool is_node_selected(EditorNode* node) const {
		return get_index(node) != -1;
	}
private:
	int get_index(EditorNode* node) const {
		for (int i = 0; i < selected_nodes.size(); i++) {
			if (selected_nodes[i].get() == node)
				return i;
		}
		return -1;
	}

	bool selection_dirty = false;
	SharedNodeList selected_nodes;
};

class ManipulateTransformTool
{
public:
	void update();
	void handle_event(const SDL_Event& event);
	bool is_hovered();
	bool is_using();

	void free_refs() {
		saved_of_set.clear();
	}
private:
	void swap_mode() {
		if (mode == ImGuizmo::LOCAL)
			mode = ImGuizmo::WORLD;
		else
			mode = ImGuizmo::LOCAL;
	}
	enum StateEnum {
		IDLE,
		MANIPULATING_OBJS,
	}state = IDLE;

	ImGuizmo::OPERATION operation_mask = ImGuizmo::OPERATION::TRANSLATE;
	ImGuizmo::MODE mode = ImGuizmo::MODE::WORLD;
	glm::mat4 current_transform_of_group = glm::mat4(1.0);
	
	bool has_translation_snap = true;
	float translation_snap = 1.0;
	bool has_scale_snap = true;
	float scale_snap = 1.0;
	bool has_rotation_snap = true;
	float rotation_snap = 45.0;

	struct SavedTransform {
		glm::mat4 pretransform;
		std::shared_ptr<EditorNode> node;
	};
	std::vector<SavedTransform> saved_of_set;
};


// maps/
//	both prefabs and maps
//	build_<map_name>/
//		cubemaps,...

class Model;
class EditorDoc : public IEditorTool
{
public:
	EditorDoc() {}
	virtual void init();
	virtual bool can_save_document() override { return true; }
	virtual void open_document_internal(const char* levelname) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;
	virtual bool has_document_open() const override {
		return is_open;
	}
	virtual const char* get_editor_name()  override {
		return "Level Editor";
	}
	virtual void draw_menu_bar() override;

	virtual bool handle_event(const SDL_Event& event) override;
	virtual void ui_paint() override;
	virtual void tick(float dt) override;
	virtual void overlay_draw() override;
	virtual void imgui_draw() override;
	virtual const View_Setup& get_vs() override;
	virtual std::string get_save_root_dir() override { return "./Data/Maps/"; }

	std::string get_full_output_path() {
		return get_doc_name().empty() ? "Maps/<unnamed map>" : "Maps/" + get_doc_name();
	}


	void duplicate_selected_and_select_them();

	void remove_any_references(EditorNode* node);

	void hook_imgui_newframe() override {
		ImGuizmo::BeginFrame();
	}
	void hook_scene_viewport_draw() override;

	world_query_result cast_ray_into_world(Ray* out_ray, int mx, int my);

	
	enum ToolMode {
		TOOL_TRANSFORM,	// translation/rotation/scale tool
	}mode = TOOL_TRANSFORM;

	bool local_transform = false;
	TransformType transform_tool_type;
	int axis_bit_mask = 0;
	glm::vec3 transform_tool_origin;
	void transform_tool_update();
	void enter_transform_tool(TransformType type);
	void leave_transform_tool(bool apply_delta);

	SelectionState selection_state;

	// dont want to deal with patching up another index
	int get_node_index(EditorNode* node) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i].get() == node) return i;
		}
		ASSERT(0);
	}

	EditorNode* create_node_from_dict(const Dict& d);
	EditorNode* spawn_from_schema_type(const char* schema_name);

	bool is_open = false;
	UndoRedoSystem command_mgr;
	AssetBrowser assets;
	View_Setup vs_setup;
	EdPropertyGrid prop_editor;
	MapLoadFile editing_map;
	EditorSchemaManager ed_schema;
	ObjectOutliner outliner;
	ManipulateTransformTool manipulate;

	bool using_ortho = false;
	User_Camera camera;
	OrthoCamera ortho_camera;


	std::vector<std::shared_ptr<EditorNode>> nodes;
	std::vector<std::string> ent_files;

	// Inherited via IEditorTool
	virtual void on_change_focus(editor_focus_state newstate) override;
private:

	void hide_everything();
	void show_everything();

	uint32_t get_next_id() {
		return id_start++;
	}

	uint32_t id_start = 0;
};