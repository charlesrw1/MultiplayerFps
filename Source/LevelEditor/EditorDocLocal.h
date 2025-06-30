#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "CommandMgr.h"
#include "glm/glm.hpp"
#include "Render/Model.h"
#include "Types.h"
#include "Level.h"
#include <SDL2/SDL.h>
#include <memory>
#include "Render/RenderObj.h"
#include <stdexcept>
#include "Game/Entity.h"
#include "Framework/Factory.h"
#include "Framework/PropertyEd.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"
#include "Physics/Physics2.h"
#include "External/ImGuizmo.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetBrowser.h"
#include "Framework/MulticastDelegate.h"
#include "GameEnginePublic.h"
#include "AssetCompile/Someutils.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "Game/LevelAssets.h"
#include "LevelSerialization/SerializationAPI.h"
#include "Render/DrawPublic.h"
#include "Game/EntityComponent.h"
#include "Game/SerializePtrHelpers.h"
#include "Framework/FnFactory.h"
#include "Framework/ConsoleCmdGroup.h"
#include "UI/Widgets/Layouts.h"
#include "Input/InputSystem.h"
#include <variant>
#include "LevelSerialization/SerializationAPI.h"
extern ConfigVar g_mousesens;

enum TransformType
{
	TRANSLATION,
	ROTATION,
	SCALE
};


extern bool std_string_input_text(const char* label, std::string& str, int flags);
const ImColor non_owner_source_color = ImColor(252, 226, 131);

class EditorDoc;

template<typename T>
using uptr = std::unique_ptr<T>;


class Texture;
class OONameFilter;
class ObjectOutliner
{
public:
	ObjectOutliner(EditorDoc& ed_doc);
	~ObjectOutliner();

	void draw();
	void init();
private:	
	bool should_draw_this(Entity* e) const;

	void on_selection_change();
	int determine_object_count() const;
	void rebuild_tree();
	void on_start() { rebuild_tree(); }
	void on_close() { delete_tree(); }
	void delete_tree() {
		rootnode.reset(nullptr);	// deletes
		num_nodes = 0;
	}
	void on_changed_ents() { 
		rebuild_tree();
	}
	struct Node {
		Node() {}
		Node(ObjectOutliner* oo, Entity* initfrom, const std::vector<std::vector<std::string>>& filter);
		void add_child(uptr<Node> other) {
			other->parent = this;
			children.push_back(std::move(other));
		}
		void sort_children() {
			std::sort(children.begin(), children.end(), [](const uptr<Node>& a, const uptr<Node>& b)->bool {
				return to_lower(a->ptr->get_editor_name()) < to_lower(b->ptr->get_editor_name());
				});
		}
		bool is_visible = true;
		bool did_pass_filter = false;
		EntityPtr ptr;
		Node* parent = nullptr;
		std::vector<uptr<Node>> children;
	};
	struct IteratorDraw {
		IteratorDraw(ObjectOutliner* oo, Node* n) : oo(oo), node(n) {}
		bool step();
		void draw(EditorDoc& ed_doc);
		Node* get_node() const {
			return node;
		}
	private:
		ObjectOutliner* oo = nullptr;
		std::vector<int> child_stack;
		int child_index = 0;
		Node* node = nullptr;
	};
	EditorDoc& ed_doc;
	friend struct IteratorDraw;
	EntityPtr setScrollHere;
	int num_nodes = 0;
	uptr<Node> rootnode;
	uptr<OONameFilter> filter;
	AssetPtr<Texture> visible;
	AssetPtr<Texture> hidden;
	EntityPtr contextMenuHandle;
};

class EdPropertyGrid
{
public:
	EdPropertyGrid(EditorDoc& ed_doc, const FnFactory<IPropertyEditor>& factory);
	void draw();
	MulticastDelegate<> on_property_change;
private:
	void on_ec_deleted(uint64_t comp) {
		if (selected_component == comp)
			selected_component = 0;
		refresh_grid();
	}
	void on_close() {
		grid.clear_all();
	}
	void refresh_grid();

	uint64_t selected_component = 0;
	uint64_t component_context_menu = 0;

	void on_select_component(Component* ec) {
		selected_component = ec->get_instance_id();
		refresh_grid();
	}

	Component* get_selected_component() const {
		if (selected_component == 0) return nullptr;
		auto o = eng->get_object(selected_component);
		if (!o) return nullptr;
		return o->cast_to<Component>();
	}
	
	void draw_components(Entity* entity);

	EditorDoc& ed_doc;
	PropertyGrid grid;
	const FnFactory<IPropertyEditor>& factory;

	string component_filter;
	bool component_set_keyboard_focus = true;
};

class OrthoCamera
{
public:
	bool can_take_input() const {
		return eng->is_game_focused();
	}

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
	void update_from_input(float aspectratio) {
		auto mouseDelta = Input::get_mouse_delta();


		position += side * (g_mousesens.get_float() * mouseDelta.x) * width;
		position += up * (g_mousesens.get_float() * mouseDelta.y) * width * aspectratio;
	}
	glm::mat4 get_view_matrix() const {
		return glm::lookAt(position, position+front, up);
	}
	glm::mat4 get_proj_matrix(float aspect_ratio) const {
		return glm::orthoZO(-width, width, -width*aspect_ratio, width * aspect_ratio,1000.f,0.001f /* reverse z*/);
	}

	// used for ImGuizmo which doesnt like reverse Z
	glm::mat4 get_friendly_proj_matrix(float aspect_ratio) const {
		return glm::ortho(-width, width, -width * aspect_ratio, width * aspect_ratio,0.001f, 1000.f);
	}
}; 
class guiEditorCube;
class guiText;
class EditorUILayout : public guiFullscreen {
public:
	CLASS_BODY(EditorUILayout);

	EditorUILayout();
	void start() final;

	void on_pressed(int x, int y, int button) override;
	void on_released(int x, int y, int button) override;
	void on_key_down(const SDL_KeyboardEvent& key_event) override;
	void on_key_up(const SDL_KeyboardEvent& key_event) override;
	void on_mouse_scroll(const SDL_MouseWheelEvent& wheel) override;
	void on_dragging(int x, int y) override;
	void paint(UIBuilder& builder) override;

	bool mouse_clicked = false;
	int button_clicked = 0;


	MulticastDelegate<const SDL_KeyboardEvent&> key_down_delegate;
	MulticastDelegate<const SDL_KeyboardEvent&> key_up_delegate;
	MulticastDelegate<int, int, int> mouse_down_delegate;
	MulticastDelegate<int, int> mouse_drag_delegate;

	MulticastDelegate<int, int, int> mouse_up_delegate;
	MulticastDelegate<const SDL_MouseWheelEvent&> wheel_delegate;

	guiEditorCube* cube = nullptr;
	guiText* tool_text = nullptr;

	EditorDoc* doc = nullptr;
};


class SelectionState
{
public:
	SelectionState(EditorDoc& ed_doc);

	MulticastDelegate<> on_selection_changed;

	bool has_any_selected() const {
		return !selected_entity_handles.empty();
	}

	int num_entities_selected() const {
		return selected_entity_handles.size();
	}

	bool has_only_one_selected() const {
		return num_entities_selected() == 1;
	}

	EntityPtr get_only_one_selected() const {
		ASSERT(has_only_one_selected());
		return EntityPtr(*selected_entity_handles.begin());
	}

	const std::unordered_set<uint64_t>& get_selection() const { return selected_entity_handles; }
	std::vector<EntityPtr> get_selection_as_vector() const {
		std::vector<EntityPtr> out;
		for (auto e : selected_entity_handles) {
			out.push_back(EntityPtr(e));
			ASSERT(eng->get_object(e)->is_a<Entity>());
		}
		return out;
	}

	void add_to_entity_selection(EntityPtr ptr) {
		ASSERT(eng->get_object(ptr.handle) && eng->get_object(ptr.handle)->is_a<Entity>());


		bool already_selected = is_entity_selected(ptr);
		if (!already_selected) {
			auto e = eng->get_entity(ptr.handle);
			e->selected_in_editor = true;
			e->set_ws_transform(e->get_ws_transform());

			selected_entity_handles.insert(ptr.handle);

			on_selection_changed.invoke();
		}
	}
	void add_to_entity_selection(const Entity* e) {
		return add_to_entity_selection(e->get_self_ptr());
	}

	void remove_from_selection(EntityPtr ptr) {

		auto e = ptr.get();
		if (e) {	// can be null
			e->selected_in_editor = false;
			e->set_ws_transform(e->get_ws_transform());
		}
		selected_entity_handles.erase(ptr.handle);
		on_selection_changed.invoke();
	}
	void remove_from_selection(const Entity* e) {
		remove_from_selection(e->get_self_ptr());
	}

	void validate_selection() {
		auto presize = selected_entity_handles.size();
		for (auto it = selected_entity_handles.begin(); it != selected_entity_handles.end();)
		{
			EntityPtr ptr(*it);
			auto ent = ptr.get();
			if (ent==nullptr) {
				it = selected_entity_handles.erase(it);
			}
			else {
				++it;
			}
		}
		if(selected_entity_handles.size()!=presize)
			on_selection_changed.invoke();
	}

	void clear_all_selected() {
		for (auto o : selected_entity_handles) {
			auto e = eng->get_entity(o);
			if (e) {
				e->selected_in_editor = false;
				e->set_ws_transform(e->get_ws_transform());
			}
		}
		selected_entity_handles.clear();
		
		on_selection_changed.invoke();
	}
	void set_select_only_this(EntityPtr ptr) {
		clear_all_selected();
		add_to_entity_selection(ptr);
	}
	void set_select_only_this(const Entity* e) {
		set_select_only_this(e->get_self_ptr());
	}

	bool is_entity_selected(EntityPtr ptr) const {
		return selected_entity_handles.find(ptr.handle) != selected_entity_handles.end();
	}
	bool is_entity_selected(const Entity* e) const {
		return is_entity_selected(e->get_self_ptr());
	}
private:
	void on_node_deleted() {
		validate_selection();
	}
	
	void on_close() {
		selected_entity_handles.clear();
		on_selection_changed.invoke();
	}

	std::unordered_set<uint64_t> selected_entity_handles;
};

class ManipulateTransformTool
{
public:
	ManipulateTransformTool(EditorDoc& ed_doc);
	void update();
	bool is_hovered();
	bool is_using();

	void stop_using_custom() {
		if (is_using_for_custom) {
			eng->log_to_fullscreen_gui(Info, "Stopped Using Custom Manipulate Tool");
		}
		is_using_for_custom = false;
		custom_user_key = nullptr;
		update_pivot_and_cached();
	}
	void set_start_using_custom(void* key, glm::mat4 transform_to_edit) {
		if (!is_using_for_custom) {
			eng->log_to_fullscreen_gui(Info, "Using Custom Manipulate Tool");
		}

		world_space_of_selected.clear();
		current_transform_of_group = transform_to_edit;
		is_using_for_custom = true;
		custom_user_key = key;
	}
	bool is_using_key_for_custom(void* key) {
		return key == custom_user_key;
	}
	glm::mat4 get_custom_transform() {
		return current_transform_of_group;
	}
	bool get_is_using_for_custom() const {
		return is_using_for_custom;
	}


	void set_force_gizmo_on(bool b) {
		force_gizmo_on = b;
		axis_mask = 0xff;
	}
	bool get_force_gizmo_on() const {
		return force_gizmo_on;
	}
	void reset_group_to_pre_transform();

	ImGuizmo::OPERATION get_operation_type() const {
		return operation_mask;
	}
	void set_operation_type(ImGuizmo::OPERATION op) {
		operation_mask = op;
	}
	void set_mode(ImGuizmo::MODE m) {
		mode = m;
	}
	ImGuizmo::MODE get_mode() const {
		return mode;
	}
private:
	bool force_gizmo_on = false;


	void on_key_down(const SDL_KeyboardEvent& k);

	void on_close();
	void on_open();
	void on_component_deleted(Component* ec);
	void on_entity_changes();
	void on_selection_changed();
	void on_prop_change();

	void on_selected_tarnsform_change(uint64_t);

	void update_pivot_and_cached();

	void begin_drag();
	void end_drag();

	enum StateEnum {
		IDLE,
		SELECTED,
		MANIPULATING_OBJS,
	}state = IDLE;

	int axis_mask = 0xff;
	ImGuizmo::OPERATION force_operation = {};
	ImGuizmo::OPERATION operation_mask = ImGuizmo::OPERATION::TRANSLATE;
	ImGuizmo::MODE mode = ImGuizmo::MODE::WORLD;
	bool has_any_changed = false;

	void* custom_user_key = nullptr;
	bool is_using_for_custom = false;

	std::unordered_map<uint64_t,glm::mat4> world_space_of_selected; // pre transform, ie transform of them is 
	
	glm::mat4 current_transform_of_group = glm::mat4(1.0);
	glm::mat4 pivot_transform = glm::mat4(1.f);

	EditorDoc& ed_doc;
};
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };
class LEPlugin;
class EditorUILayout;
class Model;
class EditorDoc : public IEditorTool
{
public:
	static MulticastDelegate<EditorDoc*> on_creation;
	static MulticastDelegate<EditorDoc*> on_deletion;

	static EditorDoc* create_scene(opt<string> scenePath);
	static EditorDoc* create_prefab(PrefabAsset* prefab);

	~EditorDoc();
	EditorDoc& operator=(const EditorDoc& other) = delete;
	EditorDoc(const EditorDoc& other) = delete;

	void init_new();

	uptr<CreateEditorAsync> create_command_to_load_back() { return nullptr; }

	bool save_document_internal() final;
	void hook_menu_bar() final;
	void hook_imgui_newframe() final {
		ImGuizmo::BeginFrame();
	}
	void hook_scene_viewport_draw() final;
	void hook_pre_scene_viewport_draw() final;
	bool wants_scene_viewport_menu_bar() const { return true; }
	const ClassTypeInfo& get_asset_type_info() const final {
		return is_editing_scene() ? SceneAsset::StaticType : PrefabAsset::StaticType;
	}
	void tick(float dt) final;
	void imgui_draw() final;
	const View_Setup* get_vs() final { return &vs_setup; }

	std::string get_full_output_path()  {
		return get_doc_name().empty() ? "Maps/<unnamed map>" : "Maps/" + get_doc_name();
	}

	enum MouseSelectionAction {
		SELECT_ONLY,
		UNSELECT,
		ADD_SELECT,
	};
	void do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_root_most_entity);

	void on_mouse_down(int x, int y, int button);
	void on_key_down(const SDL_KeyboardEvent& k);
	void on_mouse_wheel(const SDL_MouseWheelEvent& wheel) {
		if (using_ortho && ortho_camera.can_take_input())
			ortho_camera.scroll_callback(wheel.y);
		if(!using_ortho && camera.can_take_input())
			camera.scroll_callback(wheel.y);
	}

	void duplicate_selected_and_select_them();
	glm::vec3 unproject_mouse_to_ray(int mx, int my);

	const char* get_save_file_extension() const {
		if (is_editing_prefab()) return "pfb";
		else return "tmap";
	}

	enum EditCategory {
		EDIT_SCENE,
		EDIT_PREFAB,
	};

	bool is_this_object_not_inherited(const BaseUpdater* b) const {
		return this_is_a_serializeable_object(b);	// not inherted meaning i can edit it
	}
	bool is_this_object_inherited(const BaseUpdater* b) const {
		return !is_this_object_not_inherited(b);	// inherited, meaning i cant edit it
	}

	bool can_delete_this_object(const BaseUpdater* b) {
		assert(b);
		if (is_this_object_inherited(b)) // cant delete inherited objects
			return false;
		if (is_editing_prefab()) {
			auto ent = b->cast_to<Entity>();
			if (ent && ent == get_prefab_root_entity()) {
				return false;
			}
		}
		return true;	// else can delete
	}
	bool is_in_eyedropper_mode() const {
		return eye_dropper_active;
	}
	void enable_entity_eyedropper_mode(void* id);
	void exit_eyedropper_mode();
	void* get_active_eyedropper_user_id() {
		return active_eyedropper_user_id;
	}
	bool is_editing_prefab() const { return edit_category == EditCategory::EDIT_PREFAB; }
	bool is_editing_scene() const { return edit_category == EditCategory::EDIT_SCENE; }
	
	void validate_prefab();
	Entity* get_prefab_root_entity();
	string get_name();

	bool local_transform = false;
	TransformType transform_tool_type;
	int axis_bit_mask = 0;
	glm::vec3 transform_tool_origin;
	void transform_tool_update();
	void enter_transform_tool(TransformType type);
	void leave_transform_tool(bool apply_delta);
	void set_plugin(const ClassTypeInfo* plugin_type) {}

	Entity* spawn_entity();
	Component* attach_component(const ClassTypeInfo* ti, Entity* e);
	void remove_scene_object(BaseUpdater* u);
	void insert_unserialized_into_scene(UnserializedSceneFile& file, SerializedSceneFile* scene);
	void instantiate_into_scene(BaseUpdater* u);
	Entity* spawn_prefab(PrefabAsset* prefab);


	//std::unique_ptr<LEPlugin> active_plugin;
	std::unique_ptr<UndoRedoSystem> command_mgr;
	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<EdPropertyGrid> prop_editor;
	std::unique_ptr<ManipulateTransformTool> manipulate;
	std::unique_ptr<ObjectOutliner> outliner;

	EditorUILayout* gui = nullptr;

	View_Setup vs_setup;
	bool using_ortho = false;
	User_Camera camera;
	OrthoCamera ortho_camera;

	//MulticastDelegate<LEPlugin*> on_start_plugin;
	//MulticastDelegate<LEPlugin*> on_end_plugin;

	MulticastDelegate<uint64_t> on_component_deleted;
	MulticastDelegate<Component*> on_component_created;
	MulticastDelegate<EntityPtr> on_entity_created;	// after creation
	MulticastDelegate<> post_node_changes;	// called after any nodes are deleted/created

	MulticastDelegate<const Entity*> on_eyedropper_callback;

	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
	MulticastDelegate<uint64_t> on_change_name;


	void validate_fileids_before_serialize();

	void set_camera_target_to_sel();
private:
	EditorDoc();
	void init_for_prefab(PrefabAsset* prefab);
	void init_for_scene(opt<string> scenePath);


	int get_next_file_id() {
		return ++file_id_start;
	}
	void on_mouse_drag(int x, int y);


	int file_id_start = 0;

	bool eye_dropper_active = false;
	void* active_eyedropper_user_id = nullptr;	// for id purposes only

	EditCategory edit_category = EditCategory::EDIT_SCENE;

	FnFactory<IPropertyEditor> grid_factory;
	uptr<ConsoleCmdGroup> cmds;

	opt<string> assetName;
};

#endif