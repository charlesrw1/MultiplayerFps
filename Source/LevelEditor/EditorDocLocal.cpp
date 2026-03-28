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
#include "Game/Components/DecalComponent.h"
#include "Framework/PropertyEd.h"

#include "Game/Components/SpawnerComponenth.h"

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
	add_editor_commands();

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
		sys_print(Warning, "EditorDoc::set_document_path: already has path\n");
		// return;
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

EditorDoc::EditorDoc() : vis_filter(*this) {
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
	inputs.reset_keyboard_and_mouse();

	if (inputs.get_focused()) {
		inputs.get_focused()->on_focused_tick(inputs);
		// camera, dragger, manip, etc.
	}
	if (!active_mode)
		active_mode = selection_mode.get();

	ed_cam.tick(inputs, eng->get_dt());
	manipulate->check_input(inputs);
	handle_dragger->tick(inputs);
	gui->draw(inputs);
	active_mode->tick(inputs); // selection,foliage, or decal
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

	IEditorTool::imgui_draw();

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

	manipulate->update(inputs);
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
