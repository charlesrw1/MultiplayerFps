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
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"

#include "Physics/Physics2.h"
#include "External/ImGuizmo.h"
#include "AssetRegistry.h"
#include "Assets/AssetBrowser.h"

#include <functional>

template<typename... Args>
class MulticastDelegate
{
public:
	void add(void* key, std::function<void(Args...)> func)
	{
		functions_[key] = func;
	}
	void remove(void* key)
	{
		functions_.erase(key);
	}
	void invoke(Args... args) {
		for (const auto& pair : functions_)
		{
			pair.second(args...);
		}
	}
	template<typename T>
	void add(T* instance, void (T::* memberFunction)(Args...))
	{
		functions_[instance] = [instance, memberFunction](Args... args) {
			(instance->*memberFunction)(args...);
		};
	}
private:
	std::unordered_map<void*, std::function<void(Args...)>> functions_;
};

extern ConfigVar g_mousesens;



#include "Player.h"


class PhysicsActor;
class EditorDoc;
class EditorNode
{
public:
	EditorNode()  {
		player_ent.register_components();
	
	}
	~EditorNode() {
		player_ent.destroy();
		//hide();
	}

	void init_on_new_espawn();

	const char* get_schema_name() const {
		return  "unknown schema";
	}
	glm::mat4 get_transform();

	Player player_ent;

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


	//Color32 get_rendering_color() {
	//	return get_dict().get_color("color");
	//}
	//Material* get_sprite_material();

	// only write these when initially spawning like position/model, dont get queued in command system
	void write_dict_value_spawn();
	void write_dict_value(std::string key, std::string value);

	void set_selected(bool selected) {
		is_selected = selected;
	}
private:
	Dict dictionary;
	friend class EdPropertyGrid;
	bool is_selected = false;
};

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
	EdPropertyGrid();
	void draw();

	EditorNode* get_node() {
		return node;
	}
private:
	void on_selection_changed();
	void on_node_deleted(EditorNode* n);
	void on_close() {
		node = nullptr;
		selected_component = nullptr;
		grid.clear_all();
	}
	void refresh_grid();

	EditorNode* node = nullptr;
	EntityComponent* selected_component = nullptr;
	bool refresh_prop_flag = false;

	void on_select_component(EntityComponent* ec) {
		selected_component = ec;
		refresh_prop_flag = true;
	}
	void draw_components_R(EntityComponent* ec, float ofs);

	PropertyGrid grid;

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
	SelectionState();

	MulticastDelegate<EditorNode*> on_select_new_node;
	MulticastDelegate<> on_selection_changed;

	bool has_any_selected() const { return num_selected() > 0; }
	int num_selected() const { return selected_nodes.size(); }
	const SharedNodeList& get_selection() const { return selected_nodes; }
	void add_to_selection(const std::shared_ptr<EditorNode>& node) {
		if (get_index(node.get())==-1) {
			selected_nodes.push_back(node);

			on_selection_changed.invoke();
		}
		node->set_selected(true);
	}
	void remove_from_selection(EditorNode* node) {
		int index = get_index(node);
		if (index != -1) {
			selected_nodes.erase(selected_nodes.begin() + index);

			on_selection_changed.invoke();
		}
		node->set_selected(false);

	}
	void clear_all_selected(bool show_this = true) {
		for (int i = 0; i < selected_nodes.size(); i++) {
			selected_nodes[i]->set_selected(false);
			//if(show_this)
			//	selected_nodes[i]->show();	// update the model
		}
		selected_nodes.clear();

		on_selection_changed.invoke();
	}
	void set_select_only_this(const std::shared_ptr<EditorNode>& node) {
		clear_all_selected();
		add_to_selection(node);
	}

	bool is_node_selected(EditorNode* node) const {
		return get_index(node) != -1;
	}
private:
	void on_node_deleted(EditorNode* node) {
		remove_from_selection(node);
	}
	void on_close() {
		selected_nodes.clear();
	}

	int get_index(EditorNode* node) const {
		for (int i = 0; i < selected_nodes.size(); i++) {
			if (selected_nodes[i].get() == node)
				return i;
		}
		return -1;
	}
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
	EditorDoc() {
		selection_state = std::make_unique<SelectionState>();
		prop_editor = std::make_unique<EdPropertyGrid>();
	}
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


	// dont want to deal with patching up another index
	int get_node_index(EditorNode* node) {
		for (int i = 0; i < nodes.size(); i++) {
			if (nodes[i].get() == node) return i;
		}
		ASSERT(0);
	}

	bool is_open = false;
	UndoRedoSystem command_mgr;
	View_Setup vs_setup;
	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<EdPropertyGrid> prop_editor;
	MapLoadFile editing_map;

	ObjectOutliner outliner;
	ManipulateTransformTool manipulate;

	bool using_ortho = false;
	User_Camera camera;
	OrthoCamera ortho_camera;


	std::vector<std::shared_ptr<EditorNode>> nodes;
	std::vector<std::string> ent_files;

	MulticastDelegate<EditorNode*> on_node_deleted;
	MulticastDelegate<EditorNode*> on_node_created;
	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;

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