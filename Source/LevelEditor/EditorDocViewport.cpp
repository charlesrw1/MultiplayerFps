// Viewport drawing, toolbar, menu bar, and per-frame tick for EditorDoc.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "imgui.h"
#include "glad/glad.h"
#include "External/ImGuizmo.h"
#include "Framework/MyImguiLib.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"
#include "Render/RenderConfigVars.h"
#include "Assets/AssetRegistry.h"
#include "UI/GUISystemPublic.h"
#include "UI/Widgets/Layouts.h"
#include "Game/LevelAssets.h"
#include "LevelEditor/Commands.h"
#include "Framework/Rect2d.h"
#include "Game/EntityComponent.h"
#include "Game/Components/LightComponents.h"
#include "Input/InputSystem.h"
#include "Debug.h"
#include <glm/gtc/type_ptr.hpp>

extern void export_level_scene();
extern void start_play_process();

ConfigVar draw_coords_under_mouse("draw_coords_under_mouse", "0", CVAR_BOOL, "");

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------

glm::ivec2 ndc_to_screen_coord(glm::vec3 ndc) {
	ASSERT(UiSystem::inst);
	ndc.y *= -1;
	auto coordx = ndc.x * 0.5 + 0.5;
	auto coordy = ndc.y * 0.5 + 0.5;

	const auto vp_size = UiSystem::inst->get_vp_rect().get_size();
	const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();

	coordx *= vp_size.x;
	coordy *= vp_size.y;

	return {coordx, coordy};
}

// ---------------------------------------------------------------------------
// Per-frame tick
// ---------------------------------------------------------------------------

void EditorDoc::tick(float dt) {
	ASSERT(dt >= 0.f);
	// ed_cam.tick(dt);
	vs_setup = ed_cam.make_view();
}

// ---------------------------------------------------------------------------
// Main imgui draw (runs every frame)
// ---------------------------------------------------------------------------

void EditorDoc::imgui_draw() {
	ASSERT(selection_state);
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
	gui->draw(inputs, [&]() {
		if (active_mode)
			active_mode->draw_ui();
	});
	active_mode->tick(inputs); // selection,foliage, or decal
	check_inputs();
	draw_handles->tick();
	vis_filter.tick();

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

	prop_editor->draw(*sel_api_impl);

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
			const GuiFont* font = g_assets.find<GuiFont>("eng/fonts/monospace12.fnt").get();

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

// ---------------------------------------------------------------------------
// Toolbar / pre-viewport draw
// ---------------------------------------------------------------------------

void EditorDoc::hook_pre_scene_viewport_draw() {
	ASSERT(manipulate);
	auto get_icon = [](std::string str) -> ImTextureID {
		return ImTextureID(
			uint64_t(g_assets.find<Texture>("eng/editor/" + str).get()->get_internal_render_handle()));
	};

	if (!ImGui::BeginMenuBar()) return;

	// Sizes and shared style — icon_sz must match actual texture size to avoid bilinear blur
	const ImVec2 icon_sz(16, 16);
	const int icon_pad = -1;
	const ImVec4 col_active   = ImGui::GetStyle().Colors[ImGuiCol_Header];
	const ImVec4 col_hov_act  = ImGui::GetStyle().Colors[ImGuiCol_HeaderHovered];
	const ImVec4 col_inactive = ImVec4(0, 0, 0, 0);
	const ImVec4 col_hov      = ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered];

	// Helper: styled icon button with tooltip.  Returns true when clicked.
	auto toolbar_btn = [&](ImTextureID icon, bool active, const char* tip, const char* shortcut = nullptr) -> bool {
		ImGui::PushStyleColor(ImGuiCol_Button,        active ? col_active   : col_inactive);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active ? col_hov_act  : col_hov);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
		bool clicked = ImGui::ImageButton(icon, icon_sz, ImVec2(0, 0), ImVec2(1, 1), icon_pad);
		ImGui::PopStyleColor(3);
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(tip);
			if (shortcut) {
				ImGui::SameLine();
				ImGui::TextDisabled("  [%s]", shortcut);
			}
			ImGui::EndTooltip();
		}
		return clicked;
	};

	// -- Gizmo mode -------------------------------------------------------
	auto optype = manipulate->get_operation_type();

	if (toolbar_btn(get_icon("cursor.png"),    optype == 0,                    "Select",    "Q"))
		manipulate->set_operation_type({});
	if (toolbar_btn(get_icon("translate.png"), optype == ImGuizmo::TRANSLATE,  "Translate", "W"))
		manipulate->set_operation_type(ImGuizmo::TRANSLATE);
	if (toolbar_btn(get_icon("rotate.png"),    optype == ImGuizmo::ROTATE,     "Rotate",    "E"))
		manipulate->set_operation_type(ImGuizmo::ROTATE);
	if (toolbar_btn(get_icon("scale.png"),     optype == ImGuizmo::SCALE,      "Scale",     "R"))
		manipulate->set_operation_type(ImGuizmo::SCALE);

	ImGui::Separator();

	// -- Snapping ---------------------------------------------------------
	const bool snap_on = ed_has_snap.get_bool();
	if (toolbar_btn(snap_on ? get_icon("magnet_on.png") : get_icon("magnet_off.png"),
	                snap_on, "Snap to Grid", "Ctrl"))
		ed_has_snap.set_bool(!snap_on);

	// -- Coordinate space -------------------------------------------------
	const bool is_local = manipulate->get_mode() == ImGuizmo::MODE::LOCAL;
	if (toolbar_btn(is_local ? get_icon("local_coord.png") : get_icon("global_coord.png"),
	                false, is_local ? "Local Space (click for World)" : "World Space (click for Local)"))
		manipulate->set_mode(is_local ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL);

	ImGui::Separator();

	// -- Overlay toggles --------------------------------------------------
	const bool show_handles = ed_show_box_handles.get_bool();
	if (toolbar_btn(get_icon("bounding_box_pivot.png"), show_handles, "Show Bounding Box Handles"))
		ed_show_box_handles.set_bool(!show_handles);

	const bool show_text = editor_draw_name_text.get_bool();
	if (toolbar_btn(show_text ? get_icon("show_text_on.png") : get_icon("show_text_off.png"),
	                show_text, "Show Entity Labels"))
		editor_draw_name_text.set_bool(!show_text);

	ImGui::Separator();

	// -- Debug view dropdown (stays open so you can cycle modes) ----------
	struct DebugMode { const char* name; int mode; };
	static const DebugMode debug_modes[] = {
		{"Lit",           0},  {"Wireframe",      4},  {"Albedo",        5},
		{"Normals",       1},  {"AO",             3},  {"Diffuse Light", 6},
		{"Specular Light",7},  {"Lighting Only",  9},  {"Overdraw",     12},
		{"Lightmap UV",  10},  {"Object ID",      8},  {"Material ID",   2},
		{"Draw ID",      11},  {"Outlined",      100},
	};
	const int cur_dbg = r_debug_mode.get_integer();
	const char* view_label = "Lit";
	for (auto& m : debug_modes) if (m.mode == cur_dbg) { view_label = m.name; break; }

	const bool dbg_active = cur_dbg != 0;
	ImGui::PushStyleColor(ImGuiCol_Button,        dbg_active ? col_active   : col_inactive);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, dbg_active ? col_hov_act  : col_hov);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
	if (ImGui::Button(view_label))
		ImGui::OpenPopup("##dbg_view");
	ImGui::PopStyleColor(3);

	ImGui::EndMenuBar();

	// Popup rendered outside the menu bar scope so it can overlap freely
	if (ImGui::BeginPopup("##dbg_view")) {
		for (int i = 0; i < (int)(sizeof(debug_modes)/sizeof(debug_modes[0])); i++) {
			if (i == 1) ImGui::Separator(); // separator after "Lit"
			auto& m = debug_modes[i];
			bool sel = r_debug_mode.get_integer() == m.mode;
			if (ImGui::Selectable(m.name, sel, ImGuiSelectableFlags_DontClosePopups))
				r_debug_mode.set_integer(m.mode);
		}
		ImGui::EndPopup();
	}
}

// ---------------------------------------------------------------------------
// Scene viewport overlay (drag-drop + manipulator update)
// ---------------------------------------------------------------------------

void EditorDoc::hook_scene_viewport_draw() {
	ASSERT(manipulate);
	if (ImGui::BeginDragDropTarget()) {
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

				if (resource->type->get_type_name() == "Prefab") {
					command_mgr->add_command(new InstantiatePrefabCommand(*this, resource->filename, drop_transform));
				} else if (resource->type->get_type_name() == "Spawner-Entity") {
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

// ---------------------------------------------------------------------------
// Camera focus on selection
// ---------------------------------------------------------------------------

void EditorDoc::set_camera_target_to_sel() {
	ASSERT(selection_state);
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
		}
	}
}

// ---------------------------------------------------------------------------
// Menu bar
// ---------------------------------------------------------------------------

void EditorDoc::hook_menu_bar() {
	ASSERT(eng);
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
		if (ImGui::MenuItem("Road Builder", nullptr, active_mode == road_tool.get())) {
			active_mode = road_tool.get();
		}
		ImGui::EndMenu();
	}
}

#endif // EDITOR_BUILD
