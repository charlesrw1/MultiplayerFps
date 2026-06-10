// Core EditorDoc lifecycle: init, save, load, ctor/dtor, document path, eyedropper.
// Input handling -> EditorDocInput.cpp
// Viewport/UI drawing -> EditorDocViewport.cpp
// Scene object management + API impls -> EditorDocScene.cpp
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
#include "AssetCompile/Someutils.h" // string stuff
#include "Assets/AssetRegistry.h"
#include "UI/Widgets/Layouts.h"
#include "UI/GUISystemPublic.h"
#include "Game/LevelAssets.h"
#include "LevelEditor/Commands.h"
#include "Framework/Rect2d.h"
#include "Framework/AddClassToFactory.h"
#include "Game/EntityComponent.h"
#include "Scripting/ScriptManager.h"
#include "UI/UIBuilder.h"
#include "PropertyEditors.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "EditorPopupTemplate.h"
#include "Framework/StringUtils.h"
#include "UI/Widgets/EditorCube.h"
#include "UI/UILoader.h"
#include "Game/Components/LightComponents.h"
#include "EditorPopups.h"
#include "Render/RenderConfigVars.h"
#include <glm/gtc/type_ptr.hpp>
#include "Input/InputSystem.h"
#include "Game/Components/DecalComponent.h"
#include "Framework/PropertyEd.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Game/Components/PrefabAssetComponent.h"
#include "Framework/SerializerJson2.h"
#include "ObjectOutlineFilter.h"
#include "Animation/SkeletonData.h"
#include "Game/Components/BillboardComponent.h"

// ---------------------------------------------------------------------------
// Global delegates and config vars
// ---------------------------------------------------------------------------

MulticastDelegate<EditorDoc*> EditorDoc::on_creation;
MulticastDelegate<EditorDoc*> EditorDoc::on_deletion;

ConfigVar g_editor_newmap_template("g_editor_newmap_template", "eng/template_map.tmap", CVAR_DEV,
								   "whenever a new map is created, it will use this map as a template");

ConfigVar ed_has_snap("ed_has_snap", "0", CVAR_BOOL, "");
ConfigVar ed_translation_snap("ed_translation_snap", "0.2", CVAR_FLOAT, "what editor translation snap", 0.1, 128);
ConfigVar ed_translation_snap_exp("ed_translation_snap_exp", "10", CVAR_FLOAT,
								  "editor translation snap increment exponent", 1, 10);
ConfigVar ed_rotation_snap("ed_rotation_snap", "15.0", CVAR_FLOAT, "what editor rotation snap (degrees)", 0.1, 360);
ConfigVar ed_rotation_snap_exp("ed_rotation_snap_exp", "3", CVAR_FLOAT, "editor rotation snap increment exponent", 1,
							   10);
ConfigVar ed_scale_snap("ed_scale_snap", "1.0", CVAR_FLOAT, "what editor scale snap", 0.1, 360);
ConfigVar ed_scale_snap_exp("ed_scale_snap_exp", "3", CVAR_FLOAT, "editor scale snap increment exponent", 1, 10);
ConfigVar ed_force_guizmo("ed.force_guizmo", "0", CVAR_BOOL, "");

ConfigVar ed_show_box_handles("ed.show_box_handles", "0", CVAR_BOOL, "");

extern void export_godot_scene(const std::string& base_export_path);
extern void export_level_scene();
extern void start_play_process();
extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

// ---------------------------------------------------------------------------
// Utility helpers
// ---------------------------------------------------------------------------

static std::string to_string(StringView view) {
	ASSERT(view.str_start || view.str_len == 0);
	return std::string(view.str_start, view.str_len);
}

Color32 to_color32(glm::vec4 v) {
	ASSERT(v.a >= 0.f && v.a <= 1.f);
	Color32 c;
	c.r = glm::clamp(v.r * 255.f, 0.f, 255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}

string get_name_display_entity(const Entity* e) {
	ASSERT(e);
	string name = (e->get_editor_name().c_str());
	if (name.empty()) {
		if (auto prefab = e->get_component<PrefabAssetComponent>()) {
			// Extract filename from path (e.g., "Prefabs/my_prefab.tprefab" -> "my_prefab.tprefab")
			string prefab_path = prefab->prefab_path;
			size_t last_slash = prefab_path.find_last_of("/\\");
			if (last_slash != string::npos) {
				name = prefab_path.substr(last_slash + 1);
			} else {
				name = prefab_path;
			}
		} else {
			if (auto m = e->get_component<MeshComponent>()) {
				if (m->get_model())
					name = m->get_model()->get_name();
			}
			if (auto spawner = e->get_component<SpawnerComponent>()) {
				name = spawner->get_spawner_type();
			}
		}
	}
	if (name.empty()) {
		name = e->get_type().classname;
	}
	return name;
}

// ---------------------------------------------------------------------------
// Ray / unproject
// ---------------------------------------------------------------------------

Ray EditorDoc::unproject_mouse_to_ray(const int mx, const int my) {
	ASSERT(mx >= 0 || mx < 0); // no range restriction
	return ed_cam.unproject_mouse(mx, my);
}

Bounds transform_bounds(glm::mat4 transform, Bounds b);

// ---------------------------------------------------------------------------
// Validate file IDs
// ---------------------------------------------------------------------------

void EditorDoc::validate_fileids_before_serialize() {
	ASSERT(eng->get_level());
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
}

// ---------------------------------------------------------------------------
// Init / setup
// ---------------------------------------------------------------------------

void EditorDoc::init_new() {
	ASSERT(eng->get_level());
	clear_editor_changes();

	sys_print(Debug, "Edit mode: %s", "Scene");
	eng->get_level()->validate();
	command_mgr = std::make_unique<UndoRedoSystem>();

	command_mgr->on_command_execute_or_undo.add(this, [&]() { set_has_editor_changes(); });

	// Initialize API implementations
	selection_state = std::make_unique<SelectionState>(*this);
	sel_api_impl = std::make_unique<SelectionApiImpl>(selection_state.get());
	cam_api_impl = std::make_unique<CameraApiImpl>(&ed_cam);
	doc_api_impl = std::make_unique<DocumentApiImpl>(this);
	editor_api2 = std::make_unique<EditorApi2Impl>(cam_api_impl.get(), sel_api_impl.get(), doc_api_impl.get());

	gui = std::make_unique<EditorUILayout>(*editor_api2);
	prop_editor = std::make_unique<EdPropertyGrid>(grid_factory);

	// Connect EdPropertyGrid callbacks to EditorDoc events
	selection_state->on_selection_changed.add(prop_editor.get(),
											  [prop_ed = prop_editor.get(), api = sel_api_impl.get()]() {
												  if (api) {
													  prop_ed->refresh_grid(*api);
												  }
											  });
	post_node_changes.add(prop_editor.get(), [prop_ed = prop_editor.get(), api = sel_api_impl.get()]() {
		if (api) {
			prop_ed->refresh_grid(*api);
		}
	});

	// Hot-reloading a Lua class reallocates LuaClassTypeInfo::lua_props_storage and
	// rewrites ti->props, freeing the PropertyInfo entries the cached grid rows point
	// at. Drop the cache so the next draw rebuilds against the new layout.
	if (ScriptManager::inst) {
		ScriptManager::inst->on_class_reloaded.add(prop_editor.get(),
												   [prop_ed = prop_editor.get()]() { prop_ed->invalidate_cache(); });
	}

	manipulate = std::make_unique<ManipulateTransformTool>(*this);
	drag_drop_preview = std::make_unique<DragDropPreview>();
	foliage_tool = std::make_unique<FoliagePaintTool>(*this);
	stamp_tool = std::make_unique<DecalStampTool>(*this);
	road_tool = std::make_unique<RoadBuilderTool>(*this);
	handle_dragger = std::make_unique<EViewportHandles>(*this);
	selection_mode = std::make_unique<SelectionMode>(*this);
	draw_handles = std::make_unique<DrawHandlesObject>(*this);
	PropertyFactoryUtil::register_basic(grid_factory);
	PropertyFactoryUtil::register_editor(*this, grid_factory);

	cmds = ConsoleCmdGroup::create("");
	add_editor_commands();

	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  editor.ini");

	assert(eng->get_level());
}

// ---------------------------------------------------------------------------
// Document path and naming
// ---------------------------------------------------------------------------

void EditorDoc::set_document_path(string newAssetName) {
	ASSERT(!newAssetName.empty());
	if (newAssetName.empty()) {
		sys_print(Warning, "set_document_path: empty path\n");
		return;
	}
	if (assetName.has_value()) {
		sys_print(Warning, "EditorDoc::set_document_path: already has path\n");
		// return;
	}
	this->assetName = newAssetName;
}

string EditorDoc::get_name() {
	ASSERT(true); // always valid
	string name = get_doc_name();
	if (name.empty())
		name = "<unnamed>";
	return "Scene: " + name;
}

// ---------------------------------------------------------------------------
// Save
// ---------------------------------------------------------------------------

bool EditorDoc::save_document_internal() {
	ASSERT(eng->get_level());
	if (assetName.has_value() && assetName.value().empty()) {
		sys_print(Warning, "EditorDoc::save_document_internal has an empty name?\n");
		assetName = std::nullopt;
	}
	if (!assetName.has_value()) {
		PopupTemplate::create_file_save_as(
			EditorPopupManager::inst,
			[&](string path) {
				sys_print(Debug, "EditorDoc::save_document_internal: popup returned with path %s\n", path.c_str());
				this->set_document_path(path);
				save_document_internal();
			},
			get_save_file_extension());
		sys_print(Debug, "EditorDoc::save_document_internal: no path to save, so adding popup\n");
		return false;
	}

	assert(eng->get_level());
	eng->log_to_fullscreen_gui(Info, "Saving");
	sys_print(Info, "Saving Scene/Prefab (%s)...\n", assetName.value_or("<new>").c_str());
	auto& all_objs = eng->get_level()->get_all_objects();
	validate_fileids_before_serialize();
	std::vector<Entity*> all_ents;
	for (auto o : all_objs)
		if (auto e = o->cast_to<Entity>())
			all_ents.push_back(e);
	string debug_tag = "saving:" + assetName.value_or("<new>");
	auto serialized = NewSerialization::serialize_to_text(debug_tag.c_str(), all_ents, false, nullptr,
														  &eng->get_level()->preserved_unknown_objs);
	assert(assetName.has_value());
	const string path = assetName.value();

	auto outfile = FileSys::open_write_game(path.c_str());
	if (!outfile) {
		sys_print(Error, "EditorDoc::save_document_internal: couldnt write to output file! Writing recovery file.\n");
		string recovery_path = "recovery_" + StringUtils::alphanumeric_hash(assetName.value());
		outfile = FileSys::open_write_game(recovery_path.c_str());
		if (!outfile) {
			sys_print(Error, "EditorDoc::save_document_internal: couldnt write recovery file :(\n");
		} else {
			sys_print(Info, "Writing recovery file for %s: %s\n", assetName.value().c_str(), recovery_path.c_str());
			outfile->write(serialized.text.c_str(), serialized.text.size());
		}
	} else {
		outfile->write(serialized.text.c_str(), serialized.text.size());
		sys_print(Info, "Wrote Map/Prefab out to %s\n", path.c_str());
		outfile->close();
	}

	clear_editor_changes();
	set_window_title();

	return true;
}

// ---------------------------------------------------------------------------
// Eyedropper
// ---------------------------------------------------------------------------

void EditorDoc::enable_entity_eyedropper_mode(void* id) {
	ASSERT(id);
	eng->log_to_fullscreen_gui(Debug, "entering eyedropper mode...");
	active_eyedropper_user_id = id;
	eye_dropper_active = true;
}

void EditorDoc::exit_eyedropper_mode() {
	ASSERT(true);
	if (is_in_eyedropper_mode()) {
		eng->log_to_fullscreen_gui(Debug, "exiting eyedropper");
		eye_dropper_active = false;
		active_eyedropper_user_id = nullptr;
	}
}

// ---------------------------------------------------------------------------
// Scene creation entry point
// ---------------------------------------------------------------------------

EditorDoc* EditorDoc::create_scene(opt<string> scene) {
	ASSERT(eng);
	EditorDoc* out = new EditorDoc();
	out->init_for_scene(scene);
	EditorDoc::on_creation.invoke(out);
	return out;
}

void EditorDoc::init_for_scene(opt<string> scene) {
	ASSERT(eng->get_level());
	init_new();
	validate_fileids_before_serialize();

	if (scene.has_value()) {
		assetName = scene.value();
		// Detect if editing a prefab (.tprefab file)
		const auto& name = scene.value();
		if (name.size() > 8 && name.substr(name.size() - 8) == ".tprefab") {
			editing_prefab = true;

			// Save the prefab content before clearing the level
			std::vector<Entity*> prefab_entities;
			auto& all_objects = eng->get_level()->get_all_objects();
			for (auto obj : all_objects) {
				if (auto entity = obj->cast_to<Entity>()) {
					prefab_entities.push_back(entity);
				}
			}
			SerializedSceneFile prefab_content;
			if (!prefab_entities.empty()) {
				try {
					prefab_content = NewSerialization::serialize_to_text("prefab_edit_backup", prefab_entities, false);
				}
				catch (const std::exception& e) {
					sys_print(Warning, "Failed to backup prefab content: %s\n", e.what());
				}
			}

			// Load the template level (clears current level). If template.tmap can't be
			// loaded, load_level returns false and leaves the level untouched — fall back to
			// an empty level so the subsequent mark + restore steps operate on a cleared
			// level. Without this fallback the original prefab entities remain, the mark
			// loop stamps them un-editable, and the restore step inserts a duplicate set.
			if (!eng->load_level("template.tmap")) {
				sys_print(Warning,
						  "init_for_scene: template.tmap load failed; opening prefab without template background\n");
				eng->load_level("");
			}

			// Mark all template entities as dont_serialize_or_edit
			auto& template_objects = eng->get_level()->get_all_objects();
			for (auto obj : template_objects) {
				if (auto entity = obj->cast_to<Entity>()) {
					entity->dont_serialize_or_edit = true;
				}
			}

			// Restore the prefab content into the scene
			if (!prefab_content.text.empty()) {
				try {
					UnserializedSceneFile unserialized = NewSerialization::unserialize_from_text(
						"prefab_edit_restore", prefab_content.text, false);
					insert_unserialized_into_scene(unserialized);
				}
				catch (const std::exception& e) {
					sys_print(Warning, "Failed to restore prefab content: %s\n", e.what());
				}
			}

			// Auto-flatten any pre-existing PrefabAssetComponent in the loaded prefab.
			// Prefab-in-prefab is unsupported (serializer skips parented entities), so any
			// nested reference in the file is broken data. Inline it now: unparent the
			// runtime-spawned children (preserving world transform), clear their inherited
			// flag, then destroy the prefab-root entity. Fixed-point loop handles depth.
			int flatten_pass_count = 0;
			while (true) {
				ASSERT(flatten_pass_count < 100); // pathological deep nesting guard
				PrefabAssetComponent* found = nullptr;
				for (auto obj : eng->get_level()->get_all_objects()) {
					if (auto* pac = obj->cast_to<PrefabAssetComponent>()) {
						found = pac;
						break;
					}
				}
				if (!found) break;

				Entity* owner = found->get_owner();
				ASSERT(owner);
				std::vector<Entity*> children_snap(owner->get_children().begin(),
												   owner->get_children().end());
				for (Entity* child : children_snap) {
					glm::mat4 ws = child->get_ws_transform();
					child->parent_to(nullptr);
					child->set_ws_transform(ws);
					child->dont_serialize_or_edit = false;
				}
				sys_print(Warning,
						  "Auto-flattening prefab-in-prefab on load: '%s' (%d entities promoted to root)\n",
						  found->prefab_path.c_str(), (int)children_snap.size());
				owner->destroy();
				flatten_pass_count++;
			}
		}
	} else {
		assetName = std::nullopt;
	}

	on_start.invoke();
	set_window_title();
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

EditorDoc::EditorDoc() : vis_filter(*this) {
	assert(eng->get_level());
}

EditorDoc::~EditorDoc() {
	// level will get unloaded in the main loop
	sys_print(Debug, "deleting map file for editor...\n");
	if (ScriptManager::inst && prop_editor)
		ScriptManager::inst->on_class_reloaded.remove(prop_editor.get());
	command_mgr->clear_all();
	on_close.invoke();

	EditorDoc::on_deletion.invoke(this);
}

#endif // EDITOR_BUILD
