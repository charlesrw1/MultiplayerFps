#pragma once
#include "IEditorTool.h"

#include "glm/glm.hpp"
#include "Render/Model.h"
#include "Types.h"
#include "Level.h"
#include <SDL2/SDL.h>
#include <memory>

#include "Render/RenderObj.h"
#include <stdexcept>

#include "Framework/Factory.h"
#include "Framework/PropertyEd.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"

#include "Physics/Physics2.h"
#include "External/ImGuizmo.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetBrowser.h"
#include "Framework/MulticastDelegate.h"

#include "GameEngineLocal.h"	// has local access to internals

#include "AssetCompile/Someutils.h"
#include <algorithm>
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
	UndoRedoSystem();

	void on_key_event(const SDL_KeyboardEvent& k);

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
	ObjectOutliner();
	void draw();
private:

	void rebuild_tree() {
		delete_tree();

		rootnode = new Node;

		auto level = eng->get_level();
		for (auto ent : level->all_world_ents) {
			auto rootc = ent->get_root_component();
			if (!rootc->get_parent_component()) {
				Node* me = new Node(this, ent);
				rootnode->add_child(me);
			}
		}
		rootnode->sort_children();

	}
	void delete_tree() {
		delete rootnode;
		rootnode = nullptr;
		map.clear();
	}

	void on_changed_ents() { 
		rebuild_tree();
	}
	void on_start() { rebuild_tree(); }
	void on_close() { delete_tree(); }
	void on_change_name(uint64_t handle) {
		if (map.find(handle) == map.end()) {
			sys_print("??? on_change_name couldnt find handle??\n");
			return;
		}

		map.find(handle)->second->parent->sort_children();
	}

	struct Node {
		Node() {}
		Node(ObjectOutliner* oo, Entity* initfrom) {
			handle = initfrom->self_id.handle;
			for (int i = 0; i < initfrom->get_all_components().size(); i++) {
				auto c = initfrom->get_all_components()[i].get();
				if (c->get_owner() != initfrom && c->get_owner() != nullptr)
				{
					Node* other = new Node(oo, c->get_owner());
					add_child(other);
				}
			}
			sort_children();
			oo->map.insert({ handle, this });
		}

		void add_child(Node* other) {
			other->parent = this;
			children.push_back(other);
		}

		~Node() {
			for (int i = 0; i < children.size(); i++)
				delete children[i];
		}
		uint64_t handle = 0;
		Node* parent = nullptr;
		std::vector<Node*> children;
		void sort_children() {
			std::sort(children.begin(), children.end(), [](const Node* a, const Node* b)->bool {
				return to_lower(eng->get_entity(a->handle)->editor_name) < to_lower(eng->get_entity(b->handle)->editor_name);
				});
		}
	};
	std::unordered_map<uint64_t, Node*> map;
	Node* rootnode = nullptr;

	void draw_table_R(Node* n, int depth);

	uint64_t contextMenuHandle = 0;

	char nameFilter[256];
};

class EdPropertyGrid
{
public:
	EdPropertyGrid();
	void draw();
	
	MulticastDelegate<> on_property_change;
private:
	void on_ec_deleted(EntityComponent* ec) {
		refresh_grid();
	}
	void on_close() {
		grid.clear_all();
	}
	void refresh_grid();

	bool wants_set_component = false;
	EntityComponent* set_this_component = nullptr;

	void on_select_component(EntityComponent* ec) {
		set_this_component = ec;
		wants_set_component = true;
	}
	void draw_components_R(EntityComponent* ec, float ofs);

	EntityComponent* dragging_component = nullptr;

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
			node.get()->selectedInEditor = true;
			node.get()->set_ws_transform(node.get()->get_ws_transform());

			ptrs.push_back(node);
			on_selection_changed.invoke();
		}
	}
	void remove_from_selection(EntityPtr<Entity> node) {
		ec = nullptr;
		node.get()->selectedInEditor = false;
		node.get()->set_ws_transform(node.get()->get_ws_transform());

		for(int i=0;i<ptrs.size();i++)
			if (ptrs[i].handle == node.handle) {
				ptrs.erase(ptrs.begin() + i);
				i--;
				on_selection_changed.invoke();
			}
	}
	void clear_all_selected(bool show_this = true) {
		for (int i = 0; i < ptrs.size(); i++)
			if (ptrs[i].get()) {
				ptrs[i].get()->selectedInEditor = false;
				ptrs[i].get()->set_ws_transform(ptrs[i].get()->get_ws_transform());
			}
		ptrs.clear();
		if (ec) {
			ec->get_owner()->selectedInEditor = false;
			ec->get_owner()->set_ws_transform(ec->get_owner()->get_ws_transform());
		}
		ec = nullptr;
		on_selection_changed.invoke();
	}
	void set_select_only_this(EntityPtr<Entity> node) {
		clear_all_selected();
		add_to_selection(node);
	}
	void set_select_only_this(EntityComponent* ec) {
		clear_all_selected();
		this->ec = ec;
		on_selection_changed.invoke();
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
	bool is_hovered();
	bool is_using();

private:

	void on_key_down(const SDL_KeyboardEvent& k);

	void on_close();
	void on_open();
	void on_component_deleted(EntityComponent* ec);
	void on_entity_changes();
	void on_selection_changed();
	void on_selected_tarnsform_change(uint64_t);

	void update_pivot_and_cached();

	void begin_drag();
	void end_drag();

	void swap_mode() {
		if (mode == ImGuizmo::LOCAL)
			mode = ImGuizmo::WORLD;
		else
			mode = ImGuizmo::LOCAL;
	}
	enum StateEnum {
		IDLE,
		SELECTED,
		MANIPULATING_OBJS,
	}state = IDLE;

	ImGuizmo::OPERATION operation_mask = ImGuizmo::OPERATION::TRANSLATE;
	ImGuizmo::MODE mode = ImGuizmo::MODE::WORLD;

	std::vector<glm::mat4> world_space_of_selected; // pre transform, ie transform of them is 
	glm::mat4 current_transform_of_group = glm::mat4(1.0);
	glm::mat4 pivot_transform = glm::mat4(1.f);
	
	bool has_translation_snap = false;
	float translation_snap = 0.2;
	bool has_scale_snap = false;
	float scale_snap = 1.0;
	bool has_rotation_snap = false;
	float rotation_snap = 15.0;
};

// maps/
//	both prefabs and maps
//	build_<map_name>/
//		cubemaps,...
#include <unordered_map>
#include <unordered_set>
class EntityNameDatabase_Ed
{
public:
	std::string find_unique_name_for_name(bool& different_name, std::string wantname) {
		different_name = false;
		if (wantname.empty()) {
			different_name = true;
			wantname = "_";
		}
		std::string search_name = wantname;
		int integer = 2;
		while (all_names.find(search_name) != all_names.end()) {
			search_name = wantname + std::to_string(integer++);
			different_name = true;
		}
		return search_name;
	}

	EntityNameDatabase_Ed();
	std::unordered_set<std::string> all_names;
	std::unordered_map<uint64_t, std::string> id_to_name;

	void index_all_names() {
		for (auto ent : eng->get_level()->all_world_ents) {
			bool changed_name = false;
			ent->editor_name = find_unique_name_for_name(changed_name, ent->editor_name);
			all_names.insert(ent->editor_name);
			id_to_name.insert({ ent->self_id.handle, ent->editor_name });
			if(changed_name)
				invoke_change_name(ent->self_id.handle);
		}
	}

	void on_start() {
		index_all_names();
	}
	void on_close() {
		all_names.clear();
		id_to_name.clear();
	}
	void on_delete(uint64_t h) {
		all_names.erase(id_to_name.find(h)->second);
	}
	void on_add(uint64_t h) {
		auto ent = eng->get_entity(h);
		bool changed_name = false;
		ent->editor_name = find_unique_name_for_name(changed_name, ent->editor_name);
		all_names.insert(ent->editor_name);
		id_to_name.insert({ ent->self_id.handle, ent->editor_name });
		if(changed_name)
			invoke_change_name(ent->self_id.handle);
	}
	void invoke_change_name(uint64_t h);

	void on_property_change() {}
};

class EditorUILayout;
class Model;
class EditorDoc : public IEditorTool
{
public:
	EditorDoc();
	virtual void init();
	virtual bool can_save_document() override { return true; }
	virtual bool open_document_internal(const char* levelname, const char* arg) override;
	virtual void close_internal() override;
	virtual bool save_document_internal() override;

	const ClassTypeInfo& get_asset_type_info() const override {
		return Level::StaticType;
	}

	virtual void tick(float dt) override;
	virtual void imgui_draw() override;
	virtual const View_Setup* get_vs() override { return &vs_setup; }

	std::string get_full_output_path() {
		return get_doc_name().empty() ? "Maps/<unnamed map>" : "Maps/" + get_doc_name();
	}

	void on_mouse_down(int x, int y, int button);
	void on_key_down(const SDL_KeyboardEvent& k);
	void on_mouse_wheel(const SDL_MouseWheelEvent& wheel) {
		if (!eng->is_game_focused())
			return;
		if (using_ortho)
			ortho_camera.scroll_callback(wheel.y);
		else
			camera.scroll_callback(wheel.y);
	}
	void on_map_load_return(bool good);

	void duplicate_selected_and_select_them();

	void hook_imgui_newframe() override {
		ImGuizmo::BeginFrame();
	}
	void hook_scene_viewport_draw() override;

	glm::vec3 unproject_mouse_to_ray(int mx, int my);

	
	enum ToolMode {
		TOOL_TRANSFORM,	// translation/rotation/scale tool
	}mode = TOOL_TRANSFORM;


	// schema vs level editing
	bool is_editing_a_schema = false;
	const Schema* schema_source = nullptr;

	bool local_transform = false;
	TransformType transform_tool_type;
	int axis_bit_mask = 0;
	glm::vec3 transform_tool_origin;
	void transform_tool_update();
	void enter_transform_tool(TransformType type);
	void leave_transform_tool(bool apply_delta);

	std::unique_ptr<UndoRedoSystem> command_mgr;
	View_Setup vs_setup;
	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<EdPropertyGrid> prop_editor;
	std::unique_ptr<ManipulateTransformTool> manipulate;
	std::unique_ptr<ObjectOutliner> outliner;
	std::unique_ptr<EntityNameDatabase_Ed> database;
	std::unique_ptr<EditorUILayout> gui;

	bool using_ortho = false;
	User_Camera camera;
	OrthoCamera ortho_camera;


	MulticastDelegate<EntityComponent*> on_component_deleted;
	MulticastDelegate<EntityComponent*> on_component_created;
	MulticastDelegate<uint64_t> on_node_will_delete;	// called before its deleted
	MulticastDelegate<uint64_t> on_node_created;	// after creation
	MulticastDelegate<> post_node_changes;	// called after any nodes are deleted/created

	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
	MulticastDelegate<uint64_t> on_change_name;

	// Inherited via IEditorTool
private:

	void on_mouse_drag(int x, int y);

	uint32_t get_next_id() {
		return id_start++;
	}

	uint32_t id_start = 0;
};