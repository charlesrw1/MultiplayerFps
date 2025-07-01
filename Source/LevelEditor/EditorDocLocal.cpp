#include "EditorDocLocal.h"
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "glm/gtx/euler_angles.hpp"
#include "External/ImGuizmo.h"

#include "Framework/MeshBuilder.h"

#include "Framework/Files.h"
#include "Framework/MyImguiLib.h"
#include "Framework/DictWriter.h"

#include "Physics/Physics2.h"

#include "Debug.h"
#include <algorithm>
#include <stdexcept>
#include <fstream>

#include "Render/DrawPublic.h"
#include "Render/Texture.h"

#include "AssetCompile/Someutils.h"// string stuff
#include "Assets/AssetRegistry.h"

#include "UI/Widgets/Layouts.h"
#include "UI/Widgets/Interactables.h"
#include "UI/Widgets/Visuals.h"
#include "UI/GUISystemPublic.h"

#include "Game/LevelAssets.h"

#include "LevelEditor/Commands.h"
#include "Framework/Rect2d.h"

#include "Scripting/ScriptAsset.h"

#include "Framework/AddClassToFactory.h"

#include "Game/EntityComponent.h"


#include "UI/UIBuilder.h"



ConfigVar g_editor_newmap_template("g_editor_newmap_template", "eng/template_map.tmap", CVAR_DEV, "whenever a new map is created, it will use this map as a template");
ConfigVar editor_draw_name_text("editor_draw_name_text", "0", CVAR_BOOL, "draw text above every entities head in editor");
ConfigVar editor_draw_name_text_alpha("editor_draw_name_text_alpha", "150", CVAR_INTEGER, "", 0, 255);


extern bool this_is_a_serializeable_object(const BaseUpdater* b, const PrefabAsset* for_prefab);


static std::string to_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}
// Unproject mouse coords into a vector
glm::vec3 EditorDoc::unproject_mouse_to_ray(const int mx, const int my)
{
	Ray r;
	// get ui size

	const auto viewport_size = UiSystem::inst->get_vp_rect().get_size();
	const auto viewport_pos = UiSystem::inst->get_vp_rect().get_pos();

	const auto size = viewport_size;
	const int wx = viewport_pos.x;
	const int wy = viewport_pos.y;
	const float aratio = float(wy) / wx;
	glm::vec3 ndc = glm::vec3(float(mx - size.x) / wx, float(my - size.y) / wy, 0);
	ndc = ndc * 2.f - 1.f;
	ndc.y *= -1;

	if (using_ortho) {
		glm::vec3 pos = ortho_camera.position - ortho_camera.side * ndc.x * ortho_camera.width + ortho_camera.up * ndc.y * ortho_camera.width * aratio;
		glm::vec3 front = ortho_camera.front;
		r.pos = pos;
		r.dir = front;
	}
	else {
		r.pos = vs_setup.origin;


		glm::mat4 invviewproj = glm::inverse(vs_setup.viewproj);
		glm::vec4 point = invviewproj * glm::vec4(ndc, 1.0);
		point /= point.w;

		glm::vec3 dir = glm::normalize(glm::vec3(point) - r.pos);

		r.dir = dir;
	}
	return r.dir;
}

Color32 to_color32(glm::vec4 v) {
	Color32 c;
	c.r = glm::clamp(v.r * 255.f, 0.f, 255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}


void EditorDoc::validate_fileids_before_serialize()
{
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	if (edit_category == EditCategory::EDIT_PREFAB) {
		Entity* root = nullptr;
		for (auto o : objs) {
			auto ent = o->cast_to<Entity>();
			if (ent && !ent->get_parent() && !ent->dont_serialize_or_edit) {
				if (!root)
					root = ent;
				else {
					sys_print(Warning, "found an object not parented to root, parenting to first found\n");
					ent->parent_to(root);
				}
			}
		}
		if (!root) {
			sys_print(Warning, "prefab has no root??, making one\n");
			level->spawn_entity();
		}
		validate_prefab();	// idk just run this agian
	}


	// first find max
	for (auto o : objs)
		if (is_this_object_not_inherited(o) && !o->dont_serialize_or_edit) {
			file_id_start = std::max(file_id_start, o->unique_file_id);
		}
	for (auto o : objs) {
		if (is_this_object_not_inherited(o) && o->unique_file_id == 0 && !o->dont_serialize_or_edit)
			o->unique_file_id = get_next_file_id();
	}
}
#include "PropertyEditors.h"
void EditorDoc::init_new()
{
	sys_print(Debug, "Edit mode: %s", (edit_category == EDIT_PREFAB) ? "Prefab" : "Scene");
	eng->get_level()->validate();
	command_mgr = std::make_unique<UndoRedoSystem>();

	dragger.on_drag_end.add(this,[this](Rect2d rect) {
		auto newRect = gui.convert_rect(rect);
		auto selection = idraw->mouse_box_select_for_editor(newRect.x, newRect.y, newRect.w, newRect.h);
		auto type = MouseSelectionAction::ADD_SELECT;
		if (Input::is_shift_down())
			type = MouseSelectionAction::ADD_SELECT;
		else if (Input::is_ctrl_down())
			type = MouseSelectionAction::UNSELECT;
		else {
			selection_state->clear_all_selected();
		}
		for (auto handle : selection) {
			if (handle.is_valid()) {
				auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
				if (component_ptr) {
					auto owner = component_ptr->get_owner();
					ASSERT(owner);

					do_mouse_selection(type, owner, true);
				}
			}
		}
		});

	command_mgr->on_command_execute_or_undo.add(this, [&]() {
		set_has_editor_changes();
		});

	selection_state = std::make_unique<SelectionState>(*this);
	prop_editor = std::make_unique<EdPropertyGrid>(*this, grid_factory);
	manipulate = std::make_unique<ManipulateTransformTool>(*this);
	outliner = std::make_unique<ObjectOutliner>(*this);

	PropertyFactoryUtil::register_basic(grid_factory);
	PropertyFactoryUtil::register_editor(*this, grid_factory);


	//global_asset_browser.init();
	outliner->init();

	cmds = ConsoleCmdGroup::create("");
	cmds->add("SET_ORBIT_TARGET", [this](const Cmd_Args&) {
		set_camera_target_to_sel();
		});
	cmds->add("ed.HideSelected", [this](const Cmd_Args&) {
		eng->log_to_fullscreen_gui(Info, "Hide selected");
		auto& selection = selection_state->get_selection();
		for (auto s : selection) {
			EntityPtr handle(s);
			if (handle) {
				handle->set_hidden_in_editor(true);
			}
		}
		});
	cmds->add("ed.UnHideAll", [this](const Cmd_Args&) {
		eng->log_to_fullscreen_gui(Info, "Unhide all");
		auto level = eng->get_level();
		if (level) {
			for (auto e : level->get_all_objects()) {
				if (e->is_a<Entity>()) {
					auto ent = e->cast_to<Entity>();
					if (ent->get_hidden_in_editor())
						ent->set_hidden_in_editor(false);
				}
			}
		}
		});

	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  leveldock.ini");

	assert(eng->get_level());
	gui.doc = this;
	//gui = eng->get_level()->spawn_entity()->create_component<EditorUILayout>();
	//gui->doc = this;
	//gui->set_owner_dont_serialize_or_edit(true);
	//gui->set_focus();
	//gui->key_down_delegate.add(this, &EditorDoc::on_key_down);
	//gui->mouse_drag_delegate.add(this, &EditorDoc::on_mouse_drag);
	//gui->wheel_delegate.add(this, &EditorDoc::on_mouse_wheel);
	//gui->key_down_delegate.add(command_mgr.get(), &UndoRedoSystem::on_key_event);
}
void EditorDoc::set_document_path(string newAssetName)
{
	if (newAssetName.empty()) {
		sys_print(Warning, "set_document_path: empty path\n");
		return;
	}
	if (assetName.has_value()) {
		sys_print(Warning, "EditorDoc::set_document_path: already has path, skipping\n");
		return;
	}
	this->assetName = newAssetName;
}
#include "LevelSerialization/SerializeNew.h"
#include "EditorPopupTemplate.h"
#include "Framework/StringUtils.h"
bool EditorDoc::save_document_internal()
{
	if (assetName.has_value() && assetName.value().empty()) {
		sys_print(Warning, "EditorDoc::save_document_internal has an empty name?\n");
		assetName = std::nullopt;
	}
	if (!assetName.has_value()) {
		PopupTemplate::create_file_save_as(EditorPopupManager::inst, [&](string path) {
			sys_print(Debug, "EditorDoc::save_document_internal: popup returned with path %s\n", path.c_str());
			this->set_document_path(path);
			save_document_internal();
			}, get_save_file_extension());
		sys_print(Debug, "EditorDoc::save_document_internal: no path to save, so adding popup\n");
		return false;
	}
	assert(eng->get_level());
	eng->log_to_fullscreen_gui(Info, "Saving");
	sys_print(Info, "Saving Scene/Prefab (%s)...\n",assetName.value_or("<new>").c_str());
	auto& all_objs = eng->get_level()->get_all_objects();
	validate_fileids_before_serialize();
	std::vector<Entity*> all_ents;
	for (auto o : all_objs)
		if (auto e = o->cast_to<Entity>())
			all_ents.push_back(e);
	string debug_tag = "saving:" + assetName.value_or("<new>");
	auto serialized = serialize_entities_to_text(debug_tag.c_str(),all_ents);
	//NewSerialization::serialize_to_text(all_ents, pa);
	assert(assetName.has_value());
	const string path = assetName.value();

	auto outfile = FileSys::open_write_game(path.c_str());
	if (!outfile) {
		sys_print(Error, "EditorDoc::save_document_internal: couldnt write to output file! Writing recovery file.\n");
		string recovery_path = "recovery_"+StringUtils::alphanumeric_hash(assetName.value());
		outfile = FileSys::open_write_game(recovery_path.c_str());
		if (!outfile) {
			sys_print(Error, "EditorDoc::save_document_internal: couldnt write recovery file :(\n");
		}
		else {
			sys_print(Info, "Writing recovery file for %s: %s\n", assetName.value().c_str(), recovery_path.c_str());
			outfile->write(serialized.text.c_str(), serialized.text.size());
		}
	}
	else {
		outfile->write(serialized.text.c_str(), serialized.text.size());
		sys_print(Info, "Wrote Map/Prefab out to %s\n", path.c_str());
		outfile->close();
	}

	if (is_editing_prefab()) {
		PrefabAsset* pfb = g_assets.find_sync<PrefabAsset>(path).get();
		g_assets.reload_sync<PrefabAsset>(pfb);
	}

	return true;
}
Entity* EditorDoc::get_prefab_root_entity()
{
	ASSERT(is_editing_prefab());
	Entity* root = nullptr;
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	for (auto o : objs) {
		if (auto e = o->cast_to<Entity>()) {
			if (e->dont_serialize_or_edit)
				continue;
			if (!e->get_parent()) {
				assert(e->get_object_prefab_spawn_type() != EntityPrefabSpawnType::SpawnedByPrefab);
				if (root) {
					sys_print(Warning, "EditorDoc::get_prefab_root_entity: multiple roots found\n");
				}
				root =  e;
			}
		}
	}
	if (!root) {
		sys_print(Warning, "couldnt get root of prefab??\n");
	}
	return root;
}
string EditorDoc::get_name() {
	string name = get_doc_name();
	if (name.empty()) 
		name = "<unnamed>";
	if (is_editing_prefab())
		return "Prefab: " + name;
	else
		return "Scene: " + name;
}
void EditorDoc::enable_entity_eyedropper_mode(void* id) {
	eng->log_to_fullscreen_gui(Debug, "entering eyedropper mode...");
	active_eyedropper_user_id = id;
	eye_dropper_active = true;
	//gui->tool_text->hidden = false;
	//gui->tool_text->text = "EYEDROPPER ACTIVE (esc to exit)";
	//gui->tool_text->color = { 255,128,128 };
	//gui->tool_text->use_desired_size = true;
	//gui->tool_text->pivot = guiAnchor::Center;

}
void EditorDoc::exit_eyedropper_mode() {
	if (is_in_eyedropper_mode()) {
		eng->log_to_fullscreen_gui(Debug, "exiting eyedropper");
		eye_dropper_active = false;
		active_eyedropper_user_id = nullptr;

		//gui->tool_text->hidden = true;
	}
}
void EditorDoc::validate_prefab()
{
	ASSERT(is_editing_prefab());
	Entity* root = nullptr;
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	std::vector<Entity*> deleteList;
	for (auto o : objs) {
		if (auto e = o->cast_to<Entity>()) {
			if (e->dont_serialize_or_edit)
				continue;
			if (!e->get_parent()) {
				if (root) {
					deleteList.push_back(e);
				}
				else
					root = e;
			}
		}
	}
	if (!deleteList.empty()) {
		eng->log_to_fullscreen_gui(Error, "Prefab had extra root entities, deleting\n");
		for (auto e : deleteList)
			e->destroy();
	}
	if (!root) {
		sys_print(Debug, "prefab had no root\n");
		root = spawn_entity();
	}
	assert(root->get_object_prefab_spawn_type() != EntityPrefabSpawnType::SpawnedByPrefab);

}
#include "EditorPopupTemplate.h"

MulticastDelegate<EditorDoc*> EditorDoc::on_creation;
MulticastDelegate<EditorDoc*> EditorDoc::on_deletion;


EditorDoc* EditorDoc::create_prefab(PrefabAsset* prefab)
{
	EditorDoc* out = new EditorDoc();
	out->init_for_prefab(prefab);
	EditorDoc::on_creation.invoke(out);
	return out;
}
EditorDoc* EditorDoc::create_scene(opt<string> scene)
{
	EditorDoc* out = new EditorDoc();
	out->init_for_scene(scene);
	EditorDoc::on_creation.invoke(out);
	return out;
}
void EditorDoc::init_for_prefab(PrefabAsset* prefab) {
	edit_category = EditCategory::EDIT_PREFAB;
	init_new();
	// marks the templated level objects as dont serialize or edit
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
	for (auto o : objs) {
		o->dont_serialize_or_edit = true;
	}
	if (prefab) {
		Entity* root = eng->get_level()->editor_spawn_prefab_but_dont_set_spawned_by(prefab);
		if (!root) {
			sys_print(Warning, "EditorDoc::init_for_prefab: prefab does not have a root, creating one.\n");
			root = spawn_entity(); 
		}
		assert(root);
		assert(root->get_object_prefab_spawn_type() == EntityPrefabSpawnType::None);
		assetName = prefab->get_name();
	}
	else {
		assetName = std::nullopt;
		auto root = spawn_entity();
	}
	assert(get_prefab_root_entity());

	validate_prefab();

	validate_fileids_before_serialize();
	on_start.invoke();
}
void EditorDoc::init_for_scene(opt<string> scene) {
	edit_category = EditCategory::EDIT_SCENE;
	init_new();
	validate_fileids_before_serialize();

	if (scene.has_value()) {
		assetName = scene.value();
	}
	else {
		assetName = std::nullopt;
	}

	on_start.invoke();
}

EditorDoc::EditorDoc() {
	assert(eng->get_level());
}


#if 0
void EditorDoc::on_map_load_return(bool good)
{
	eng->get_on_map_delegate().remove(this);	// mark the delegate to be removed

	if (good && (!get_is_open() || !eng->get_level())) {
		sys_print(Warning, "on_map_load_return but level editor not open\n");
		return;
	}

	if (!good) {
		sys_print(Warning, "failed to load editor map\n");
		PopupTemplate::create_basic_okay(
			EditorPopupManager::inst,
			"Error",
			"Couldn't load map: " + get_doc_name()
		);
		eng->open_level("__empty__");
		// this will call on_map_load_return again, sort of an infinite loop risk, but should always be valid with "__empty__"
	}
	else {
		
		if (is_editing_prefab()) {

			// marks the templated level objects as dont serialize or edit
			auto level = eng->get_level();
			auto& objs = level->get_all_objects();
			for (auto o : objs) {
				o->dont_serialize_or_edit = true;
			}

			if (!get_doc_name().empty()) {
				editing_prefab_ptr = g_assets.find_sync<PrefabAsset>(get_doc_name()).get();
				if (!editing_prefab_ptr) {
					PopupTemplate::create_basic_okay(
						EditorPopupManager::inst,
						"Error",
						"Couldn't load prefab: " + get_doc_name()
					);
					eng->log_to_fullscreen_gui(Error, "Couldnt load prefab");
					set_empty_doc();
				}
				else
					eng->get_level()->spawn_prefab(editing_prefab_ptr);
			}

			if (!editing_prefab_ptr) {
				editing_prefab_ptr = new PrefabAsset;
				g_assets.install_system_asset(editing_prefab_ptr, "!EMPTY");
			}
			ASSERT(editing_prefab_ptr);

			if (get_doc_name().empty())
				spawn_entity();// spawn empty prefab entity

			validate_prefab();
		}

		validate_fileids_before_serialize();


		on_start.invoke();
	}
}
#endif


EditorDoc::~EditorDoc() {
	// level will get unloaded in the main loop
	sys_print(Debug, "deleting map file for editor...\n");
	command_mgr->clear_all();
	on_close.invoke();

	EditorDoc::on_deletion.invoke(this);
}


ConfigVar ed_has_snap("ed_has_snap", "0", CVAR_BOOL, "");

ConfigVar ed_translation_snap("ed_translation_snap", "0.2", CVAR_FLOAT, "what editor translation snap", 0.1, 128);
ConfigVar ed_translation_snap_exp("ed_translation_snap_exp", "10", CVAR_FLOAT, "editor translation snap increment exponent", 1, 10);

ConfigVar ed_rotation_snap("ed_rotation_snap", "15.0", CVAR_FLOAT, "what editor rotation snap (degrees)", 0.1, 360);
ConfigVar ed_rotation_snap_exp("ed_rotation_snap_exp", "3", CVAR_FLOAT, "editor rotation snap increment exponent", 1, 10);

ConfigVar ed_scale_snap("ed_scale_snap", "15.0", CVAR_FLOAT, "what editor scale snap", 0.1, 360);
ConfigVar ed_scale_snap_exp("ed_scale_snap_exp", "3", CVAR_FLOAT, "editor scale snap increment exponent", 1, 10);

void ManipulateTransformTool::check_input()
{
	if (UiSystem::inst->is_game_capturing_mouse() || !ed_doc.selection_state->has_any_selected())
		return;
	if (!UiSystem::inst->is_vp_focused())
		return;
	if (UiSystem::inst->blocking_keyboard_inputs())
		return;


	const bool has_shift = Input::is_shift_down();
	if (Input::was_key_pressed(SDL_SCANCODE_R)) {

		reset_group_to_pre_transform();

		force_operation = ImGuizmo::ROTATE;

		set_force_gizmo_on(true);
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_G)) {

		reset_group_to_pre_transform();

		force_operation = ImGuizmo::TRANSLATE;

		set_force_gizmo_on(true);
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_X) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 2 | 4;
		else
			axis_mask = 1;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_Y) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 1 | 4;
		else
			axis_mask = 2;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_Z) && get_force_gizmo_on()) {
		reset_group_to_pre_transform();
		if (has_shift)
			axis_mask = 1 | 2;
		else
			axis_mask = 4;
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_S)) {
		reset_group_to_pre_transform();
		force_operation = ImGuizmo::SCALE;
		mode = ImGuizmo::LOCAL;	// local scaling only
		set_force_gizmo_on(true);
	}
}

const Entity* select_outermost_entity(const Entity* in) {
	const Entity* sel = in;
	while (sel) {
		if (sel->get_object_prefab_spawn_type() != EntityPrefabSpawnType::SpawnedByPrefab)
			break;
		sel = sel->get_parent();
	}
	return sel;
}

void EditorDoc::do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_rootmost_entity)
{
	const Entity* actual_entity_to_select = e;
	if (select_rootmost_entity) {
		actual_entity_to_select = select_outermost_entity(actual_entity_to_select);
	}
	if (is_in_eyedropper_mode()) {
		sys_print(Debug, "eyedrop!\n");
		on_eyedropper_callback.invoke(actual_entity_to_select);
		exit_eyedropper_mode();
		return;
	}
	if (e->dont_serialize_or_edit)
		return;

	ASSERT(actual_entity_to_select);
	if (action == SELECT_ONLY)
		selection_state->set_select_only_this(actual_entity_to_select);
	else if (action == ADD_SELECT)
		selection_state->add_to_entity_selection(actual_entity_to_select);
	else if (action == UNSELECT)
		selection_state->remove_from_selection(actual_entity_to_select);
}


void EditorDoc::on_mouse_pick()
{
	if (selection_state->has_any_selected() && (manipulate->is_hovered() || manipulate->is_using()))
		return;

	auto pos = Input::get_mouse_pos();
	const auto screen_pos = UiSystem::inst->get_vp_rect().get_pos();
	pos = pos - screen_pos;

	if (pos.x >= 0 && pos.y >= 0) {
		auto type = MouseSelectionAction::SELECT_ONLY;
		assert(Input::is_shift_down() == ImGui::GetIO().KeyShift);
		if (Input::is_shift_down())
			type = MouseSelectionAction::ADD_SELECT;
		else if (Input::is_ctrl_down())
			type = MouseSelectionAction::UNSELECT;

		auto handle = idraw->mouse_pick_scene_for_editor(pos.x, pos.y);
		if (handle.is_valid()) {
			auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
			if (component_ptr) {
				auto owner = component_ptr->get_owner();
				ASSERT(owner);

				do_mouse_selection(type, owner, true);
			}
		}
		else {
			exit_eyedropper_mode();	// ?
		}
	}
}

void EditorDoc::on_mouse_drag(int x, int y)
{
	
}


void EditorDoc::on_key_down(const SDL_KeyboardEvent& key)
{
	uint32_t scancode = key.keysym.scancode;
	bool has_shift = key.keysym.mod & KMOD_SHIFT;
	const float ORTHO_DIST = 20.0;
	if (scancode == SDL_SCANCODE_DELETE) {
		if (selection_state->has_any_selected()) {
			auto selected_handles = selection_state->get_selection_as_vector();
			if (!selected_handles.empty()) {
				RemoveEntitiesCommand* cmd = new RemoveEntitiesCommand(*this, selected_handles);
				command_mgr->add_command(cmd);
			}
		}
	}
	else if (scancode == SDL_SCANCODE_D && has_shift) {
		if (selection_state->has_any_selected()) {
			auto selected_handles = selection_state->get_selection_as_vector();;
			DuplicateEntitiesCommand* cmd = new DuplicateEntitiesCommand(*this, selected_handles);
			command_mgr->add_command(cmd);
		}
	}
	else if (scancode == SDL_SCANCODE_ESCAPE) {
		if (is_in_eyedropper_mode())
			exit_eyedropper_mode();
	}
	else if (scancode == SDL_SCANCODE_KP_5) {
		using_ortho = false;
	}
	else if (scancode == SDL_SCANCODE_KP_7 && key.keysym.mod & KMOD_CTRL) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, ORTHO_DIST + 50.0, 0), glm::vec3(0, -1, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_7) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, -(ORTHO_DIST + 50.0), 0), glm::vec3(0, 1, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_3 && key.keysym.mod & KMOD_CTRL) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(ORTHO_DIST, 0, 0), glm::vec3(-1, 0, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_3) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(-ORTHO_DIST, 0, 0), glm::vec3(1, 0, 0));
	}
	else if (scancode == SDL_SCANCODE_KP_1 && key.keysym.mod & KMOD_CTRL) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, 0, ORTHO_DIST), glm::vec3(0, 0, -1));
	}
	else if (scancode == SDL_SCANCODE_KP_1) {
		set_camera_target_to_sel();
		using_ortho = true;
		ortho_camera.set_position_and_front(camera.orbit_target + glm::vec3(0, 0, -ORTHO_DIST), glm::vec3(0, 0, 1));
	}
}


Bounds transform_bounds(glm::mat4 transform, Bounds b)
{
	glm::vec3 corners[8];
	corners[0] = glm::vec3(b.bmin);
	corners[1] = glm::vec3(b.bmax.x, b.bmin.y, b.bmin.z);
	corners[2] = glm::vec3(b.bmax.x, b.bmax.y, b.bmin.z);
	corners[3] = glm::vec3(b.bmin.x, b.bmax.y, b.bmin.z);

	corners[4] = glm::vec3(b.bmin.x, b.bmin.y, b.bmax.z);
	corners[5] = glm::vec3(b.bmax.x, b.bmin.y, b.bmax.z);
	corners[6] = glm::vec3(b.bmax.x, b.bmax.y, b.bmax.z);
	corners[7] = glm::vec3(b.bmin.x, b.bmax.y, b.bmax.z);
	for (int i = 0; i < 8; i++) {
		corners[i] = transform * glm::vec4(corners[i], 1.0f);
	}

	Bounds out;
	out.bmin = corners[0];
	out.bmax = corners[0];
	for (int i = 1; i < 8; i++) {
		out.bmax = glm::max(out.bmax, corners[i]);
		out.bmin = glm::min(out.bmin, corners[i]);
	}
	return out;
}

void some_funcs()
{
	//auto gedlayout = ed_doc.gui.get();
	//ImGui::DragInt2("box pos", &gedlayout->test->ls_position.x, 1.f, -1000, 1000);
	//auto& a = gedlayout->test->anchor;
	//int x[2] = { a.positions[0][0],a.positions[1][1] };
	//ImGui::SliderInt2("anchor", x, 0, 255);
	//a.positions[0][0] = x[0];
	//a.positions[0][1] = x[0];
	//a.positions[1][0] = x[1];
	//a.positions[1][1] = x[1];
}
AddToDebugMenu myfuncs("edbox test", some_funcs);
#include "Input/InputSystem.h"
void EditorDoc::tick(float dt)
{

	auto window_sz = UiSystem::inst->get_vp_rect().get_size();
	float aratio = (float)window_sz.y / window_sz.x;
	float fov = glm::radians(g_fov.get_float());




	{
		camera.orbit_mode = Input::is_mouse_down(1) || Input::last_recieved_input_from_con();// && !UiSystem::inst->is_game_capturing_mouse();

		{
			if (using_ortho && ortho_camera.can_take_input())
				ortho_camera.update_from_input(aratio);
			if (!using_ortho && camera.can_take_input())
				camera.update_from_input(window_sz.x, window_sz.y, aratio, fov);
		}

	}

	if (!using_ortho)
		vs_setup = View_Setup(camera.position, camera.front, fov, 0.01, 100.0, window_sz.x, window_sz.y);
	else {
		View_Setup vs;
		vs.far = 100.0;
		vs.front = ortho_camera.front;
		vs.origin = ortho_camera.position;
		vs.height = window_sz.y;
		vs.width = window_sz.x;
		vs.proj = ortho_camera.get_proj_matrix(aratio);
		vs.view = ortho_camera.get_view_matrix();
		vs.viewproj = vs.proj * vs.view;
		vs.near = 0.001;
		vs.fov = fov;
		vs.is_ortho = true;
		vs_setup = vs;
	}
}

bool line_plane_intersect(Ray r, glm::vec3 plane, float planed, glm::vec3& intersect)
{
	float denom = dot(plane, r.dir);

	if (abs(denom) > 0.00001) {	// such a high epsilon to deal with weird issues
		float planedist = dot(plane, r.pos) + planed;
		float time = -planedist / denom;
		intersect = r.pos + r.dir * time;
		return true;
	}
	return false;
}

glm::vec3 project_onto_line(glm::vec3 a, glm::vec3 b, glm::vec3 p)
{
	glm::vec3 ap = p - a;
	glm::vec3 ab = b - a;
	return a + dot(ap, ab) / dot(ab, ab) * ab;
}

void EditorDoc::transform_tool_update()
{
	glm::vec3 planes[3];
	float planed[3];

	planes[0] = glm::vec3(1, 0, 0);
	planes[1] = glm::vec3(0, 1, 0);
	planes[2] = glm::vec3(0, 0, 1);
	for (int i = 0; i < 3; i++) {
		planed[i] = -glm::dot(planes[i], transform_tool_origin);
	}

	// now find the intersection point
	bool xaxis = axis_bit_mask & 1;
	bool yaxis = axis_bit_mask & (1 << 1);
	bool zaxis = axis_bit_mask & (1 << 2);
	bool xandy = xaxis && yaxis;
	bool yandz = yaxis && zaxis;
	bool zandx = zaxis && xaxis;

#if  0
	Ray r;
	cast_ray_into_world(&r);	// just using this to get the unprojected ray :/
	bool good2 = true;
	glm::vec3 intersect_point = glm::vec3(0.f);
	if (xandy) {
		bool good = line_plane_intersect(r, planes[0], planed[0], intersect_point);
		if (!good) line_plane_intersect(r, planes[1], planed[1], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[2], intersect_point);
	}
	else if (yandz) {
		bool good = line_plane_intersect(r, planes[1], planed[1], intersect_point);
		if (!good) line_plane_intersect(r, planes[2], planed[2], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[0], intersect_point);
	}
	else if (zandx) {
		bool good = line_plane_intersect(r, planes[2], planed[2], intersect_point);
		if (!good) line_plane_intersect(r, planes[0], planed[0], intersect_point);
		intersect_point = project_onto_line(transform_tool_origin, transform_tool_origin + planes[1], intersect_point);
	}
	else if (xaxis) {
		good2 = line_plane_intersect(r, planes[0], planed[0], intersect_point);
	}
	else if (yaxis) {
		good2 = line_plane_intersect(r, planes[1], planed[1], intersect_point);
	}
	else if (zaxis) {
		good2 = line_plane_intersect(r, planes[2], planed[2], intersect_point);
	}
#endif

	//if(good2)
	//	selected_node->position = intersect_point;

}


uint32_t color_to_uint(Color32 c) {
	return c.r | c.g << 8 | c.b << 16 | c.a << 24;
}


#include <glm/gtc/type_ptr.hpp>
bool ManipulateTransformTool::is_hovered()
{
	return ImGuizmo::IsOver();
}
bool ManipulateTransformTool::is_using()
{
	return ImGuizmo::IsUsing();
}




ManipulateTransformTool::ManipulateTransformTool(EditorDoc& ed) : ed_doc(ed)
{
	ed_doc.post_node_changes.add(this, &ManipulateTransformTool::on_entity_changes);
	ed_doc.selection_state->on_selection_changed.add(this,
		&ManipulateTransformTool::on_selection_changed);
	ed_doc.on_close.add(this, &ManipulateTransformTool::on_close);
	ed_doc.on_start.add(this, &ManipulateTransformTool::on_open);

	ed_doc.selection_state->on_selection_changed.add(this, &ManipulateTransformTool::on_selection_changed);

	// refresh cached data
	ed_doc.prop_editor->on_property_change.add(this, &ManipulateTransformTool::on_prop_change);


}

void ManipulateTransformTool::on_close() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_open() {
	state = IDLE;
	world_space_of_selected.clear();
}
void ManipulateTransformTool::on_component_deleted(Component* ec) {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_entity_changes() {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_selection_changed() {
	stop_using_custom();

	update_pivot_and_cached();
}
void ManipulateTransformTool::on_prop_change() {

	// no stop_using_custom
	update_pivot_and_cached();
}
void ManipulateTransformTool::reset_group_to_pre_transform()
{
	for (auto& pair : world_space_of_selected) {
		EntityPtr e(pair.first);
		if (e.get()) {
			e->set_ws_transform(pair.second);
		}
	}
	update_pivot_and_cached();
}
void ManipulateTransformTool::update_pivot_and_cached()
{
	if (get_is_using_for_custom())
		return;

	world_space_of_selected.clear();
	auto& ss = ed_doc.selection_state;
	auto has_parent_in_selection_R = [&](auto&& self, Entity* e) -> bool {
		if (!e->get_parent()) return false;
		auto& sel = ss->get_selection();
		if (sel.find(e->get_parent()->get_instance_id()) != sel.end())
			return true;
		return self(self, e->get_parent());
	};

	if (ss->has_any_selected()) {
		for (auto ehandle : ss->get_selection()) {
			EntityPtr e(ehandle);
			if (e.get()) {
				const bool should_skip = has_parent_in_selection_R(has_parent_in_selection_R, e.get());
				if (!should_skip)
					world_space_of_selected[e.handle] = (e.get()->get_ws_transform());
			}
		}
	}
	static bool selectFirstOnly = true;
	if (world_space_of_selected.size() == 1 || (!world_space_of_selected.empty() && selectFirstOnly)) {
		pivot_transform = world_space_of_selected.begin()->second;
	}
	else if (world_space_of_selected.size() > 1) {
		glm::vec3 v = glm::vec3(0.f);
		for (auto s : world_space_of_selected) {
			v += glm::vec3(s.second[3]);
		}
		v /= (float)world_space_of_selected.size();
		pivot_transform = glm::translate(glm::mat4(1), v);
	}
	current_transform_of_group = pivot_transform;


	if (world_space_of_selected.size() == 0)
		state = IDLE;
	else
		state = SELECTED;

	//return;
	auto snap_to_value = [](float x, float snap) {
		return glm::round(x / snap) * snap;
	};

	glm::vec3 p, s;
	glm::quat q;
	decompose_transform(current_transform_of_group, p, q, s);
	glm::vec3 asEuler = glm::eulerAngles(q);
	//printf(": %f\n", asEuler.x);
	if (ed_has_snap.get_bool()) {
		float translation_snap = ed_translation_snap.get_float();
		p.x = snap_to_value(p.x, translation_snap);
		p.y = snap_to_value(p.y, translation_snap);
		p.z = snap_to_value(p.z, translation_snap);
	}
	current_transform_of_group = glm::translate(glm::mat4(1), p);
	current_transform_of_group = current_transform_of_group * glm::mat4_cast(glm::normalize(q));
	current_transform_of_group = glm::scale(current_transform_of_group, glm::vec3(s));

	glm::vec3 p2, s2;
	glm::quat q2;
	decompose_transform(current_transform_of_group, p2, q2, s2);
	asEuler = glm::eulerAngles(q2);
	//printf(".: %f\n", asEuler.x);
}

void ManipulateTransformTool::on_selected_tarnsform_change(uint64_t h) {
	stop_using_custom();

	update_pivot_and_cached();
}

void ManipulateTransformTool::begin_drag() {
	ASSERT(state == SELECTED);
	state = MANIPULATING_OBJS;
}

void ManipulateTransformTool::end_drag() {
	ASSERT(state == MANIPULATING_OBJS);
	if (has_any_changed) {
		auto& arr = ed_doc.selection_state->get_selection();
		ed_doc.command_mgr->add_command(new TransformCommand(ed_doc, arr, world_space_of_selected));
		has_any_changed = false;
	}
	update_pivot_and_cached();
}

// ie: snap = base
//	on_increment()
//		snap = snap * exp
// on_decrement()
//		snap = snap / mult

ConfigVar ed_force_guizmo("ed.force_guizmo", "0", CVAR_BOOL, "");



// bool force_gizmo = on/off
// G -> force gizmo on, type = translate
// R -> force gizmo on, type = rotation
// X -> if force gizmo on, set mask to x
// Y,Z ...
// mouse 1 click -> force gizmo off
// mouse 2 click -> force gizmo off, reset



void ManipulateTransformTool::update()
{
	if (state == IDLE)
		return;

	ImGuizmo::SetImGuiContext(eng->get_imgui_context());
	ImGuizmo::SetDrawlist();
	const auto s_pos = UiSystem::inst->get_vp_rect().get_pos();
	const auto s_sz = UiSystem::inst->get_vp_rect().get_size();

	ImGuizmo::SetRect(s_pos.x, s_pos.y, s_sz.x, s_sz.y);
	ImGuizmo::Enable(true);
	ImGuizmo::SetOrthographic(ed_doc.using_ortho);
	//ImGuizmo::GetStyle().TranslationLineArrowSize = 20.0;
	ImGuizmo::GetStyle().TranslationLineThickness = 6.0;
	ImGuizmo::GetStyle().RotationLineThickness = 6.0;
	ImGuizmo::GetStyle().ScaleLineThickness = 6.0;

	const auto mask_to_use = (force_gizmo_on) ? force_operation : operation_mask;

	glm::vec3 snap(-1);
	if (mask_to_use == ImGuizmo::TRANSLATE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_translation_snap.get_float());
	else if (mask_to_use == ImGuizmo::SCALE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_scale_snap.get_float());
	else if (mask_to_use == ImGuizmo::ROTATE && ed_has_snap.get_bool())
		snap = glm::vec3(ed_rotation_snap.get_float());


	auto get_real_op_mask = [](ImGuizmo::OPERATION op, int axis_mask) -> ImGuizmo::OPERATION {
		using namespace ImGuizmo;
		ImGuizmo::OPERATION out{};
		if (op == ImGuizmo::TRANSLATE) {
			if (axis_mask & 1) out = out | OPERATION::TRANSLATE_X;
			if (axis_mask & 2) out = out | OPERATION::TRANSLATE_Y;
			if (axis_mask & 4) out = out | OPERATION::TRANSLATE_Z;
		}
		else if (op == ImGuizmo::ROTATE) {
			if (axis_mask & 1) out = out | OPERATION::ROTATE_X;
			if (axis_mask & 2) out = out | OPERATION::ROTATE_Y;
			if (axis_mask & 4) out = out | OPERATION::ROTATE_Z;
			if (axis_mask == 0xff) out = OPERATION::ROTATE;
		}
		else if (op == ImGuizmo::SCALE) {
			if (axis_mask & 1) out = out | OPERATION::SCALE_X;
			if (axis_mask & 2) out = out | OPERATION::SCALE_Y;
			if (axis_mask & 4) out = out | OPERATION::SCALE_Z;
		}
		return out;
	};

	const auto window_sz = UiSystem::inst->get_vp_rect().get_size();
	const float aratio = (float)window_sz.y / window_sz.x;
	const float* const view = glm::value_ptr(ed_doc.vs_setup.view);
	const glm::mat4 friendly_proj_matrix = (ed_doc.using_ortho) ? ed_doc.ortho_camera.get_friendly_proj_matrix(aratio) : ed_doc.vs_setup.make_opengl_perspective_with_near_far();
	const float* const proj = glm::value_ptr(friendly_proj_matrix);
	float* model = glm::value_ptr(current_transform_of_group);
	ImGuizmo::SetOrthographic(ed_doc.using_ortho);
	bool good = ImGuizmo::Manipulate(get_force_gizmo_on(), view, proj, get_real_op_mask(mask_to_use, axis_mask), mode, model, nullptr, (snap.x > 0) ? &snap.x : nullptr);

	has_any_changed |= good;

	if (ImGuizmo::IsUsingAny() && state == SELECTED) {
		begin_drag();	// was not being used last frame, but now using
	}
	else if (!ImGuizmo::IsUsingAny() && state == MANIPULATING_OBJS) {
		end_drag();		// was using last frame, but now not using. this also saves off transforms into a undoable command
	}
	if (state == MANIPULATING_OBJS) {
		// save off for visible state (command is sent in end_drag)
		if (!get_is_using_for_custom()) {
			auto& ss = ed_doc.selection_state;
			auto& arr = ss->get_selection();
			for (auto elm : arr) {
				auto find = world_space_of_selected.find(elm);
				if (find == world_space_of_selected.end()) continue;	// this is valid, a parent is in the set already
				glm::mat4 ws = current_transform_of_group * glm::inverse(pivot_transform) * find->second;
				EntityPtr e(elm);
				ASSERT(e.get());
				e.get()->set_ws_transform(ws);
			}
		}
	}
}

void EditorDoc::imgui_draw()
{
	gui.draw();
	manipulate->check_input();

	if (Input::was_mouse_released(0)&&!dragger.get_is_dragging()) {

		if (UiSystem::inst->blocking_mouse_inputs()) {
			if (UiSystem::inst->is_vp_hovered()) {
				if (manipulate->get_force_gizmo_on()) {
					manipulate->set_force_gizmo_on(false);
				}
				else {
					on_mouse_pick();
				}
			}
			else
				sys_print(Warning, "vp not focused\n");
		}
		else
			sys_print(Warning, "blocked input\n");
	}
	if (Input::was_mouse_pressed(2)) {
		if (manipulate->get_force_gizmo_on()) {
			manipulate->reset_group_to_pre_transform();
			manipulate->set_force_gizmo_on(false);
		}
	}

	if (is_in_eyedropper_mode()) {
		//gui->tool_text->hidden = false;
	//gui->tool_text->text = "EYEDROPPER ACTIVE (esc to exit)";
	//gui->tool_text->color = { 255,128,128 };
	//gui->tool_text->use_desired_size = true;
	//gui->tool_text->pivot = guiAnchor::Center;
		
		TextShape shape;
		shape.text = "EYEDROPPER ACTIVE";
		shape.color = { 255,128,128 };
		shape.with_drop_shadow = true;
		// center it
		Rect2d size = GuiHelpers::calc_text_size(shape.text, nullptr);
		glm::ivec2 pos = { -size.w / 2,size.h + 5 };
		glm::ivec2 ofs = GuiHelpers::calc_layout(pos, guiAnchor::Top, UiSystem::inst->get_vp_rect());
		shape.rect = Rect2d(ofs, {});

		UiSystem::inst->window.draw(shape);

	}


	outliner->draw();

	prop_editor->draw();

	IEditorTool::imgui_draw();

	dragger.tick();

	command_mgr->execute_queued_commands();
}
void EditorDoc::hook_pre_scene_viewport_draw()
{
	auto get_icon = [](std::string str) -> ImTextureID {
		return ImTextureID(uint64_t(g_assets.find_global_sync<Texture>("eng/editor/" + str).get()->gl_id));
	};
	auto magnet_on = get_icon("magnet_on.png");
	auto magnet_off = get_icon("magnet_off.png");
	auto localcoord = get_icon("local_coord.png");
	auto globalcoord = get_icon("global_coord.png");
	auto boundingbox = get_icon("bounding_box_pivot.png");
	auto pivotcenter = get_icon("pivot_center.png");
	auto showtext_off = get_icon("show_text_off.png");
	auto showtext_on = get_icon("show_text_on.png");
	auto translate = get_icon("translate.png");
	auto cursor = get_icon("cursor.png");
	auto rotation = get_icon("rotate.png");
	auto scale = get_icon("scale.png");
	auto size = ImVec2(16, 16);
	auto push_active_style = [](bool b, bool dont_pop_prev = false) {
		if (!dont_pop_prev)
			ImGui::PopStyleColor(1);
		if (b)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5, 0.5, 0.5, 0.5));
		else
			ImGui::PushStyleColor(ImGuiCol_Button, 0);
	};
	if (ImGui::BeginMenuBar())
	{
		ImGui::PushStyleColor(ImGuiCol_Button, 0);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.5));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);

		auto optype = manipulate->get_operation_type();
		push_active_style(optype == 0, true);
		if (ImGui::ImageButton(cursor, size)) {
			manipulate->set_operation_type({});
		}
		push_active_style(optype == ImGuizmo::TRANSLATE);
		if (ImGui::ImageButton(translate, size)) {
			manipulate->set_operation_type(ImGuizmo::TRANSLATE);
		}
		push_active_style(optype == ImGuizmo::ROTATE);
		if (ImGui::ImageButton(rotation, size)) {
			manipulate->set_operation_type(ImGuizmo::ROTATE);
		}
		push_active_style(optype == ImGuizmo::SCALE);
		if (ImGui::ImageButton(scale, size)) {
			manipulate->set_operation_type(ImGuizmo::SCALE);
		}
		ImGui::Separator();

		push_active_style(ed_has_snap.get_bool());
		if (ed_has_snap.get_bool()) {
			if (ImGui::ImageButton(magnet_on, size)) {
				ed_has_snap.set_bool(false);
			}
		}
		else {
			if (ImGui::ImageButton(magnet_off, size)) {
				ed_has_snap.set_bool(true);
			}
		}

		push_active_style(false);
		auto mode = manipulate->get_mode();
		if (mode == ImGuizmo::MODE::LOCAL) {
			if (ImGui::ImageButton(localcoord, size)) {
				manipulate->set_mode(ImGuizmo::MODE::WORLD);
			}
		}
		else {
			if (ImGui::ImageButton(globalcoord, size)) {
				manipulate->set_mode(ImGuizmo::MODE::LOCAL);
			}
		}

		ImGui::ImageButton(boundingbox, size);

		auto& drawtext = editor_draw_name_text;
		push_active_style(drawtext.get_bool());
		if (drawtext.get_bool()) {
			if (ImGui::ImageButton(showtext_on, size)) {
				drawtext.set_bool(false);
			}
		}
		else {
			if (ImGui::ImageButton(showtext_off, size)) {
				drawtext.set_bool(true);
			}
		}

		ImGui::EndMenuBar();
		ImGui::PopStyleColor(4);
	}
}

void EditorDoc::hook_scene_viewport_draw()
{
	//if (get_focus_state() != editor_focus_state::Focused)
	//	return;

	if (ImGui::BeginDragDropTarget())
	{
		//const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		//if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");


		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop"))
		{
			glm::mat4 drop_transform = glm::mat4(1.f);

			int x, y;
			SDL_GetMouseState(&x, &y);
			auto size = UiSystem::inst->get_vp_rect().get_pos();
			const float scene_depth = idraw->get_scene_depth_for_editor(x - size.x, y - size.y);

			glm::vec3 dir = unproject_mouse_to_ray(x, y);
			glm::vec3 worldpos = (abs(scene_depth) > 50.0) ? vs_setup.origin - dir * 25.0f : vs_setup.origin + dir * scene_depth;
			drop_transform[3] = glm::vec4(worldpos, 1.0);

			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;

			if (resource->type->get_asset_class_type()->is_a(Entity::StaticType)) {
				command_mgr->add_command(new CreateCppClassCommand(*this,
					resource->filename,
					drop_transform, EntityPtr(), false)
				);
			}
			else if (resource->type->get_asset_class_type()->is_a(Model::StaticType)) {
				command_mgr->add_command(new CreateStaticMeshCommand(*this,
					resource->filename,
					drop_transform)
				);
			}
			else if (resource->type->get_asset_class_type()->is_a(Component::StaticType)) {
				EntityPtr parent_to;
				{
					const ClassTypeInfo* type = ClassBase::find_class(resource->filename.c_str());
					
				}
				command_mgr->add_command(new CreateCppClassCommand(*this,
					resource->filename,
					drop_transform,
					parent_to, true)
				);
			}
			else if (resource->type->get_asset_class_type()->is_a(PrefabAsset::StaticType)) {
				command_mgr->add_command(new CreatePrefabCommand(*this,
					resource->filename,
					drop_transform
				));
			}

		}
		ImGui::EndDragDropTarget();
	}

	manipulate->update();


}

#include "EditorPopups.h"


int imgui_std_string_resize(ImGuiInputTextCallbackData* data)
{
	std::string* user = (std::string*)data->UserData;
	assert(user);

	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize) {
		user->resize(data->BufSize);
		data->Buf = (char*)user->data();
	}

	return 0;
}
bool std_string_input_text(const char* label, std::string& str, int flags)
{
	return ImGui::InputText(label, (char*)str.c_str(), str.size() + 1, flags | ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &str);
}



void EdPropertyGrid::draw_components(Entity* entity)
{
	ASSERT(selected_component != 0);
	ASSERT(eng->get_object(selected_component)->is_a<Component>());
	ASSERT(eng->get_object(selected_component)->cast_to<Component>()->entity_owner == entity);

	auto draw_component = [&](Entity* e, Component* ec) {
		ASSERT(ec && e && ec->get_owner() == e);

		ImGui::TableNextRow();
		ImGui::TableNextColumn();

		ImGui::PushID(ec);

		ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;
		if (ImGui::Selectable("##selectednode", ec->get_instance_id() == selected_component, selectable_flags, ImVec2(0, 0))) {
			on_select_component(ec);
		}

		if (ImGui::IsItemHovered() && ImGui::GetIO().MouseClicked[1]) {
			ImGui::OpenPopup("outliner_ctx_menu");
			on_select_component(ec);
			component_context_menu = ec->get_instance_id();
		}
		if (ImGui::BeginPopup("outliner_ctx_menu")) {

			if (eng->get_object(component_context_menu) == nullptr) {
				component_context_menu = 0;
				ImGui::CloseCurrentPopup();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, color32_to_imvec4({ 255,50,50,255 }));
				if (ImGui::MenuItem("Remove (warning: no undo)")) {

					auto ec_ = eng->get_object(component_context_menu)->cast_to<Component>();
					if (ed_doc.is_this_object_not_inherited(ec_)) {
						ed_doc.command_mgr->add_command(new RemoveComponentCommand(ed_doc, ec_->get_owner(), ec_));
					}
					else
						eng->log_to_fullscreen_gui(Error, "Cant remove inherited components");

					component_context_menu = 0;
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor(1);
			}

			ImGui::EndPopup();
		}


		ImGui::SameLine();
		ImGui::Dummy(ImVec2(5.f, 1.0));
		ImGui::SameLine();

		const char* s = ec->get_editor_outliner_icon();
		if (*s) {
			auto tex = g_assets.find_global_sync<Texture>(s);
			if (tex) {
				ImGui::Image(ImTextureID(uint64_t(tex->gl_id)), ImVec2(tex->width, tex->height));
				ImGui::SameLine(0, 0);
			}
		}

		if (!ed_doc.is_this_object_not_inherited(ec))
			ImGui::TextColored(non_owner_source_color, ec->get_type().classname);
		else
			ImGui::Text(ec->get_type().classname);
		ImGui::PopID();
	};

	for (auto& c : entity->get_components())
		if (!c->dont_serialize_or_edit)
			draw_component(entity, c);
}
#include "Framework/StringUtils.h"

void EdPropertyGrid::draw()
{
	auto& ss = ed_doc.selection_state;
	if (ImGui::Begin("Properties")) {
		if (ed_doc.selection_state->has_only_one_selected()) {
			grid.update();

			if (grid.rows_had_changes) {

				auto e = ss->get_only_one_selected();
				e->editor_on_change_properties();
				e->post_change_transform_R();

				auto ec = get_selected_component();
				if (ec)
					ec->editor_on_change_property();

				on_property_change.invoke();

			}

		}
		else {
			ImGui::Text("Nothing selected\n");
		}
	}

	ImGui::End();

	if (ImGui::Begin("Components")) {

		if (!ss->has_any_selected()) {
			ImGui::Text("Nothing selected\n");
			selected_component = 0;
		}
		else if (!ss->has_only_one_selected()) {
			ImGui::Text("Select 1 entity to see components\n");
			selected_component = 0;
		}
		else if (ss->get_only_one_selected().get() && !serialize_this_objects_children(ss->get_only_one_selected().get())) {
			ImGui::Text("Prefab instance is not editable.\nMake it editable through the context menu.");
			selected_component = 0;
		}
		else {

			Entity* ent = ss->get_only_one_selected().get();

			if (ImGui::Button("Add Component")) {
				ImGui::OpenPopup("addcomponentpopup");
			}
			ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(300, 500));
			if (ImGui::BeginPopup("addcomponentpopup")) {
				if (component_set_keyboard_focus) {
					ImGui::SetKeyboardFocusHere();
					component_set_keyboard_focus = false;
				}
				if (ImGui::InputText("##text", (char*)component_filter.c_str(), component_filter.size() + 1, ImGuiInputTextFlags_CallbackResize, imgui_std_string_resize, &component_filter)) {
					component_filter = component_filter.c_str();
				}
				const string filter_lower = StringUtils::to_lower(component_filter);
				auto iter = ClassBase::get_subclasses<Component>();
				for (; !iter.is_end(); iter.next()) {
					string lower = StringUtils::to_lower(iter.get_type()->classname);
					if (component_filter.empty()||lower.find(filter_lower) != string::npos) {
						if (iter.get_type()->default_class_object) {
							const char* s = ((Component*)iter.get_type()->default_class_object)->get_editor_outliner_icon();

							if (*s) {
								auto tex = g_assets.find_global_sync<Texture>(s);
								if (tex) {
									ImGui::Image(ImTextureID(uint64_t(tex->gl_id)), ImVec2(tex->width, tex->height));
									ImGui::SameLine(0, 0);
								}
							}
						}
						if (ImGui::Selectable(iter.get_type()->classname)) {

							ed_doc.command_mgr->add_command(new CreateComponentCommand(ed_doc,
								ent, iter.get_type()
							));

							ImGui::CloseCurrentPopup();
						}
					}
				}
				ImGui::EndPopup();
			}
			else {
				component_set_keyboard_focus = true;
				component_filter.clear();
			}


			auto& comps = ent->get_components();
			{
				if (selected_component == 0 && comps.size() > 0)
					selected_component = comps[0]->get_instance_id();

				uint32_t ent_list_flags = ImGuiTableFlags_PadOuterX | ImGuiTableFlags_Borders |
					ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable;
				if (ImGui::BeginTable("animadfedBrowserlist", 1, ent_list_flags))
				{


					ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);

					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, Color32{ 59, 0, 135 }.to_uint());
					ImGuiSelectableFlags selectable_flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap;

					ImGui::SameLine();
					ImGui::Text(ent->get_type().classname);

					if (comps.size() > 0)
						draw_components(ent);

					ImGui::EndTable();

					if (ImGui::BeginDragDropTarget())
					{
						const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly);
						if (payload) {

							auto component_metadata = AssetRegistrySystem::get().find_for_classtype(&Component::StaticType);
							auto mesh_metadata = AssetRegistrySystem::get().find_for_classtype(&Model::StaticType);


							AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;
							bool actually_accept = false;
							auto type = resource->type;
							if (type == component_metadata || type == mesh_metadata) {
								actually_accept = true;
							}

							if (actually_accept) {
								if ((payload = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop")))
								{
									Entity* ent = ss->get_only_one_selected().get();
									ASSERT(ent);
									if (type == component_metadata) {
										auto comp_type = ClassBase::find_class(resource->filename.c_str());
										if (comp_type && comp_type->is_a(Component::StaticType)) {
											ed_doc.command_mgr->add_command(
												new CreateComponentCommand(ed_doc, ent, comp_type)
											);
										}
									}
									else if (type == mesh_metadata) {
										ed_doc.command_mgr->add_command(
											new CreateMeshComponentCommand(ed_doc, ent, g_assets.find_sync<Model>(resource->filename).get()));
									}
								}
							}
						}
						ImGui::EndDragDropTarget();
					}

				}
			}
		}


	}
	ImGui::End();
}






EdPropertyGrid::EdPropertyGrid(EditorDoc& ed_doc, const FnFactory<IPropertyEditor>& factory) : ed_doc(ed_doc), factory(factory), grid(factory)
{
	auto& ss = ed_doc.selection_state;
	ss->on_selection_changed.add(this, &EdPropertyGrid::refresh_grid);
	ed_doc.on_close.add(this, &EdPropertyGrid::on_close);
	ed_doc.on_component_deleted.add(this, &EdPropertyGrid::on_ec_deleted);
	ed_doc.on_component_created.add(this, &EdPropertyGrid::on_select_component);

}

void EdPropertyGrid::refresh_grid()
{
	grid.clear_all();

	auto& ss = ed_doc.selection_state;

	if (!ss->has_any_selected())
		return;

	if (ss->has_only_one_selected()) {
		auto entity = ss->get_only_one_selected();
		assert(entity);
		sys_print(Debug,"EdPropertyGrid::refresh_grid: adding to grid: %s\n", entity->get_type().classname);

		auto ti = &entity->get_type();
		while (ti) {
			if (ti->props) {
				grid.add_property_list_to_grid(ti->props, entity.get());
			}
			ti = ti->super_typeinfo;
		}


		auto& comps = entity->get_components();

		if (!comps.empty() && serialize_this_objects_children(entity.get())) {
			if (selected_component == 0)
				selected_component = comps[0]->get_instance_id();
			if (eng->get_object(selected_component) == nullptr || eng->get_object(selected_component)->cast_to<Component>() == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>()->get_owner() != entity.get())
				selected_component = comps[0]->get_instance_id();

			ASSERT(selected_component != 0);


			auto c = eng->get_object(selected_component)->cast_to<Component>();
			sys_print(Debug, "EdPropertyGrid::refresh_grid: adding to grid: %s\n", c->get_type().classname);

			ASSERT(c);
			ti = &c->get_type();
			while (ti) {
				if (ti->props)
					grid.add_property_list_to_grid(ti->props, c);
				ti = ti->super_typeinfo;
			}
		}
	}
}


SelectionState::SelectionState(EditorDoc& ed_doc)
{
	ed_doc.post_node_changes.add(this, &SelectionState::on_node_deleted);
	ed_doc.on_close.add(this, &SelectionState::on_close);
}


#if 0
DECLARE_ENGINE_CMD(STRESS_TEST)
{
	static int counter = 0;
	const int size = 10;
	auto model = g_assets.find_sync<Model>("wall2x2.cmdl");
	for (int z = 0; z < size; z++) {
		for (int y = 0; y < size; y++) {
			for (int x = 0; x < size; x++) {
				glm::vec3 p(x, y, z + counter * size);
				glm::mat4 transform = glm::translate(glm::mat4(1), p * 2.0f);

				auto ent = eng->get_level()->spawn_entity();
				ent->create_component<MeshComponent>()->set_model(model.get());
				ent->set_ws_transform(transform);

			}
		}
	}
	counter++;
}
#endif

#include "PropertyEditors.h"
void EditorDoc::set_camera_target_to_sel()
{
	if(selection_state->has_only_one_selected()) {
		auto ptr = selection_state->get_only_one_selected();
		if (ptr) {
			float radius = 1.f;
			auto mesh = ptr->get_component<MeshComponent>();
			if (mesh && mesh->get_model()) {
				radius = glm::max(mesh->get_model()->get_bounding_sphere().w, 0.5f);
			}
			auto pos = ptr->get_ws_position();
			camera.set_orbit_target(pos, radius);
		}
	}
}

extern void export_scene_model();

void EditorDoc::hook_menu_bar()
{
	if (ImGui::BeginMenu("Plugins")) {

		if (ImGui::MenuItem("<none>")) {
			set_plugin(nullptr);
		}

		//auto iter = ClassBase::get_subclasses<LEPlugin>();
		//for (; !iter.is_end(); iter.next()) {
		//	auto type = iter.get_type();
		//	if (ImGui::MenuItem(type->classname)) {
		//		set_plugin(type);
		//	}
		//
		//}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Commands")) {
		if (ImGui::MenuItem("Export as .glb")) {
			export_scene_model();
		}
		ImGui::EndMenu();
	}
}

EditorUILayout::EditorUILayout() {

}
#include "UI/Widgets/EditorCube.h"


#include "UI/UILoader.h"
ConfigVar test1("test1", "200", CVAR_INTEGER, "", 0, 256);
ConfigVar test2("test2", "200", CVAR_INTEGER, "", 0, 256);

void EditorUILayout::draw() {
	RenderWindow& window = UiSystem::inst->window;
	cube.rotation_matrix = (glm::mat3)doc->vs_setup.view;
	cube.draw(window);
	// paint
	if (doc->dragger.get_is_dragging()) {
		auto rect = doc->dragger.get_drag_rect();
		//builder.draw_solid_rect({ rect.x,rect.y }, { rect.w,rect.h }, { 200,200,200,50 });
		rect.x -= UiSystem::inst->get_vp_rect().get_pos().x;
		rect.y -= UiSystem::inst->get_vp_rect().get_pos().y;

		RectangleShape shape;
		shape.rect = rect;
		Uint8 c = test1.get_integer();
		Uint8 a = test2.get_integer();
		shape.color = { c,c,c,a };
		window.draw(shape);
	}


	int x, y;
	SDL_GetMouseState(&x, &y);
	bool do_mouse_click = mouse_clicked && button_clicked == 1;
	mouse_clicked = false;

	if (doc->selection_state->has_any_selected() && (doc->manipulate->is_hovered() || doc->manipulate->is_using()))
		do_mouse_click = false;

	if (!editor_draw_name_text.get_bool())
		return;
	if (!eng->get_level())
		return;

	const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	if (!font) 
		font = UiSystem::inst->defaultFont;

	struct obj {
		glm::vec3 pos = glm::vec3(0.f);
		const Entity* e = nullptr;
	};
	std::vector<obj> objs;
	auto& all_objs = eng->get_level()->get_all_objects();
	for (auto o : all_objs) {
		if (Entity* e = o->cast_to<Entity>()) {
			if (!this_is_a_serializeable_object(e))
				continue;
			obj ob;
			glm::vec3 todir = glm::vec3(e->get_ws_position()) - doc->vs_setup.origin;
			float dist = glm::dot(todir, todir);
			if (dist > 20.0 * 20.0)
				continue;
			ob.e = e;
			glm::vec4 pos = doc->vs_setup.viewproj * glm::vec4(e->get_ws_position(), 1.0);
			ob.pos = pos / pos.w;

			if (ob.pos.z < 0)
				continue;

			objs.push_back(ob);
		}
	}
	std::sort(objs.begin(), objs.end(), [](const obj& a, const obj& b)->bool
		{
			return a.pos.z < b.pos.z;
		});
	const Entity* clicked = nullptr;
	for (auto o : objs) {
		const char* name = (o.e->get_editor_name().c_str());
		const bool is_prefab_root = o.e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
		if (!*name) {
			if (is_prefab_root)
				name = o.e->get_object_prefab().get_name().c_str();
			else {
				if (auto m = o.e->get_component<MeshComponent>()) {
					if (m->get_model())
						name = m->get_model()->get_name().c_str();
				}
			}
		}
		if (!*name) {
			name = o.e->get_type().classname;
		}

		const int icon_size = 16;
		InlineVec<Texture*, 6> icons;
		auto e = o.e;
		if (is_prefab_root) {
			const char* s = "eng/editor/prefab_p.png";
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		for (auto c : o.e->get_components()) {
			if (c->dont_serialize_or_edit_this()) continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s) continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		auto size = GuiHelpers::calc_text_size_no_wrap(name, font);

		const int text_offset = (icon_size + 1) * icons.size();
		size.w += text_offset;


		o.pos.y *= -1;
		auto coordx = o.pos.x * 0.5 + 0.5;
		auto coordy = o.pos.y * 0.5 + 0.5;

		const auto vp_size = UiSystem::inst->get_vp_rect().get_size();
		const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();


		coordx *= vp_size.x;
		coordy *= vp_size.y;
		//coordx += vp_pos.x;
		//coordy += vp_pos.y;
		coordx -= size.w / 2;
		coordy -= size.h / 2;

		Color32 color = { 50,50,50,(uint8_t)editor_draw_name_text_alpha.get_integer() };
		if (o.e->get_selected_in_editor())
			color = { 255,180,0,150 };

		if (do_mouse_click) {
			Rect2d r(coordx - 3, coordy - 3, size.w + 6, size.h + 6);
			if (r.is_point_inside(x, y)) {

				clicked = o.e;
			}
		}
		glm::ivec2 textofs = { 0,font->base };

		RectangleShape shape;
		shape.rect = Rect2d({ coordx - 3,coordy - 3 }, { size.w + 6,size.h + 6 });
		shape.color = color;
		window.draw(shape);

		//builder.draw_solid_rect({ coordx - 3,coordy - 3 }, { size.w + 6,size.h + 6 }, color);
		for (int i = 0; i < icons.size(); i++) {
			const int ofs = (i) * (icon_size + 1);

			shape.rect = Rect2d({ coordx + ofs,coordy }, { icon_size,icon_size });
			shape.texture = icons[i];
			shape.color = COLOR_WHITE;
			window.draw(shape);
		}

		TextShape tshape;
		tshape.rect = Rect2d(glm::ivec2{ coordx + 1 + text_offset,coordy + 1 } + textofs, {});
		tshape.font = font;
		tshape.text = name;
		tshape.color = COLOR_BLACK;
		window.draw(shape);
		tshape.rect = Rect2d(glm::ivec2{ coordx + text_offset,coordy } + textofs, {});
		tshape.color = COLOR_WHITE;
		window.draw(tshape);

	}
	if (clicked) {
		if (ImGui::GetIO().KeyShift) {
			doc->do_mouse_selection(EditorDoc::MouseSelectionAction::ADD_SELECT, clicked, true);
		}
		else if (ImGui::GetIO().KeyCtrl) {
			doc->do_mouse_selection(EditorDoc::MouseSelectionAction::UNSELECT, clicked, true);
		}
		else {
			doc->do_mouse_selection(EditorDoc::MouseSelectionAction::SELECT_ONLY, clicked, true);
		}
	}
	else {
		//if(do_mouse_click)
		//	mouse_down_delegate.invoke(x-ws_position.x, y-ws_position.y, button_clicked);
	}

}


#endif

Entity* EditorDoc::spawn_entity()
{
	Entity* e = eng->get_level()->spawn_entity();
	instantiate_into_scene(e);
	return e;
}

Component* EditorDoc::attach_component(const ClassTypeInfo* ti, Entity* e)
{
	Component* c = e->create_component_type(ti);
	instantiate_into_scene(c);
	return c;
}

void EditorDoc::remove_scene_object(BaseUpdater* u)
{
	u->destroy_deferred();
}

void EditorDoc::insert_unserialized_into_scene(UnserializedSceneFile& file, SerializedSceneFile* scene)
{
	if (!scene) {	// means assign new ids
		for (auto& [path, e] : file.file_id_to_obj) {
			if(!is_this_object_inherited(e))
				e->unique_file_id = get_next_file_id();
			else {
				auto as_ent = e->cast_to<Entity>();
				assert(!as_ent || as_ent->get_object_prefab_spawn_type() == EntityPrefabSpawnType::SpawnedByPrefab);
			}
		}
	}
	eng->get_level()->insert_unserialized_entities_into_level(file, scene);
}

void EditorDoc::instantiate_into_scene(BaseUpdater* u)
{
	u->unique_file_id = get_next_file_id();
	if (auto ent = u->cast_to<Entity>()) {
		assert(ent->get_object_prefab_spawn_type() != EntityPrefabSpawnType::SpawnedByPrefab);
	}
}

Entity* EditorDoc::spawn_prefab(PrefabAsset* prefab)
{
	if (!prefab)
		return nullptr;
	Entity* e = eng->get_level()->spawn_prefab(prefab);
	if (!e)
		return nullptr;
	e->unique_file_id = get_next_file_id();
	assert(e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab);
	return e;
}

void DragDetector::tick()
{
	if (Input::is_mouse_down(0)) {
		if (!is_dragging) {
			mouseClickX = Input::get_mouse_pos().x;
			mouseClickY = Input::get_mouse_pos().y;
			is_dragging = true;
		}
	}
	else {
		if (is_dragging) {
			if(get_is_dragging())
				on_drag_end.invoke(get_drag_rect());
			is_dragging = false;
			mouseClickX = 0;
			mouseClickY = 0;
		}
	}
}

bool DragDetector::get_is_dragging() const
{
	if (!is_dragging) return false;
	auto rect = get_drag_rect();
	if (rect.w >= 2 || rect.h >= 2) {
		return true;
	}
	return false;
}

Rect2d DragDetector::get_drag_rect() const
{
	auto pos = Input::get_mouse_pos();
	glm::ivec2 clickPos = { mouseClickX,mouseClickY };
	Rect2d rect;
	auto minP = glm::min(clickPos, pos);
	auto maxP = glm::max(clickPos, pos);

	rect.x = minP.x;
	rect.y = minP.y;
	rect.w = maxP.x - minP.x;
	rect.h = maxP.y - minP.y;

	return rect;
}
