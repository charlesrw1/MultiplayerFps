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
#include "Render/DrawPublic.h"
#include "Game/EntityComponent.h"

#include "Framework/FnFactory.h"
#include "Framework/ConsoleCmdGroup.h"
#include "UI/Widgets/Layouts.h"
#include "Input/InputSystem.h"
#include <variant>
#include "UI/GUISystemPublic.h"
#include "LevelSerialization/SerializeNew.h"
#include "DragDropPreview.h"
#include "DragDetector.h"

extern ConfigVar g_mousesens;

extern ConfigVar ed_has_snap;
extern ConfigVar ed_translation_snap;
extern ConfigVar ed_translation_snap_exp;
extern ConfigVar ed_rotation_snap;
extern ConfigVar ed_rotation_snap_exp;
extern ConfigVar ed_scale_snap_exp;
extern ConfigVar ed_scale_snap;



enum TransformType
{
	TRANSLATION,
	ROTATION,
	SCALE
};
enum class MouseSelectionAction
{
	SELECT_ONLY,
	UNSELECT,
	ADD_SELECT,
};

extern bool std_string_input_text(const char* label, std::string& str, int flags);
const ImColor non_owner_source_color = ImColor(252, 226, 131);

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
class guiEditorCube;
class guiText;
class EditorUILayout
{
public:
	EditorUILayout(EditorDoc& doc);

	bool draw();
	void do_box_select(MouseSelectionAction action);
	Rect2d convert_rect(Rect2d screenSpaceRect) {
		Rect2d out = screenSpaceRect;
		auto pos = UiSystem::inst->get_vp_rect().get_pos();
		out.x -= pos.x;
		out.y -= pos.y;
		return out;
	}
	struct obj
	{
		glm::vec3 pos = glm::vec3(0.f);
		const Entity* e = nullptr;
	};
	std::vector<EditorUILayout::obj> get_objs();

	guiEditorCube cube;
	EditorDoc* doc = nullptr;
};







#include "Render/DrawPublic.h"

class DrawHandlesObject
{
public:
	DrawHandlesObject(EditorDoc& doc) : doc(doc) {}
	EditorDoc& doc;
	void tick();

private:
	EntityPtr last_selected;
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
	void hook_imgui_newframe() final { ImGuizmo::BeginFrame(); }
	void hook_scene_viewport_draw() final;
	void hook_pre_scene_viewport_draw() final;
	bool wants_scene_viewport_menu_bar() const { return true; }

	void tick(float dt) final;
	void imgui_draw() final;
	const View_Setup* get_vs() final { return &vs_setup; }

	std::string get_full_output_path() {
		return get_doc_name().empty() ? "Maps/<unnamed map>" : "Maps/" + get_doc_name();
	}

	void do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_root_most_entity);
	void do_mouse_selection(MouseSelectionAction action, vector<EntityPtr> ents, bool select_root_most_entity);

	void on_mouse_pick();
	void duplicate_selected_and_select_them();
	Ray unproject_mouse_to_ray(int mx, int my);

	const char* get_save_file_extension() const { return "tmap"; }

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

	bool is_editing_scene() const { return true; }

	string get_name();

	Entity* spawn_entity();
	Component* attach_component(const ClassTypeInfo* ti, Entity* e);
	void remove_scene_object(BaseUpdater* u);
	void insert_unserialized_into_scene(UnserializedSceneFile& file);
	void instantiate_into_scene(BaseUpdater* u);
	bool get_using_ortho() const { return ed_cam.get_is_using_ortho(); }
	void validate_fileids_before_serialize();
	void set_camera_target_to_sel();
	string get_doc_name() const final { return assetName.value_or("<unnamed>"); }

	std::unique_ptr<UndoRedoSystem> command_mgr;
	std::unique_ptr<SelectionState> selection_state;
	std::unique_ptr<EdPropertyGrid> prop_editor;
	std::unique_ptr<ManipulateTransformTool> manipulate;
	std::unique_ptr<DragDropPreview> drag_drop_preview;
	std::unique_ptr<FoliagePaintTool> foliage_tool;
	std::unique_ptr<DecalStampTool> stamp_tool;
	std::unique_ptr<EditorUILayout> gui;
	std::unique_ptr<EViewportHandles> handle_dragger;
	std::unique_ptr<SelectionMode> selection_mode;
	std::unique_ptr<DrawHandlesObject> draw_handles;
	View_Setup vs_setup;
	EditorInputs inputs;
	EditorCamera ed_cam;
	DragDetector dragger;
	IEditorMode* active_mode = nullptr;
	EntityVisiblityFilter vis_filter;

	MulticastDelegate<uint64_t> on_component_deleted;
	MulticastDelegate<Component*> on_component_created;
	MulticastDelegate<EntityPtr> on_entity_created; // after creation
	MulticastDelegate<> post_node_changes;			// called after any nodes are deleted/created
	MulticastDelegate<const Entity*> on_eyedropper_callback;
	MulticastDelegate<> on_start;
	MulticastDelegate<> on_close;
	MulticastDelegate<uint64_t> on_change_name;

private:
	EditorDoc();
	void init_for_scene(opt<string> scenePath);
	void on_mouse_drag(int x, int y);

	bool eye_dropper_active = false;
	void* active_eyedropper_user_id = nullptr; // for id purposes only
	FnFactory<IPropertyEditor> grid_factory;
	uptr<ConsoleCmdGroup> cmds;
	opt<string> assetName;
};

#endif