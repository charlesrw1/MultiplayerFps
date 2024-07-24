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
#include "Framework/MulticastDelegate.h"

#include "GameEngineLocal.h"	// has local access to internals

extern ConfigVar g_mousesens;

class Command
{
public:
	virtual ~Command() {}
	virtual void execute() = 0;
	virtual void undo() = 0;
	virtual std::string to_string() = 0;
	virtual bool is_valid() { return true; }
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

		if (!c->is_valid()) {
			sys_print("??? command not valid %s\n", c->to_string().c_str());
			delete c;
			return;
		}

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
	void draw_table_R(uint64_t handle, int depth);
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

private:
	void on_selection_changed() {
		refresh_grid();
	}
	void on_node_deleted(uint64_t n) {
		refresh_grid();
	}
	void on_ec_deleted(EntityComponent* ec) {
		refresh_grid();
	}
	void on_close() {
		grid.clear_all();
	}
	void refresh_grid();

	bool refresh_prop_flag = false;

	void on_select_component(EntityComponent* ec) {
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

struct WorldSelection
{
	EntityPtr<Entity> ent;
	EntityComponent* component = nullptr;
};

class SelectionState
{
public:
	SelectionState();

	MulticastDelegate<> on_selection_changed;

	// set only selected
	// clear selected
	// unselect
	// add to selected
	// get selected


	bool has_any_selected() const {
		return ec != nullptr || ptrs.size() > 0;
	}
	bool is_selecting_entity_component() const {
		return ec != nullptr;
	}
	int num_entities_selected() const {
		return ptrs.size();
	}

	const std::vector<EntityPtr<Entity>>& get_selection() const { return ptrs; }
	EntityComponent* get_ec_selected() const { return ec; }

	void set_entity_component_select(EntityComponent* ec) {
		ptrs.clear();
		this->ec = ec;
		on_selection_changed.invoke();
	}
	void unselect_ent_component() {
		ec = nullptr;
		on_selection_changed.invoke();
	}

	void add_to_selection(EntityPtr<Entity> node) {
		ec = nullptr;
		bool already_selected = is_node_selected(node);
		if (!already_selected) {
			ptrs.push_back(node);
			on_selection_changed.invoke();
		}
	}
	void remove_from_selection(EntityPtr<Entity> node) {
		ec = nullptr;
		for(int i=0;i<ptrs.size();i++)
			if (ptrs[i].handle == node.handle) {
				ptrs.erase(ptrs.begin() + i);
				i--;
				on_selection_changed.invoke();
			}
	}
	void clear_all_selected(bool show_this = true) {
		ptrs.clear();
		ec = nullptr;
		on_selection_changed.invoke();
	}
	void set_select_only_this(EntityPtr<Entity> node) {
		clear_all_selected();
		add_to_selection(node);
	}

	bool is_node_selected(EntityPtr<Entity> node) const {
		for (int i = 0; i < ptrs.size(); i++)
			if (ptrs[i].handle == node.handle)
				return true;
		return false;
	}
private:
	void on_node_deleted(uint64_t node) {
		remove_from_selection({ node });
	}
	void on_entity_component_delete(EntityComponent* ec) {
		if (ec == this->ec)
			unselect_ent_component();
	}

	void on_close() {
		ptrs.clear();
		ec = nullptr;
		on_selection_changed.invoke();
	}

	EntityComponent* ec = nullptr;
	std::vector<EntityPtr<Entity>> ptrs;
};

class ManipulateTransformTool
{
public:
	ManipulateTransformTool();
	void update();
	void handle_event(const SDL_Event& event);
	bool is_hovered();
	bool is_using();

	void free_refs() {
		saved_of_set.clear();
	}
private:

	void on_component_deleted(EntityComponent* ec) {

	}
	void on_entity_deleted(uint64_t handle) {
	
	}
	void on_selection_changed() {

	}

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
		EntityComponent* ec = nullptr;
		uint64_t handle = 0;
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
		manipulate = std::make_unique<ManipulateTransformTool>();
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


	// schema vs level editing
	bool is_editing_a_schema = false;


	bool local_transform = false;
	TransformType transform_tool_type;
	int axis_bit_mask = 0;
	glm::vec3 transform_tool_origin;
	void transform_tool_update();
	void enter_transform_tool(TransformType type);
	void leave_transform_tool(bool apply_delta);

	bool is_open = false;
	UndoRedoSystem command_mgr;
	View_Setup vs_setup;
	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<EdPropertyGrid> prop_editor;
	std::unique_ptr<ManipulateTransformTool> manipulate;
	ObjectOutliner outliner;

	bool using_ortho = false;
	User_Camera camera;
	OrthoCamera ortho_camera;

	MulticastDelegate<EntityComponent*> on_component_deleted;
	MulticastDelegate<EntityComponent*> on_component_created;
	MulticastDelegate<uint64_t> on_node_deleted;
	MulticastDelegate<uint64_t> on_node_created;
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