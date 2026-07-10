#pragma once
#ifdef EDITOR_BUILD
#include "IEditorTool.h"
#include "Commands.h"
#include "glm/glm.hpp"
#include "Render/Model.h"
#include "EditorInputs.h"
#include "EdPropertyGrid.h"
#include "LevelEditorCamera.h"
#include "SelectionState.h"
#include "ViewportHandles.h"
#include "EditorModes.h"
#include "ManipulateTransformTool.h"

#include "Level.h"
#include <SDL3/SDL.h>
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
#include "Framework/MyImguiLib.h"
#include "GameEnginePublic.h"
#include "AssetCompile/Someutils.h"
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include "Game/LevelAssets.h"
#include "Render/DrawPublic.h"
#include "Game/EntityComponent.h"
#include "Game/EditorAddMenu.h"

#include "Framework/FnFactory.h"
#include "Framework/ConsoleCmdGroup.h"
#include "UI/BaseGUI.h"
#include "Input/InputSystem.h"
#include <variant>
#include "UI/GUISystemPublic.h"
#include "LevelSerialization/SerializeNew.h"
#include "DragDropPreview.h"
#include "DragDetector.h"
#include "EditorUILayout.h"
#include "IEditorApi.h"
#include "RoadBuilderTool.h"

extern ConfigVar g_mousesens;

extern ConfigVar ed_has_snap;
extern ConfigVar ed_translation_snap;
extern ConfigVar ed_translation_snap_exp;
extern ConfigVar ed_rotation_snap;
extern ConfigVar ed_rotation_snap_exp;
extern ConfigVar ed_scale_snap_exp;
extern ConfigVar ed_scale_snap;

extern ConfigVar editor_draw_name_text;

enum TransformType
{
	TRANSLATION,
	ROTATION,
	SCALE
};
extern glm::ivec2 ndc_to_screen_coord(glm::vec3 ndc);
extern bool std_string_input_text(const char* label, std::string& str, int flags);
const ImColor non_owner_source_color = ImColor(252, 226, 131);
extern Bounds transform_bounds(glm::mat4 transform, Bounds b);
class EditorDoc;

template <typename T> using uptr = std::unique_ptr<T>;

// orbit and ortho camera
/*
EditorCamera
	orbit_target
	mode

*/

#include "UI/GUISystemPublic.h"
#include "UI/Widgets/EditorCube.h"

#include "Render/DrawPublic.h"
#include "Framework/MeshBuilder.h"

class MaterialInstance;
class DrawHandlesObject
{
public:
	DrawHandlesObject(EditorDoc& doc) : doc(doc) {}
	~DrawHandlesObject();
	EditorDoc& doc;
	void tick();

private:
	// Blender-style dashed parent lines: rebuilt each frame into a single textured ribbon (a stretched
	// dashed sprite, not GL lines). Drawn as a particle object because that is the only meshbuilder
	// render path that actually binds a material/texture; the plain meshbuilder path ignores it and
	// renders flat vertex-colour lines. Prefab-edit mode only (empty meshbuilder = nothing drawn).
	void tick_parent_lines();
	EntityPtr last_selected;
	bool was_dragging = false;
	glm::mat4 pre_drag_transform{};

	MeshBuilder parent_line_mb;
	handle<Particle_Object> parent_line_handle;
	MaterialInstance* dashed_mat = nullptr;
	bool tried_load_dashed_mat = false;
};

class EntityVisiblityFilter
{
public:
	EntityVisiblityFilter(EditorDoc& doc) : doc(doc) {}
	void tick();

private:
	EditorDoc& doc;
	std::unordered_map<std::string, bool> status;
};

template <class... Ts> struct overloads : Ts...
{ using Ts::operator()...; };
class LEPlugin;
class EditorUILayout;
class Model;


class CameraApiImpl : public IEditorCameraApi
{
public:
	CameraApiImpl(EditorCamera* cam) : cam(cam) {}

	void set_look_at(glm::vec3 pos, glm::vec3 look) final {
		// Set camera to look at 'look' from position 'pos'
		cam->set_orbit_target(look, glm::distance(pos, look));
	}

	glm::vec3 get_positon() const final { return cam->make_view().origin; }

	Ray unproject_ray(int x, int y) const final { return cam->unproject_mouse(x, y); }

	bool is_ortho() const final { return cam->get_is_using_ortho(); }

	View_Setup get_view_setup() const final { return cam->make_view(); }

	void set_ortho_view(glm::vec3 eye_dir) final { cam->set_ortho_view(eye_dir); }
	void set_perspective_view() final { cam->set_perspective_view(); }

	EditorCamera* cam = nullptr;
};

class DocumentApiImpl : public IDocumentApi
{
public:
	DocumentApiImpl(EditorDoc* doc) : doc(doc) {}

	void save() final;
	void undo() final;
	void redo() final;
	std::string get_document_name() const final;

	EditorDoc* doc = nullptr;
};

class SelectionApiImpl : public ISelectionApi
{
public:
	SelectionApiImpl(SelectionState* ss) : ss(ss) {}

	std::vector<EntityPtr> get_selected() const final { return ss->get_selection_as_vector(); }
	viewMulticastDelegate<> on_selection_changed() const final { return ss->on_selection_changed; }
	void add_select(EntityPtr ptr) final { ss->add_to_entity_selection(ptr); }
	void remove_select(EntityPtr ptr) final { ss->remove_from_selection(ptr); }
	void clear_selected() final { ss->clear_all_selected(); }
	bool is_selected(EntityPtr ptr) const final { return ss->is_entity_selected(ptr); }

	SelectionState* ss = nullptr;
};

class EditorApi2Impl : public IEditorApi2
{
public:
	EditorApi2Impl(CameraApiImpl* cam, SelectionApiImpl* sel, DocumentApiImpl* doc)
		: cam_impl(cam), sel_impl(sel), doc_impl(doc) {}

	IEditorCameraApi* camera() final { return cam_impl; }
	ISelectionApi* selection() final { return sel_impl; }
	IDocumentApi* document() final { return doc_impl; }

	CameraApiImpl* cam_impl = nullptr;
	SelectionApiImpl* sel_impl = nullptr;
	DocumentApiImpl* doc_impl = nullptr;
};

extern string get_name_display_entity(const Entity* e);
class EditorDoc : public IEditorTool
{
public:
	static MulticastDelegate<EditorDoc*> on_creation;
	static MulticastDelegate<EditorDoc*> on_deletion;

	static EditorDoc* create_scene(opt<string> scenePath);

	~EditorDoc();
	EditorDoc& operator=(const EditorDoc& other) = delete;
	EditorDoc(const EditorDoc& other) = delete;

	void init_new();
	void set_document_path(string newAssetName);
	void check_inputs();
	bool save_document_internal() final;
	void hook_menu_bar() final;
	void hook_menu_bar_file_menu() final;
	void hook_imgui_newframe() final { ImGuizmo::BeginFrame(); }
	void hook_scene_viewport_draw() final;
	void hook_pre_scene_viewport_draw() final;
	bool wants_scene_viewport_menu_bar() const { return true; }

	void tick(float dt) final;
	void imgui_draw() final;
	const View_Setup* get_vs() final { return &vs_setup; }

	void do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_root_most_entity);
	void do_mouse_selection(MouseSelectionAction action, vector<EntityPtr> ents, bool select_root_most_entity);

	void on_mouse_pick();
	void duplicate_selected_and_select_them();
	Ray unproject_mouse_to_ray(int mx, int my);

	const char* get_save_file_extension() const { return editing_prefab ? "tprefab" : "tmap"; }
	void add_editor_commands();
	bool is_this_object_not_inherited(const BaseUpdater* b) const {
		return this_is_a_serializeable_object(b); // not inherted meaning i can edit it
	}
	bool is_this_object_inherited(const BaseUpdater* b) const {
		return !is_this_object_not_inherited(b); // inherited, meaning i cant edit it
	}

	bool can_delete_this_object(const BaseUpdater* b) {
		assert(b);
		if (is_this_object_inherited(b)) // cant delete inherited objects
			return false;
		return true; // else can delete
	}
	bool is_in_eyedropper_mode() const { return eye_dropper_active; }
	void enable_entity_eyedropper_mode(void* id);
	void exit_eyedropper_mode();
	void* get_active_eyedropper_user_id() { return active_eyedropper_user_id; }

	bool is_editing_scene() const { return !is_editing_prefab(); }
	bool is_editing_prefab() const { return editing_prefab; }

	IEditorApi2& get_editor_api() final { return *editor_api2; }

	string get_name();

	Entity* spawn_entity();
	Component* attach_component(const ClassTypeInfo* ti, Entity* e);
	void remove_scene_object(BaseUpdater* u);
	void insert_unserialized_into_scene(UnserializedSceneFile& file);
	void instantiate_into_scene(BaseUpdater* u);
	bool get_using_ortho() const { return ed_cam.get_is_using_ortho(); }
	void validate_fileids_before_serialize();
	void set_camera_target_to_sel();
	string get_doc_name() const final {
		string name = assetName.value_or("<unnamed>");
		if (editing_prefab) name += " [Prefab]";
		return name;
	}
	// Raw asset path (empty if unnamed). Used by EditorRecents to record
	// which document the user is leaving.
	string get_asset_path() const { return assetName.value_or(""); }

	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<SelectionApiImpl> sel_api_impl;
	std::unique_ptr<CameraApiImpl> cam_api_impl;
	std::unique_ptr<DocumentApiImpl> doc_api_impl;
	std::unique_ptr<EditorApi2Impl> editor_api2;

	std::unique_ptr<UndoRedoSystem> command_mgr;

	// Delegates declared before prop_editor so they are destroyed after it
	// (reverse declaration order). Editors subscribe to these in their ctors
	// and unsubscribe in their dtors; the delegates must still be alive then.
	MulticastDelegate<uint64_t> on_component_deleted;
	MulticastDelegate<Component*> on_component_created;
	MulticastDelegate<EntityPtr> on_entity_created; // after creation
	MulticastDelegate<> post_node_changes;			// called after any nodes are deleted/created
	MulticastDelegate<const Entity*> on_eyedropper_callback;
	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
	MulticastDelegate<uint64_t> on_change_name;

	std::unique_ptr<EdPropertyGrid> prop_editor;
	std::unique_ptr<ManipulateTransformTool> manipulate;
	std::unique_ptr<DragDropPreview> drag_drop_preview;
	std::unique_ptr<FoliagePaintTool> foliage_tool;
	std::unique_ptr<DecalStampTool> stamp_tool;
	std::unique_ptr<EditorUILayout> gui;
	std::unique_ptr<EViewportHandles> handle_dragger;
	std::unique_ptr<SelectionMode> selection_mode;
	std::unique_ptr<RoadBuilderTool> road_tool;
	std::unique_ptr<DrawHandlesObject> draw_handles;
	View_Setup vs_setup;
	EditorCamera ed_cam;
	IEditorMode* active_mode = nullptr;
	EntityVisiblityFilter vis_filter;

private:
	EditorDoc();
	void init_for_scene(opt<string> scenePath);
	void on_mouse_drag(int x, int y);

	bool eye_dropper_active = false;
	void* active_eyedropper_user_id = nullptr; // for id purposes only
	bool editing_prefab = false;
	// Set by check_inputs() on Ctrl+P / Alt+P; consumed in the viewport menu-bar draw where the
	// parenting popups live (must OpenPopup in the same imgui window as their BeginPopup).
	bool want_open_parent_menu = false;
	bool want_open_unparent_menu = false;
	// Ctrl+Tab quick-switcher over EditorRecents (see EditorRecents.h). While open, repeated Tab
	// (Shift+Tab reverses) or Up/Down cycle recent_switcher_index; Enter or releasing Ctrl confirms
	// (alt-tab style); Escape cancels. Confirm executes the same "recent N" command as the Open
	// Recent menu, deferred through Cmd_Manager for the same imgui-teardown-safety reason.
	bool want_open_recent_switcher = false;
	bool recent_switcher_open = false;
	int recent_switcher_index = 0;
	bool check_recent_switcher_input(bool has_ctrl, bool has_shift);
	void confirm_recent_switcher();
	void draw_recent_switcher_popup();
	// "Parent to Bone" searchable combo state (same pattern as EntityBoneParentStringEditor).
	std::string parent_bone_filter_buf;
	bool parent_bone_focus_filter = true;
	void draw_parenting_popups();

	// Right-click scene context menu. EditorCamera::tick() unconditionally grabs input focus on
	// right-mouse-down (for fly-camera), so a still right-click vs. a right-click-drag is
	// disambiguated here by tracking press/release ourselves rather than relying on focus state.
	// Movement is accumulated relative motion, not absolute screen position, since the fly camera
	// captures the mouse (relative/warped) while held.
	bool rmb_press_tracking = false;
	uint64_t rmb_press_time_ms = 0;
	glm::ivec2 rmb_drag_accum_px{0, 0};
	bool want_open_scene_context_menu = false;
	glm::mat4 scene_context_menu_transform = glm::mat4(1.f);
	void check_scene_context_menu_input();
	void draw_scene_context_menu();
	// Set while the "Add" submenu (or one of its nested categories) is open, so the Blender-style
	// auto-close-on-mouse-distance check in draw_scene_context_menu() doesn't close the root popup
	// out from under it.
	bool scene_ctx_menu_has_open_submenu = false;
	// Path prompt for "Make Prefab Using..." in the scene context menu; drawn unconditionally each
	// frame alongside draw_scene_context_menu() so it survives the right-click popup closing.
	FolderNamePopup make_prefab_path_popup;
	void select_entities_using_asset(const std::string& asset_gamepath) override;
	// Draws the nested "Add" submenu tree built from EditorAddMenuRegistry entries (Game/EditorAddMenu.h).
	void draw_add_menu_tree(const std::vector<EditorAddMenuEntry>& entries, const glm::mat4& transform);
	FnFactory<IPropertyEditor> grid_factory;
	uptr<ConsoleCmdGroup> cmds;
	opt<string> assetName;

	EditorInputs inputs;
};

#endif