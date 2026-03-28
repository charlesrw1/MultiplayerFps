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
#include "UI/UIBuilder.h"
#include "PropertyEditors.h"
#include "LevelSerialization/SerializeNew.h"
#include "EditorPopupTemplate.h"
#include "Framework/StringUtils.h"
#include "EditorPopupTemplate.h"
#include "UI/Widgets/EditorCube.h"
#include "UI/UILoader.h"
#include "PropertyEditors.h"
#include "Game/Components/LightComponents.h"
#include "Framework/StringUtils.h"
#include "EditorPopups.h"
#include <glm/gtc/type_ptr.hpp>
#include "Input/InputSystem.h"

MulticastDelegate<EditorDoc*> EditorDoc::on_creation;
MulticastDelegate<EditorDoc*> EditorDoc::on_deletion;

ConfigVar g_editor_newmap_template("g_editor_newmap_template", "eng/template_map.tmap", CVAR_DEV,
								   "whenever a new map is created, it will use this map as a template");
ConfigVar editor_draw_name_text("editor_draw_name_text", "0", CVAR_BOOL,
								"draw text above every entities head in editor");
ConfigVar editor_draw_name_text_alpha("editor_draw_name_text_alpha", "150", CVAR_INTEGER, "", 0, 255);
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
ConfigVar test1("test1", "200", CVAR_INTEGER, "", 0, 256);
ConfigVar test2("test2", "200", CVAR_INTEGER, "", 0, 256);
ConfigVar ed_show_box_handles("ed.show_box_handles", "0", CVAR_BOOL, "");

extern void export_godot_scene(const std::string& base_export_path);
extern void export_level_scene();
extern void start_play_process();
extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

static std::string to_string(StringView view) {
	return std::string(view.str_start, view.str_len);
}
// Unproject mouse coords into a vector
Ray EditorDoc::unproject_mouse_to_ray(const int mx, const int my) {
	return ed_cam.unproject_mouse(mx, my);
}

Color32 to_color32(glm::vec4 v) {
	Color32 c;
	c.r = glm::clamp(v.r * 255.f, 0.f, 255.f);
	c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
	c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
	c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
	return c;
}

bool check_if_string_is_number(const char* str) {
	try {
		std::stod(str);
		return true;
	}
	catch (...) {
		return false;
	}
}

string get_name_display_entity(const Entity* e) {
	string name = (e->get_editor_name().c_str());
	const bool is_prefab_root = false; // o.e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
	if (name.empty()) {
		if (is_prefab_root) {
			// name = o.e->get_object_prefab().get_name().c_str();
		} else {
			if (auto m = e->get_component<MeshComponent>()) {
				if (m->get_model())
					name = m->get_model()->get_name();
			}
			if (auto m = e->get_component<SpawnerComponent>()) {
				name = m->get_spawner_type();
			}
		}
	}
	if (name.empty()) {
		name = e->get_type().classname;
	}
	return name;
}

void EditorDoc::validate_fileids_before_serialize() {
	auto level = eng->get_level();
	auto& objs = level->get_all_objects();
}
#include "Framework/SerializerJson2.h"
#include "ObjectOutlineFilter.h"
#include "Animation/SkeletonData.h"
#include "Game/Components/BillboardComponent.h"
Bounds transform_bounds(glm::mat4 transform, Bounds b);
void EditorDoc::init_new() {
	clear_editor_changes();

	sys_print(Debug, "Edit mode: %s", "Scene");
	eng->get_level()->validate();
	command_mgr = std::make_unique<UndoRedoSystem>();

	dragger.on_drag_end.add(this, [this](Rect2d rect) {
		auto type = MouseSelectionAction::ADD_SELECT;
		if (Input::is_shift_down())
			type = MouseSelectionAction::ADD_SELECT;
		else if (Input::is_ctrl_down())
			type = MouseSelectionAction::UNSELECT;
		else {
			selection_state->clear_all_selected();
		}

		auto newRect = gui->convert_rect(rect);
		vector<EntityPtr> ents;
		if (ed_cam.get_is_using_ortho()) {

			const Bounds camb = ed_cam.get_ortho_selection_bounds(newRect);

			auto& allobjs = eng->get_level()->get_all_objects();

			for (auto obj : allobjs) {

				if (auto m = obj->cast_to<MeshComponent>()) {
					if (m->get_model() && m->get_is_visible() && !m->get_owner()->get_hidden_in_editor() &&
						!m->get_is_skybox() /*skipskybox*/) {
						auto thisbounds = m->get_model()->get_bounds();
						thisbounds = transform_bounds(m->get_owner()->get_ws_transform(), thisbounds);
						if (thisbounds.intersect(camb))
							ents.push_back(m->get_owner());
					}

				} else if (auto b = obj->cast_to<BillboardComponent>()) {
					if (!b->get_owner()->get_hidden_in_editor()) {
						auto thisbounds =
							Bounds(b->get_ws_position() - glm::vec3(0.5), b->get_ws_position() + glm::vec3(0.5));
						if (thisbounds.intersect(camb))
							ents.push_back(b->get_owner());
					}
				}
			}
			// rect.x to world space:

		} else {
			auto selection = idraw->mouse_box_select_for_editor(newRect.x, newRect.y, newRect.w, newRect.h);
			for (auto handle : selection) {
				if (handle.is_valid()) {
					auto component_ptr = idraw->get_scene()->get_read_only_object(handle)->owner;
					if (component_ptr) {
						auto owner = component_ptr->get_owner();
						ASSERT(owner);
						ents.push_back(owner);
					}
				}
			}
		}
		do_mouse_selection(type, ents, true);

		gui->do_box_select(type);
	});

	command_mgr->on_command_execute_or_undo.add(this, [&]() { set_has_editor_changes(); });
	gui = std::make_unique<EditorUILayout>(*this);
	selection_state = std::make_unique<SelectionState>(*this);
	prop_editor = std::make_unique<EdPropertyGrid>(*this, grid_factory);
	manipulate = std::make_unique<ManipulateTransformTool>(*this);
	drag_drop_preview = std::make_unique<DragDropPreview>();
	foliage_tool = std::make_unique<FoliagePaintTool>(*this);
	stamp_tool = std::make_unique<DecalStampTool>(*this);
	handle_dragger = std::make_unique<EViewportHandles>(*this);
	selection_mode = std::make_unique<SelectionMode>(*this);
	draw_handles = std::make_unique<DrawHandlesObject>(*this);
	PropertyFactoryUtil::register_basic(grid_factory);
	PropertyFactoryUtil::register_editor(*this, grid_factory);

	cmds = ConsoleCmdGroup::create("");
	cmds->add("all", [this](const Cmd_Args& args) {
		auto& objs = eng->get_level()->get_all_objects();
		for (auto s : objs)
			args.sys_print(Info, "%lld\n", int64_t(s->get_instance_id()));
	});
	cmds->add("sel", [this](const Cmd_Args& args) {
		auto selected = selection_state->get_selection_as_vector();
		for (auto s : selected)
			args.sys_print(Info, "%lld\n", int64_t(s.handle));
	});
	cmds->add("pp", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);

		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			const char* comp_name = "<no component>";
			string ent_name = get_name_display_entity(ent);

			args.sys_print(Info, "%-5lld %-20s %s\n", int64_t(id), comp_name, ent_name.c_str());
		}
	});

	cmds->add("fil", [this](const Cmd_Args& args) {
		const string ents = args.at(2);
		const string filter = args.at(1);

		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			if (OONameFilter::does_entity_pass_one_filter(filter, ent))
				args.sys_print(Info, "%lld\n", int64_t(id));
		}
	});
	cmds->add("so", [this](const Cmd_Args& args) {
		if (args.size() == 1) {
			selection_state->clear_all_selected();
			return;
		}
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		selection_state->clear_all_selected();
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->add_to_entity_selection(EntityPtr(id));
		}
	});
	cmds->add("sfso", [this](const Cmd_Args& args) {
		auto str = string_format("sel | fil %s | so\n", args.at(1));
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, str);
	});
	cmds->add("afso", [this](const Cmd_Args& args) {
		auto str = string_format("all | fil %s | so\n", args.at(1));
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::NOW, str);
	});

	cmds->add("as", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->add_to_entity_selection(EntityPtr(id));
		}
	});
	cmds->add("us", [this](const Cmd_Args& args) {
		const string ents = args.at(1);
		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			selection_state->remove_from_selection(EntityPtr(id));
		}
	});
	cmds->add("set-to-camera", [this](const Cmd_Args& args) {
		if (ed_cam.get_is_using_ortho()) {
			return;
		}
		if (!selection_state->has_only_one_selected())
			return;
		auto ent = selection_state->get_only_one_selected();
		if (ent.get()) {
			glm::mat4 cam_transform = glm::inverse(vs_setup.view);
			ent->set_ws_transform(cam_transform);
			manipulate->update_pivot_and_cached();
		}
	});
	cmds->add("set-field", [this](const Cmd_Args& args) {
		const string ents = args.at(3);
		nlohmann::json jsonObj;
		if (check_if_string_is_number(args.at(2)))
			jsonObj[args.at(1)] = std::stod(args.at(2));
		else if (strcmp(args.at(2), "true") == 0)
			jsonObj[args.at(1)] = true;
		else if (strcmp(args.at(2), "false") == 0)
			jsonObj[args.at(1)] = false;
		else
			jsonObj[args.at(1)] = args.at(2);

		const string filter = args.at(1);

		auto lines = StringUtils::to_lines(ents);
		for (auto& e : lines) {
			int64_t id = std::stoll(e);
			Entity* ent = EntityPtr(id).get();
			if (!ent)
				continue;
			{ ReadSerializerBackendJson2 reader("", jsonObj, *AssetDatabase::loader, *ent); }
			if (ent->get_components().size() > 0) {
				ReadSerializerBackendJson2 reader("", jsonObj, *AssetDatabase::loader, *ent->get_components().at(0));
			}
			ent->invalidate_transform(nullptr);
		}
	});

	cmds->add("set-box", [this](const Cmd_Args& args) {
		glm::vec3 min{};
		glm::vec3 max{};
		for (int i = 0; i < 3; i++)
			min[i] = std::atof(args.at(i + 1));
		for (int i = 0; i < 3; i++)
			max[i] = std::atof(args.at(i + 4));

		for (int i = 0; i < 3; i++) {
			if (min[i] > max[i])
				std::swap(min[i], max[i]);
		}
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		MeshComponent* mc = e->get_component<MeshComponent>();
		if (!mc || !mc->get_model())
			return;
		auto bounds = mc->get_model()->get_bounds();

		glm::vec3 set_size = (max - min);
		vec3 bounds_size = bounds.bmax - bounds.bmin;

		glm::vec3 scale = set_size / bounds_size;
		glm::vec3 bounds_c = bounds.get_center();

		glm::vec3 frac = glm::abs(bounds.bmin / bounds_size);

		glm::vec3 set_center = frac * set_size + min;
		e->set_ws_transform(set_center, glm::quat(), scale);
	});
	cmds->add("bone-list", [this](const Cmd_Args& args) {
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		MeshComponent* mc = e->get_component<MeshComponent>();
		if (!mc || !mc->get_model())
			return;
		MSkeleton* skel = mc->get_model()->get_skel();
		if (!skel)
			return;
		auto& bones = skel->get_all_bones();
		int count = bones.size();
		sys_print(Info, "%s: num bones: %d\n", mc->get_model()->get_name().c_str(), count);
		for (int i = 0; i < count; i++) {
			sys_print(Info, "\t%s\n", bones[i].strname.c_str());
		}
	});
	cmds->add("parent-to", [this](const Cmd_Args& args) {
		if (!selection_state->has_only_one_selected())
			return;
		Entity* e = selection_state->get_only_one_selected().get();
		if (!e)
			return;
		int64_t id = std::stoll(args.at(1));
		string bonename = args.at(2);
		EntityPtr parent_to(id);
		Entity* parent_to_e = parent_to.get();
		if (!parent_to_e)
			return;
		e->parent_to(parent_to_e);
		e->set_ls_position(glm::vec3(0.f));
		e->set_parent_bone(bonename.c_str());
	});

	cmds->add("SET_ORBIT_TARGET", [this](const Cmd_Args&) { set_camera_target_to_sel(); });
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

	Cmd_Manager::get()->execute(Cmd_Execute_Mode::NOW, "load_imgui_ini  editor.ini");

	assert(eng->get_level());

	// gui = eng->get_level()->spawn_entity()->create_component<EditorUILayout>();
	// gui->doc = this;
	// gui->set_owner_dont_serialize_or_edit(true);
	// gui->set_focus();
	// gui->key_down_delegate.add(this, &EditorDoc::on_key_down);
	// gui->mouse_drag_delegate.add(this, &EditorDoc::on_mouse_drag);
	// gui->wheel_delegate.add(this, &EditorDoc::on_mouse_wheel);
	// gui->key_down_delegate.add(command_mgr.get(), &UndoRedoSystem::on_key_event);
}
void EditorDoc::set_document_path(string newAssetName) {
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

bool EditorDoc::save_document_internal() {
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
	auto serialized = NewSerialization::serialize_to_text(debug_tag.c_str(), all_ents, false);
	// NewSerialization::serialize_to_text(all_ents, pa);
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

string EditorDoc::get_name() {
	string name = get_doc_name();
	if (name.empty())
		name = "<unnamed>";
	return "Scene: " + name;
}
void EditorDoc::enable_entity_eyedropper_mode(void* id) {
	eng->log_to_fullscreen_gui(Debug, "entering eyedropper mode...");
	active_eyedropper_user_id = id;
	eye_dropper_active = true;
	// gui->tool_text->hidden = false;
	// gui->tool_text->text = "EYEDROPPER ACTIVE (esc to exit)";
	// gui->tool_text->color = { 255,128,128 };
	// gui->tool_text->use_desired_size = true;
	// gui->tool_text->pivot = guiAnchor::Center;
}
void EditorDoc::exit_eyedropper_mode() {
	if (is_in_eyedropper_mode()) {
		eng->log_to_fullscreen_gui(Debug, "exiting eyedropper");
		eye_dropper_active = false;
		active_eyedropper_user_id = nullptr;

		// gui->tool_text->hidden = true;
	}
}

EditorDoc* EditorDoc::create_scene(opt<string> scene) {
	EditorDoc* out = new EditorDoc();
	out->init_for_scene(scene);
	EditorDoc::on_creation.invoke(out);
	return out;
}

void EditorDoc::init_for_scene(opt<string> scene) {
	init_new();
	validate_fileids_before_serialize();

	if (scene.has_value()) {
		assetName = scene.value();
	} else {
		assetName = std::nullopt;
	}

	on_start.invoke();
	set_window_title();
}

EditorDoc::EditorDoc() : ed_cam(inputs), dragger(*this), vis_filter(*this) {
	assert(eng->get_level());
}

EditorDoc::~EditorDoc() {
	// level will get unloaded in the main loop
	sys_print(Debug, "deleting map file for editor...\n");
	command_mgr->clear_all();
	on_close.invoke();

	EditorDoc::on_deletion.invoke(this);
}


const Entity* select_outermost_entity(const Entity* in) {
	const Entity* sel = in;
	while (sel) {
		if (!sel->dont_serialize_or_edit) {
			break;
		}
		sel = sel->get_parent();
	}
	return sel;
}

void EditorDoc::do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_rootmost_entity) {
	const Entity* actual_entity_to_select = e;
	if (select_rootmost_entity) {
		actual_entity_to_select = select_outermost_entity(actual_entity_to_select);
	}
	if (!actual_entity_to_select)
		return;

	if (is_in_eyedropper_mode()) {
		sys_print(Debug, "eyedrop!\n");
		on_eyedropper_callback.invoke(actual_entity_to_select);
		exit_eyedropper_mode();
		return;
	}

	ASSERT(actual_entity_to_select);
	if (action == MouseSelectionAction::SELECT_ONLY)
		selection_state->set_select_only_this(actual_entity_to_select);
	else if (action == MouseSelectionAction::ADD_SELECT)
		selection_state->add_to_entity_selection(actual_entity_to_select);
	else if (action == MouseSelectionAction::UNSELECT)
		selection_state->remove_from_selection(actual_entity_to_select);
}

void EditorDoc::do_mouse_selection(MouseSelectionAction action, vector<EntityPtr> ents, bool select_root_most_entity) {
	if (action == MouseSelectionAction::SELECT_ONLY) {
		selection_state->clear_all_selected();
		selection_state->add_entities_to_selection(ents);
	} else if (action == MouseSelectionAction::ADD_SELECT) {
		selection_state->add_entities_to_selection(ents);

	} else if (action == MouseSelectionAction::UNSELECT) {
		selection_state->remove_from_selection(ents);
	}
}

void EditorDoc::on_mouse_pick() {
	if (!inputs.can_use_mouse_click())
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
		} else {
			exit_eyedropper_mode(); // ?
		}

		inputs.eat_mouse_click();
	}
}

void EditorDoc::on_mouse_drag(int x, int y) {}

void EditorDoc::check_inputs() {
	const bool is_keyboard_blocked = UiSystem::inst->blocking_keyboard_inputs();
	if (is_keyboard_blocked)
		return;

	const bool has_shift = Input::is_shift_down();
	const bool has_ctrl = Input::is_ctrl_down();

	if (Input::was_key_pressed(SDL_SCANCODE_Z) && has_ctrl) {
		command_mgr->undo();
	} else if (Input::was_key_pressed(SDL_SCANCODE_S) && has_ctrl) {
		save();
	} else if (ed_cam.handle_events()) {
	}
}

Bounds transform_bounds(glm::mat4 transform, Bounds b) {
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

ConfigVar draw_coords_under_mouse("draw_coords_under_mouse", "0", CVAR_BOOL, "");

void EditorDoc::tick(float dt) {
	// ed_cam.tick(dt);
	vs_setup = ed_cam.make_view();
}


void EditorDoc::imgui_draw() {
	inputs.reset();

	if (inputs.get_focused()) {
		inputs.get_focused()->on_focused_tick();
		// camera, dragger, manip, etc.
	}
	if (!active_mode)
		active_mode = selection_mode.get();

	ed_cam.tick(eng->get_dt());
	manipulate->check_input();
	handle_dragger->tick();
	gui->draw();
	active_mode->tick(); // foliage, decal
	check_inputs();
	draw_handles->tick();
	vis_filter.tick();
	// const bool in_foliage_tool = foliage_active.get_bool();

	// handle_dragger->tick();

	// manipulate->check_input();
	// bool clicked = gui->draw();
	// check_inputs();

	int text_ofs = 0;
	auto draw_text = [&](const char* str) {
		TextShape shape;
		shape.text = str;
		shape.color = {200, 200, 200};
		shape.with_drop_shadow = true;
		shape.drop_shadow_ofs = 1;
		// center it
		Rect2d size = GuiHelpers::calc_text_size(shape.text, nullptr);
		glm::ivec2 pos = {-size.w / 2, size.h + text_ofs};
		glm::ivec2 ofs = GuiHelpers::calc_layout(pos, guiAnchor::Top, UiSystem::inst->get_vp_rect());
		shape.rect = Rect2d(ofs, {});

		UiSystem::inst->window.draw(shape);

		text_ofs += size.h;
	};
	if (is_in_eyedropper_mode()) {
		draw_text("Eyedropper Active");
	}
	if (manipulate->get_is_using_for_custom()) {
		draw_text("Manipulate Stolen");
	}

	prop_editor->draw();

	ILevelEditorInterface::imgui_draw();

	command_mgr->execute_queued_commands();

	drag_drop_preview->tick();

	if (draw_coords_under_mouse.get_bool() && UiSystem::inst->is_vp_hovered()) {
		int x, y;
		SDL_GetMouseState(&x, &y);

		auto size = UiSystem::inst->get_vp_rect().get_pos();

		const float scene_depth = idraw->get_scene_depth_for_editor(x - size.x, y - size.y);
		if (abs(scene_depth) <= 300 || ed_cam.get_is_using_ortho()) {
			Ray dir = unproject_mouse_to_ray(x, y);
			glm::vec3 pos = vs_setup.origin + dir.dir * scene_depth;
			if (ed_cam.get_is_using_ortho())
				pos = dir.pos;

			auto& win = UiSystem::inst->window;
			const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();

			TextShape text;
			text.font = font;
			text.with_drop_shadow = true;
			text.color = COLOR_WHITE;

			std::string str = string_format("%.1f %.1f %.1f", pos.x, pos.y, pos.z);
			text.text = str.c_str();
			text.rect.x = x - size.x;
			text.rect.y = y - size.y;

			win.draw(text);
		}
	}
}
void EditorDoc::hook_pre_scene_viewport_draw() {
	auto get_icon = [](std::string str) -> ImTextureID {
		return ImTextureID(
			uint64_t(g_assets.find_global_sync<Texture>("eng/editor/" + str).get()->get_internal_render_handle()));
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
	if (ImGui::BeginMenuBar()) {
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
		} else {
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
		} else {
			if (ImGui::ImageButton(globalcoord, size)) {
				manipulate->set_mode(ImGuizmo::MODE::LOCAL);
			}
		}

		{
			ImVec4 tintColor(0.6, 0.6, 0.6, 1.0);
			if (ed_show_box_handles.get_bool())
				tintColor = ImVec4(1.1, 1.1, 1.1, 1);
			if (ImGui::ImageButton(boundingbox, size, ImVec2(), ImVec2(1, 1), -1, ImVec4(), tintColor)) {
				ed_show_box_handles.set_bool(!ed_show_box_handles.get_bool());
			}
		}

		auto& drawtext = editor_draw_name_text;
		push_active_style(drawtext.get_bool());
		if (drawtext.get_bool()) {
			if (ImGui::ImageButton(showtext_on, size)) {
				drawtext.set_bool(false);
			}
		} else {
			if (ImGui::ImageButton(showtext_off, size)) {
				drawtext.set_bool(true);
			}
		}

		ImGui::EndMenuBar();
		ImGui::PopStyleColor(4);
	}
}

void EditorDoc::hook_scene_viewport_draw() {
	// if (get_focus_state() != editor_focus_state::Focused)
	//	return;

	if (ImGui::BeginDragDropTarget()) {
		// const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		// if (payload->IsDataType("AssetBrowserDragDrop"))
		//	sys_print("``` accepting\n");

		if (const ImGuiPayload* payload =
				ImGui::AcceptDragDropPayload("AssetBrowserDragDrop", ImGuiDragDropFlags_AcceptPeekOnly)) {
			glm::mat4 drop_transform = glm::mat4(1.f);

			int x, y;
			SDL_GetMouseState(&x, &y);
			auto size = UiSystem::inst->get_vp_rect().get_pos();
			const float scene_depth = idraw->get_scene_depth_for_editor(x - size.x, y - size.y);

			glm::vec3 dir = unproject_mouse_to_ray(x, y).dir;
			glm::vec3 worldpos =
				(abs(scene_depth) > 50.0) ? vs_setup.origin - dir * 25.0f : vs_setup.origin + dir * scene_depth;
			drop_transform[3] = glm::vec4(worldpos, 1.0);

			AssetOnDisk* resource = *(AssetOnDisk**)payload->Data;

			auto asset_class_type = resource->type->get_asset_class_type();
			if (asset_class_type) {
				if (asset_class_type->is_a(Component::StaticType)) {
					const ClassTypeInfo* t = ClassBase::find_class(resource->filename.c_str());
					drag_drop_preview->set_preview_component(t, drop_transform);
				} else if (asset_class_type->is_a(Model::StaticType)) {
					Model* mod = Model::load(resource->filename);
					drag_drop_preview->set_preview_model(mod, drop_transform);
				}
			}

			if (const ImGuiPayload* dummy = ImGui::AcceptDragDropPayload("AssetBrowserDragDrop")) {

				if (resource->type->get_type_name() == "Spawner-Entity") {
					command_mgr->add_command(new CreateSpawnerCommand(*this, resource->filename, drop_transform));
				} else if (resource->type->get_asset_class_type()->is_a(Entity::StaticType)) {
					command_mgr->add_command(
						new CreateCppClassCommand(*this, resource->filename, drop_transform, EntityPtr(), false));
				} else if (resource->type->get_asset_class_type()->is_a(Model::StaticType)) {
					command_mgr->add_command(new CreateStaticMeshCommand(*this, resource->filename, drop_transform));
				} else if (resource->type->get_asset_class_type()->is_a(Component::StaticType)) {
					EntityPtr parent_to;
					{ const ClassTypeInfo* type = ClassBase::find_class(resource->filename.c_str()); }
					command_mgr->add_command(
						new CreateCppClassCommand(*this, resource->filename, drop_transform, parent_to, true));
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	manipulate->update();
}

#include "Game/Components/SpawnerComponenth.h"

class SpawnerIProped : public IPropertyEditor
{
public:
	SpawnerIProped(SpawnerComponent* sc, string key) : sc(sc), key2(key) {
		prop = &hacked_bullshit;
		hacked_bullshit.name = key2.c_str();
		value = sc->obj[key];
		instance = this; // bs
	}
	~SpawnerIProped() {
		if (sc.get())
			sc->obj[key2] = value;
	}
	bool internal_update() {
		ImguiInputTextCallbackUserStruct user;
		user.string = &value;
		if (ImGui::InputText("##input_text", (char*)value.c_str(), value.size() + 1 /* null terminator byte */,
							 ImGuiInputTextFlags_CallbackResize, imgui_input_text_callback_function, &user)) {
			value.resize(strlen(value.c_str())); // imgui messes with buffer size
			sc->obj[key2] = value;
			return true;
		}
		return false;
	}
	PropertyInfo hacked_bullshit;
	string key2;
	string value;
	obj<SpawnerComponent> sc;
};

class SpawnerModelProp : public IPropertyEditor
{
public:
	SpawnerModelProp(SpawnerComponent* sc) : sc(sc) {
		prop = &hacked_bullshit;
		hacked_bullshit.name = "model";

		instance = this; // bs

		assetprop.instance = this;
		assetprop.prop = &hacked_bullshit2;
		hacked_bullshit2.type = core_type_id::AssetPtr;
		hacked_bullshit2.class_type = &Model::StaticType;
		hacked_bullshit2.offset = offsetof(SpawnerModelProp, model);

		if (sc->obj["model"].is_string()) {
			string str = sc->obj["model"];
			if (!str.empty())
				model = Model::load(str);
		}
	}
	~SpawnerModelProp() {}
	bool internal_update() {
		bool res = assetprop.internal_update();
		if (res) {
			set_mod();
		}
		return res;
	}
	bool can_reset() final { return assetprop.can_reset(); }
	void reset_value() final {
		assetprop.reset_value();
		set_mod();
	}
	void set_mod() {
		if (model)
			sc->obj["model"] = model->get_name();
		else
			sc->obj["model"] = "";

		sc->set_model();
	}

	AssetPropertyEditor assetprop;
	Model* model = nullptr;

	PropertyInfo hacked_bullshit;
	PropertyInfo hacked_bullshit2;

	obj<SpawnerComponent> sc;
};

void EdPropertyGrid::refresh_grid() {
	grid.clear_all();

	auto& ss = ed_doc.selection_state;

	if (!ss->has_any_selected())
		return;

	if (ss->has_only_one_selected() && ss->get_only_one_selected() /* can return null...*/) {
		auto entity = ss->get_only_one_selected();
		assert(entity);
		sys_print(Debug, "EdPropertyGrid::refresh_grid: adding to grid: %s\n", entity->get_type().classname);

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
			if (eng->get_object(selected_component) == nullptr ||
				eng->get_object(selected_component)->cast_to<Component>() == nullptr ||
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

			if (c->is_a<SpawnerComponent>()) {
				auto sc = (SpawnerComponent*)c;
				for (auto& [name, prop] : sc->obj.items()) {
					if (!prop.is_string())
						continue;
					if (name[0] == '_')
						continue;
					if (name == "model")
						grid.add_iproped_manual(new SpawnerModelProp(sc));
					else
						grid.add_iproped_manual(new SpawnerIProped(sc, name));
				}
			}
		}
	}
}


void EditorDoc::set_camera_target_to_sel() {
	if (selection_state->has_only_one_selected()) {
		auto ptr = selection_state->get_only_one_selected();
		if (ptr) {
			float radius = 1.f;
			auto mesh = ptr->get_component<MeshComponent>();
			auto pos = ptr->get_ws_position();
			if (mesh && mesh->get_model()) {
				radius = glm::max(mesh->get_model()->get_bounding_sphere().w, 0.5f);
				auto sphere = glm::vec3(mesh->get_model()->get_bounding_sphere());
				pos = glm::vec3(ptr->get_ws_transform() * glm::vec4(sphere, 1.0));
			}

			ed_cam.set_orbit_target(pos, radius);
			// camera.set_orbit_target(pos, radius);
		}
	}
}

void EditorDoc::hook_menu_bar() {

	if (ImGui::BeginMenu("Commands")) {
		if (ImGui::MenuItem("Export as .glb")) {
			export_level_scene();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Import lightmap from baking")) {
			LightmapComponent* lm =
				(LightmapComponent*)eng->get_level()->find_first_component(&LightmapComponent::StaticType);
			if (lm) {
				lm->do_import();
			} else {
				sys_print(Error, "no lightmap object in scene, add a LightmapComponent\n");
			}
		}
		if (ImGui::MenuItem("Export for lightmap bake")) {
			LightmapComponent* lm =
				(LightmapComponent*)eng->get_level()->find_first_component(&LightmapComponent::StaticType);
			if (lm) {
				lm->do_export();
			} else {
				sys_print(Error, "no lightmap object in scene, add a LightmapComponent\n");
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Play")) {
			start_play_process();
		}

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Mode")) {

		if (ImGui::MenuItem("Default", nullptr, active_mode == selection_mode.get())) {
			active_mode = selection_mode.get();
		}
		if (ImGui::MenuItem("Foliage Paint", nullptr, active_mode == foliage_tool.get())) {
			active_mode = foliage_tool.get();
		}
		if (ImGui::MenuItem("Decal Stamp", nullptr, active_mode == stamp_tool.get())) {
			active_mode = stamp_tool.get();
		}
		ImGui::EndMenu();
	}
}

EditorUILayout::EditorUILayout(EditorDoc& doc) : doc(&doc) {

	doc.ed_cam.on_ortho_state_change.add(this, [&]() { cube.rotation.begin_interpolate(); });
}

glm::ivec2 ndc_to_screen_coord(glm::vec3 ndc) {
	ndc.y *= -1;
	auto coordx = ndc.x * 0.5 + 0.5;
	auto coordy = ndc.y * 0.5 + 0.5;

	const auto vp_size = UiSystem::inst->get_vp_rect().get_size();
	const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();

	coordx *= vp_size.x;
	coordy *= vp_size.y;

	return {coordx, coordy};
}

bool EditorUILayout::draw() {
	const float dt = eng->get_dt();

	RenderWindow& window = UiSystem::inst->window;
	cube.rotation.set_current((glm::mat3)doc->vs_setup.view);
	cube.draw(window, dt);
	// paint
	if (doc->dragger.get_is_dragging()) {
		auto rect = doc->dragger.get_drag_rect();
		// builder.draw_solid_rect({ rect.x,rect.y }, { rect.w,rect.h }, { 200,200,200,50 });
		rect.x -= UiSystem::inst->get_vp_rect().get_pos().x;
		rect.y -= UiSystem::inst->get_vp_rect().get_pos().y;

		RectangleShape shape;
		shape.rect = rect;
		Uint8 c = test1.get_integer();
		Uint8 a = test2.get_integer();
		shape.color = {c, c, c, a};
		window.draw(shape);
	}

	bool do_mouse_click =
		Input::was_mouse_released(0) && UiSystem::inst->is_vp_hovered() && !doc->dragger.get_is_dragging();
	int x = Input::get_mouse_pos().x;
	int y = Input::get_mouse_pos().y;

	if (!doc->inputs.can_use_mouse_click())
		do_mouse_click = false;

	if (!eng->get_level())
		return false;

	const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
	if (!font)
		font = UiSystem::inst->defaultFont;
	auto objs = get_objs();
	std::sort(objs.begin(), objs.end(), [](const obj& a, const obj& b) -> bool { return a.pos.z < b.pos.z; });
	const Entity* clicked = nullptr;
	for (const auto o : objs) {
		string name = get_name_display_entity(o.e);

		const int icon_size = 16;
		InlineVec<Texture*, 6> icons;
		auto e = o.e;

		bool found_script = false;
		for (auto c : o.e->get_components()) {
			if (c->dont_serialize_or_edit_this())
				continue;
			const char* s = c->get_editor_outliner_icon();
			if (c->get_type().get_is_lua_class()) {
				found_script = true;
				s = "eng/editor/script_lua.png";
			}
			if (!*s)
				continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}
		if (!(found_script || editor_draw_name_text.get_bool()))
			continue;

		auto size = GuiHelpers::calc_text_size_no_wrap(name, font);

		const int text_offset = (icon_size + 1) * icons.size();
		size.w += text_offset;

		const auto coord = ndc_to_screen_coord(o.pos);
		const auto coordx = coord.x - size.w / 2;
		const auto coordy = coord.y - size.h / 2;

		const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();

		Color32 color = {50, 50, 50, (uint8_t)editor_draw_name_text_alpha.get_integer()};
		if (o.e->get_selected_in_editor())
			color = {255, 180, 0, 150};

		if (do_mouse_click) {
			Rect2d r(coordx - 3, coordy - 3, size.w + 6, size.h + 6);
			if (r.is_point_inside(x - vp_pos.x, y - vp_pos.y)) {

				clicked = o.e;
			}
		}
		glm::ivec2 textofs = {0, font->base};

		RectangleShape shape;
		shape.rect = Rect2d({coordx - 3, coordy - 3}, {size.w + 6, size.h + 6});
		shape.color = color;
		window.draw(shape);

		// builder.draw_solid_rect({ coordx - 3,coordy - 3 }, { size.w + 6,size.h + 6 }, color);
		for (int i = 0; i < icons.size(); i++) {
			const int ofs = (i) * (icon_size + 1);

			shape.rect = Rect2d({coordx + ofs, coordy}, {icon_size, icon_size});
			shape.texture = icons[i];
			shape.color = COLOR_WHITE;
			window.draw(shape);
		}

		TextShape tshape;
		tshape.rect = Rect2d(glm::ivec2{coordx + 1 + text_offset, coordy + 1} + textofs, {});
		tshape.font = font;
		tshape.text = name;
		tshape.color = COLOR_BLACK;
		window.draw(shape);
		tshape.rect = Rect2d(glm::ivec2{coordx + text_offset, coordy} + textofs, {});
		tshape.color = COLOR_WHITE;
		window.draw(tshape);
	}
	if (clicked) {
		doc->inputs.eat_mouse_click();
		if (Input::is_shift_down()) {
			doc->do_mouse_selection(MouseSelectionAction::ADD_SELECT, clicked, true);
		} else if (Input::is_ctrl_down()) {
			doc->do_mouse_selection(MouseSelectionAction::UNSELECT, clicked, true);
		} else {
			doc->do_mouse_selection(MouseSelectionAction::SELECT_ONLY, clicked, true);
		}
		return true;
	} else {
		// if(do_mouse_click)
		//	mouse_down_delegate.invoke(x-ws_position.x, y-ws_position.y, button_clicked);
	}
	return false;
}

void EditorUILayout::do_box_select(MouseSelectionAction action) {
	if (!editor_draw_name_text.get_bool())
		return;
	if (!eng->get_level())
		return;

	auto objs = get_objs();
	assert(doc->dragger.get_is_dragging());
	const auto vp_size = UiSystem::inst->get_vp_rect().get_size();
	const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();
	auto area = doc->dragger.get_drag_rect();
	area.x -= vp_pos.x;
	area.y -= vp_pos.y;

	for (auto o : objs) {
		const char* name = (o.e->get_editor_name().c_str());
		const bool is_prefab_root =
			false; // o.e->get_object_prefab_spawn_type() == EntityPrefabSpawnType::RootOfPrefab;
		if (!*name) {
			if (is_prefab_root) {
				// name = o.e->get_object_prefab().get_name().c_str();
			} else {
				if (auto m = o.e->get_component<MeshComponent>()) {
					if (m->get_model())
						name = m->get_model()->get_name().c_str();
				}
			}
		}
		if (!*name) {
			name = o.e->get_type().classname;
		}
		const GuiFont* font = g_assets.find_global_sync<GuiFont>("eng/fonts/monospace12.fnt").get();
		if (!font)
			font = UiSystem::inst->defaultFont;
		const int icon_size = 16;
		InlineVec<Texture*, 6> icons;
		auto e = o.e;
		if (is_prefab_root) {
			const char* s = "eng/editor/prefab_p.png";
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		for (auto c : o.e->get_components()) {
			if (c->dont_serialize_or_edit_this())
				continue;
			const char* s = c->get_editor_outliner_icon();
			if (!*s)
				continue;
			auto tex = g_assets.find_global_sync<Texture>(s);
			icons.push_back(tex.get());
		}

		auto size = GuiHelpers::calc_text_size_no_wrap(name, font);

		const int text_offset = (icon_size + 1) * icons.size();
		size.w += text_offset;

		o.pos.y *= -1;
		auto coordx = o.pos.x * 0.5 + 0.5;
		auto coordy = o.pos.y * 0.5 + 0.5;

		coordx *= vp_size.x;
		coordy *= vp_size.y;
		// coordx += vp_pos.x;
		// coordy += vp_pos.y;
		coordx -= size.w / 2;
		coordy -= size.h / 2;

		Rect2d r(coordx - 3, coordy - 3, size.w + 6, size.h + 6);
		if (r.overlaps(area)) {
			doc->do_mouse_selection(action, e, true);
		}
	}
}

std::vector<EditorUILayout::obj> EditorUILayout::get_objs() {
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
	return objs;
}

Entity* EditorDoc::spawn_entity() {
	Entity* e = eng->get_level()->spawn_entity();
	instantiate_into_scene(e);
	return e;
}

Component* EditorDoc::attach_component(const ClassTypeInfo* ti, Entity* e) {
	Component* c = e->create_component(ti);
	instantiate_into_scene(c);
	return c;
}

void EditorDoc::remove_scene_object(BaseUpdater* u) {
	u->destroy_deferred();
}

void EditorDoc::insert_unserialized_into_scene(UnserializedSceneFile& file) {

	eng->get_level()->insert_unserialized_entities_into_level(file);
}

void EditorDoc::instantiate_into_scene(BaseUpdater* u) {}

#endif
#include "Render/MaterialPublic.h"
void DragDropPreview::set_preview_model(Model* m, const glm::mat4& where) {

	had_state_set = true;
	if (!(state == State::PreviewModel && preview_model == m)) {
		state = State::PreviewModel;
		preview_model = m;
		delete_obj();
		Entity* e = eng->get_level()->spawn_entity();
		e->dont_serialize_or_edit = true;
		auto mesh = e->create_component<MeshComponent>();
		mesh->set_model(m);
		mesh->set_material_override(MaterialInstance::load("trigger_zone.mm"));
		obj_ptr = e;
		fixup_entity();
	}
	if (obj_ptr) {
		obj_ptr->set_ws_transform(where);
	}
}

void DragDropPreview::set_preview_component(const ClassTypeInfo* t, const glm::mat4& where) {

	if (!t || !t->is_a(Component::StaticType)) {
		sys_print(Warning, "set_preview_component: not a Component subtype\n");
		return;
	}
	had_state_set = true;
	if (!(state == State::PreviewComponent && preview_comp == t)) {
		state = State::PreviewComponent;
		preview_comp = t;
		delete_obj();
		Entity* e = eng->get_level()->spawn_entity();
		e->dont_serialize_or_edit = true;
		e->create_component(t);
		obj_ptr = e;
		fixup_entity();
	}
	if (obj_ptr) {
		obj_ptr->set_ws_transform(where);
	}
}

void DragDropPreview::tick() {
	if (!had_state_set) {
		delete_obj();
		state = State::None;
		assert(!obj_ptr);
	} else {
		had_state_set = false;
	}
}
#include "Game/Components/ArrowComponent.h"
#include "Game/Components/BillboardComponent.h"
void DragDropPreview::fixup_entity() {
	auto set_r = [](auto&& self, Entity* e) -> void {
		for (Component* c : e->get_components()) {
			if (auto mesh = c->cast_to<MeshComponent>()) {
				mesh->set_material_override(MaterialInstance::load("trigger_zone.mm"));
			}
			if (auto arrow = c->cast_to<ArrowComponent>()) {
				arrow->visible = false;
			}
			if (auto bb = c->cast_to<BillboardComponent>()) {
				bb->set_is_visible(false);
			}
		}
		for (Entity* c : e->get_children()) {
			self(self, c);
		}
	};
	if (obj_ptr)
		set_r(set_r, obj_ptr.get());
}

void DragDropPreview::delete_obj() {
	Entity* e = obj_ptr.get();
	if (e) {
		e->destroy_deferred();
	}
	obj_ptr = nullptr;
}


void DrawHandlesObject::tick() {
	if (!ed_show_box_handles.get_bool())
		return;

	if (doc.selection_state->has_only_one_selected()) {
		auto selected = doc.selection_state->get_only_one_selected();
		if (selected->get_editor_name() == "___handle_marker") {

		} else {
			last_selected = selected;
		}
	} else {
		last_selected = EntityPtr();
	}
	if (last_selected.get()) {

		bool good_to_use = false;
		Bounds bounds_to_use;

		auto mesh = last_selected->get_component<MeshComponent>();
		if (mesh && mesh->get_model()) {
			bounds_to_use = mesh->get_model()->get_bounds();
			good_to_use = true;
		} else if (auto cubemap = last_selected->get_component<CubemapComponent>()) {
			bounds_to_use = Bounds(-vec3(0.5), vec3(0.5));
			good_to_use = true;
		} else if (auto decal = last_selected->get_component<DecalComponent>()) {
			bounds_to_use = Bounds(-vec3(0.5), vec3(0.5));
			good_to_use = true;
		}

		if (good_to_use) {
			auto transform = last_selected->get_ws_transform();
			glm::mat4 m = transform * glm::translate(glm::mat4(1), bounds_to_use.bmin);
			auto extents = bounds_to_use.bmax - bounds_to_use.bmin;
			auto result = doc.handle_dragger->box_handles(1, m, extents);

			if (result == VHResult::Changing) {

				// now do the inverse... (m was set)

				mat4 want = m * glm::inverse(glm::translate(glm::mat4(1), bounds_to_use.bmin));
				glm::vec3 p, s;
				glm::quat q;
				decompose_transform(want, p, q, s);
				q = glm::normalize(q);
				want = compose_transform(p, q, s);

				last_selected->set_ws_transform(want);
			}

			Debug::add_transformed_box(m, extents, {255, 165, 0}, 0, false);
		}
	} else {
		last_selected = EntityPtr();
	}
}

void EntityVisiblityFilter::tick() {
	if (!ImGui::Begin("OutlineFilter")) {
		ImGui::End();
		return;
	}

	// not a pretty way
	auto draw_item = [&](const string& s) -> int {
		if (!MapUtil::contains(status, s))
			status[s] = true;
		bool b = status[s];
		if (ImGui::Selectable(s.c_str(), false, 0, ImVec2(200, 0)))
			return 1;
		ImGui::SameLine();
		ImGui::PushID(s.c_str());
		if (ImGui::Checkbox("##empty", &b)) {
			status[s] = b;
			ImGui::PopID();
			return b ? 2 : 3;
		}
		ImGui::PopID();
		return 0;
	};
	auto add_component_type_to_selection = [&](const ClassTypeInfo& t) {
		auto& objs = eng->get_level()->get_all_objects();
		const bool select_only = !Input::is_shift_down();
		if (select_only)
			doc.selection_state->clear_all_selected();
		for (auto o : objs) {
			if (o->get_type().is_a(t)) {
				auto owner = o->cast_to<Component>()->get_owner();
				doc.selection_state->add_to_entity_selection(owner);
			}
		}
	};
	auto set_component_visibility = [&](const ClassTypeInfo& t, bool b) {
		auto& objs = eng->get_level()->get_all_objects();
		for (auto o : objs) {
			if (o->get_type().is_a(t)) {
				auto owner = o->cast_to<Component>()->get_owner();
				owner->set_hidden_in_editor(!b);
			}
		}
	};
	auto do_stuff = [&](const ClassTypeInfo& t, int res) {
		if (res == 1)
			add_component_type_to_selection(t);
		else if (res == 2)
			set_component_visibility(t, true);
		else if (res == 3)
			set_component_visibility(t, false);
	};

	int res = draw_item("Lights");
	do_stuff(PointLightComponent::StaticType, res);
	do_stuff(SpotLightComponent::StaticType, res);
	res = draw_item("GiVols");
	do_stuff(GiVolumeComponent::StaticType, res);
	res = draw_item("CubemapVols");
	do_stuff(CubemapComponent::StaticType, res);
	res = draw_item("Decals");
	do_stuff(DecalComponent::StaticType, res);
	res = draw_item("Sun");
	do_stuff(SunLightComponent::StaticType, res);
	res = draw_item("Env");
	do_stuff(SkylightComponent::StaticType, res);
	res = draw_item("Spawners");
	do_stuff(SpawnerComponent::StaticType, res);
	ImGui::End();
}
