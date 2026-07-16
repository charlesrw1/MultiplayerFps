// Viewport drawing, toolbar, menu bar, and per-frame tick for EditorDoc.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "EditorRecents.h"
#include "Framework/Config.h"
#include "imgui.h"
#include "imgui_internal.h" // ImGuiItemFlags_NoNav for the recent-switcher popup
#include "glad/glad.h"
#include "External/ImGuizmo.h"
#include "Framework/MyImguiLib.h"
#include "Render/DrawPublic.h"
#include "Render/Texture.h"
#include "Render/RenderConfigVars.h"
#include "Assets/AssetRegistry.h"
#include "Assets/AssetBrowser.h"
#include "Assets/AssetSizeViewer.h"
#include "AssetTools/DiagnosticsWindow.h"
#include "Framework/ProfilerUI.h"
#include "UI/GUISystemPublic.h"
#include "UI/BaseGUI.h"
#include "Game/LevelAssets.h"
#include "LevelEditor/Commands.h"
#include "Framework/Rect2d.h"
#include "Game/EntityComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Animation/SkeletonData.h"
#include "Input/InputSystem.h"
#include "Framework/StringUtils.h"
#include "Game/EditorAddMenu.h"
#include "Game/Components/PrefabAssetComponent.h"
#include "Game/Prefab.h"
#include "Debug.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

extern void export_level_scene();
extern void start_play_process(const std::string& play_map_path, bool lua_debug, bool cpp_debug);
extern int imgui_std_string_resize(ImGuiInputTextCallbackData* data);

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
	draw_recent_switcher_popup();
	draw_handles->tick();
	vis_filter.tick();

	int text_ofs = 0;
	auto draw_text = [&](const char* str) {
		TextShape shape;
		shape.text = str;
		shape.color = {200, 200, 200};
		shape.with_drop_shadow = true;
		shape.drop_shadow_ofs = 1;
		shape.font = g_assets.find<GuiFont>("eng/fonts/monospace12.fnt").get();
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
	// Tells the user their map has entities the loader couldn't instantiate. They're
	// being held verbatim and will round-trip on save, but they aren't editable here.
	if (auto level = eng->get_level()) {
		const int n = (int)level->preserved_unknown_objs.size();
		if (n > 0) {
			std::string warn = "Unresolved entities in map: " + std::to_string(n) +
							   " (preserved as opaque blobs — see Commands > Unresolved Entities)";
			draw_text(warn.c_str());
		}
		// Fields whose JSON key didn't match any reflected property on the resolved type.
		// Usually a typo or a stale field from an older version of the component.
		const int nf = (int)level->unknown_field_warnings.size();
		if (nf > 0) {
			std::string warn = "Map has " + std::to_string(nf) +
							   " unknown field(s) — see Commands > Unknown Fields";
			draw_text(warn.c_str());
		}
		// No skylight means indirect lighting/reflections fall back to a flat 0.1 constant ambient
		// with no specular (see ConstAmbientF.txt) - every real map should have one.
		if (!idraw->get_scene()->has_skylight()) {
			draw_text("Scene has no skylight — indirect lighting/reflections disabled");
		}
	}

	prop_editor->draw(*sel_api_impl);

	IEditorTool::imgui_draw();

	command_mgr->execute_queued_commands();

	drag_drop_preview->tick();

	if (draw_coords_under_mouse.get_bool() && UiSystem::inst->is_vp_hovered()) {
		float mxf = 0.f, myf = 0.f;
		SDL_GetMouseState(&mxf, &myf);
		int x = (int)mxf, y = (int)myf;

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

	ImGui::SameLine(0, 0);
	ImGui::PushStyleColor(ImGuiCol_Button,        snap_on ? col_active   : col_inactive);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, snap_on ? col_hov_act  : col_hov);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3, 3));
	const bool snap_dd_clicked = ImGui::ArrowButton("##snap_dd", ImGuiDir_Down);
	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
		ImGui::BeginTooltip(); ImGui::TextUnformatted("Snap Settings"); ImGui::EndTooltip();
	}

	// -- Coordinate space -------------------------------------------------
	const bool is_local = manipulate->get_mode() == ImGuizmo::MODE::LOCAL;
	if (toolbar_btn(is_local ? get_icon("local_coord.png") : get_icon("global_coord.png"),
	                false, is_local ? "Local Space (click for World)" : "World Space (click for Local)"))
		manipulate->set_mode(is_local ? ImGuizmo::MODE::WORLD : ImGuizmo::MODE::LOCAL);

	ImGui::Separator();

	// -- Overlay toggles --------------------------------------------------
	const int box_handle_mode = ed_show_box_handles.get_integer();
	const char* box_handle_labels[] = {"Box Handles: Off", "Box Handles: Face", "Box Handles: Edge"};
	if (toolbar_btn(get_icon("bounding_box_pivot.png"), box_handle_mode > 0, box_handle_labels[box_handle_mode]))
		ed_show_box_handles.set_integer((box_handle_mode + 1) % 3);

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
	const bool dbg_btn_clicked = ImGui::Button(view_label);
	ImGui::PopStyleColor(3);

	ImGui::Separator();

	// -- Overlay texture dropdown (ot command) ----------------------------
	struct OtEntry { const char* label; const char* tex; bool has_mip; };
	static const OtEntry ot_list[] = {
		{"Scene Color",      "_scene_color",          false},
		{"Scene Depth",      "_scene_depth",          false},
		{"GBuffer 0",        "_gbuffer0",             false},
		{"GBuffer 1",        "_gbuffer1",             false},
		{"GBuffer 2",        "_gbuffer2",             false},
		{"SSAO",             "_ssao_result",          false},
		{"SSAO Blur",        "_ssao_blur",            false},
		{"Linear Depth",     "_linear_depth",         false},
		{"CSM Shadow",       "_csm_shadow",           true },
		{"Spot Shadows",     "_spto_shadow",          false},
		{"Bloom",            "_bloom_result",         false},
		{"Velocity",         "_scene_motion",         false},
		{"SSR",              "_ssr",                  false},
		{"DDGI Accum",       "_ddgi_accum",           false},
		{"Scene Mip Chain",  "_scene_color_mipchain", true },
	};

	const auto ov = idraw->editor_get_debug_overlay_state();
	const bool ot_active = ov.active_tex != nullptr;
	const char* ot_btn_label = "OT: Off";
	if (ot_active) {
		ot_btn_label = ov.active_tex;
		for (auto& o : ot_list)
			if (strcmp(o.tex, ov.active_tex) == 0) { ot_btn_label = o.label; break; }
	}

	ImGui::PushStyleColor(ImGuiCol_Button,        ot_active ? col_active   : col_inactive);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ot_active ? col_hov_act  : col_hov);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
	const bool ot_btn_clicked = ImGui::Button(ot_btn_label);
	ImGui::PopStyleColor(3);

	ImGui::EndMenuBar();

	// OpenPopup must be called outside BeginMenuBar()/EndMenuBar() so it shares
	// the same window ID namespace as the BeginPopup() calls below.
	if (snap_dd_clicked) ImGui::OpenPopup("##snap_settings");
	if (dbg_btn_clicked) ImGui::OpenPopup("##dbg_view");
	if (ot_btn_clicked)  ImGui::OpenPopup("##ot_view");

	// Ctrl+P / Alt+P parenting popups (opened via flags from check_inputs so they share this
	// window's ID namespace, same as the popups above).
	draw_parenting_popups();

	// Popup rendered outside the menu bar scope so it can overlap freely
	if (ImGui::BeginPopup("##snap_settings")) {
		ImGui::TextDisabled("Snap Settings");
		ImGui::Separator();
		auto snap_row = [](const char* label, ConfigVar& cvar, float min_val, float max_val, const char* fmt) {
			float val = cvar.get_float();
			ImGui::TextUnformatted(label);
			ImGui::SameLine(110.f);
			ImGui::PushID(label);
			if (ImGui::SmallButton("/2")) cvar.set_float(glm::clamp(val * 0.5f, min_val, max_val));
			ImGui::SameLine();
			char buf[32]; snprintf(buf, sizeof(buf), fmt, val);
			ImGui::SetNextItemWidth(70.f);
			if (ImGui::InputText("##v", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
				float parsed = (float)atof(buf);
				if (parsed > 0.f) cvar.set_float(glm::clamp(parsed, min_val, max_val));
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("x2")) cvar.set_float(glm::clamp(val * 2.0f, min_val, max_val));
			ImGui::PopID();
		};

		snap_row("Translation",  ed_translation_snap, 0.05f, 128.f,  "%.4g");
		snap_row("Rotation (°)", ed_rotation_snap,    1.0f,  180.f,  "%.4g");
		snap_row("Scale",        ed_scale_snap,        0.05f, 64.f,   "%.4g");
		ImGui::EndPopup();
	}

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

	if (ImGui::BeginPopup("##ot_view")) {
		static float s_ot_scale = 1.f, s_ot_alpha = 1.f, s_ot_mip = 0.f;

		const auto ov2 = idraw->editor_get_debug_overlay_state();

		bool cur_has_mip = false;
		if (ov2.active_tex)
			for (auto& o : ot_list)
				if (strcmp(o.tex, ov2.active_tex) == 0) { cur_has_mip = o.has_mip; break; }

		bool params_changed = false;
		ImGui::SetNextItemWidth(180.f);
		if (ImGui::SliderFloat("Scale", &s_ot_scale, 0.1f, 4.f)) params_changed = true;
		ImGui::SetNextItemWidth(180.f);
		if (ImGui::SliderFloat("Alpha", &s_ot_alpha, 0.f,  1.f)) params_changed = true;
		if (cur_has_mip) {
			ImGui::SetNextItemWidth(180.f);
			if (ImGui::SliderFloat("Mip / Slice", &s_ot_mip, 0.f, 7.f)) params_changed = true;
		}
		if (params_changed && ov2.active_tex)
			idraw->editor_set_debug_overlay(ov2.active_tex, s_ot_scale, s_ot_alpha, s_ot_mip);

		ImGui::Separator();

		if (ImGui::Selectable("Off", !ov2.active_tex, ImGuiSelectableFlags_DontClosePopups))
			idraw->editor_clear_debug_overlay();

		ImGui::Separator();

		for (auto& o : ot_list) {
			const bool sel = ov2.active_tex && strcmp(o.tex, ov2.active_tex) == 0;
			if (ImGui::Selectable(o.label, sel, ImGuiSelectableFlags_DontClosePopups)) {
				s_ot_scale = 1.f; s_ot_alpha = 1.f; s_ot_mip = 0.f;
				idraw->editor_set_debug_overlay(o.tex, s_ot_scale, s_ot_alpha, s_ot_mip);
			}
		}
		ImGui::EndPopup();
	}
}

// Ctrl+P "Set Parent" and Alt+P "Clear Parent" popups. Parenting is prefab-only; the shortcuts in
// check_inputs already gate on is_editing_prefab(). All actions route through the undoable
// ParentToCommand.
void EditorDoc::draw_parenting_popups() {
	if (want_open_parent_menu) {
		ImGui::OpenPopup("##parent_menu");
		want_open_parent_menu = false;
	}
	if (want_open_unparent_menu) {
		ImGui::OpenPopup("##unparent_menu");
		want_open_unparent_menu = false;
	}

	std::vector<Entity*> selected;
	for (auto& ptr : selection_state->get_selection_as_vector())
		if (Entity* e = ptr.get())
			selected.push_back(e);
	Entity* active = selection_state->get_active().get();

	auto children_excluding = [&](Entity* target) {
		std::vector<Entity*> out;
		for (auto* e : selected)
			if (e != target)
				out.push_back(e);
		return out;
	};

	if (ImGui::BeginPopup("##parent_menu")) {
		ImGui::TextDisabled("Set Parent");
		ImGui::Separator();

		const bool can_parent_active = active && selected.size() >= 2;
		if (ImGui::MenuItem("Parent to Active", nullptr, false, can_parent_active))
			command_mgr->add_command(new ParentToCommand(*this, children_excluding(active), active, false, false));
		if (ImGui::MenuItem("Parent to New Empty", nullptr, false, !selected.empty()))
			command_mgr->add_command(new ParentToCommand(*this, selected, nullptr, true, false));

		// "Parent to Bone" — only when the active target carries a skeletal mesh. Searchable combo
		// (same pattern as EntityBoneParentStringEditor): opening focuses the filter box so you can
		// type; clicking away closes the combo without issuing a command (skips parenting).
		MeshComponent* mesh = active ? active->get_component<MeshComponent>() : nullptr;
		const MSkeleton* skel = (mesh && mesh->get_model()) ? mesh->get_model()->get_skel() : nullptr;
		if (skel && can_parent_active) {
			if (ImGui::BeginCombo("Parent to Bone", "<pick a bone>")) {
				if (parent_bone_focus_filter) {
					ImGui::SetKeyboardFocusHere();
					parent_bone_focus_filter = false;
				}
				ImGui::InputText("##bonefilter", (char*)parent_bone_filter_buf.c_str(),
								 parent_bone_filter_buf.size() + 1, ImGuiInputTextFlags_CallbackResize,
								 imgui_std_string_resize, &parent_bone_filter_buf);

				const std::string filter_lower = StringUtils::to_lower(parent_bone_filter_buf.c_str());
				for (auto& bone : skel->get_all_bones()) {
					const char* label = bone.strname.empty() ? bone.name.get_c_str() : bone.strname.c_str();
					if (!label)
						label = "<bone>";
					if (!filter_lower.empty() &&
						StringUtils::to_lower(label).find(filter_lower) == std::string::npos)
						continue;
					if (ImGui::Selectable(label)) {
						command_mgr->add_command(
							new ParentToCommand(*this, children_excluding(active), active, false, false, bone.name));
						ImGui::CloseCurrentPopup(); // dismiss the whole parent menu after picking
					}
				}
				ImGui::EndCombo();
			} else {
				// Combo closed (clicked away or never opened): reset so it re-focuses next open, and
				// clear the stale filter text. No command issued -> parenting skipped, as requested.
				parent_bone_focus_filter = true;
				parent_bone_filter_buf.clear();
			}
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopup("##unparent_menu")) {
		ImGui::TextDisabled("Clear Parent");
		ImGui::Separator();
		if (ImGui::MenuItem("Clear Parent (Keep Transform)", nullptr, false, !selected.empty()))
			command_mgr->add_command(new ParentToCommand(*this, selected, nullptr, false, true));
		ImGui::EndPopup();
	}
}

// Opens the recent doc named by recent_switcher_index (same "recent N" command as the Open Recent
// menu, deferred through Cmd_Manager since imgui-time teardown of this doc isn't safe) and closes
// the switcher. If the selected entry is the document already open (e.g. only one other doc has
// ever been visited and the user tabs back to where they started), treat it as a cancel instead of
// re-loading the current map.
void EditorDoc::confirm_recent_switcher() {
	auto entry = g_editor_recents.at_slot(recent_switcher_index + 1);
	if (entry && entry->path != get_asset_path()) {
		std::string cmd = "recent " + std::to_string(recent_switcher_index + 1);
		Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, cmd.c_str());
	}
	recent_switcher_open = false;
}

// Ctrl+Tab quick-switcher popup. Selection is driven entirely by recent_switcher_index (set in
// check_recent_switcher_input) rather than imgui nav, so held-Ctrl+Tab cycling and Up/Down agree
// with what's highlighted. Closing via check_inputs (Escape/Enter/Ctrl-release) is signaled by
// recent_switcher_open going false, which we notice here and turn into CloseCurrentPopup().
void EditorDoc::draw_recent_switcher_popup() {
	if (want_open_recent_switcher) {
		ImGui::OpenPopup("##recent_switcher");
		want_open_recent_switcher = false;
	}

	if (ImGui::BeginPopup("##recent_switcher")) {
		if (!recent_switcher_open) {
			ImGui::CloseCurrentPopup();
		} else {
			ImGui::TextDisabled("Switch Document (Ctrl+Tab)");
			ImGui::Separator();
			const int count = g_editor_recents.size();
			// Selection highlight is driven entirely by recent_switcher_index (see
			// check_recent_switcher_input), not imgui's own nav focus. NoNav keeps imgui's nav
			// highlight rectangle from independently drifting with the same arrow-key presses,
			// which otherwise showed up as two highlighted rows moving out of sync.
			ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
			for (int slot = 1; slot <= count; ++slot) {
				auto entry = g_editor_recents.at_slot(slot);
				if (!entry) continue;
				const bool is_sel = (slot - 1) == recent_switcher_index;
				if (ImGui::Selectable(entry->path.c_str(), is_sel))
					confirm_recent_switcher();
			}
			ImGui::PopItemFlag();
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

			float mxf = 0.f, myf = 0.f;
			SDL_GetMouseState(&mxf, &myf);
			int x = (int)mxf, y = (int)myf;
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
				} else if (asset_class_type->is_a(PrefabAsset::StaticType)) {
					drag_drop_preview->set_preview_prefab(resource->filename, drop_transform);
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

	draw_scene_context_menu();
	make_prefab_path_popup.draw();
}

// Builds a menu tree out of the flat, '/'-delimited EditorAddMenuRegistry entries (see
// Game/EditorAddMenu.h) and draws it as nested ImGui submenus, e.g. "NPCs/Enemy" becomes a "NPCs"
// submenu containing an "Enemy" item.
namespace {
struct AddMenuNode {
	std::string name;
	std::vector<AddMenuNode> children;
	const EditorAddMenuEntry* leaf = nullptr;
};

AddMenuNode build_add_menu_tree(const std::vector<EditorAddMenuEntry>& entries) {
	AddMenuNode root;
	for (auto& entry : entries) {
		AddMenuNode* cur = &root;
		size_t start = 0;
		for (;;) {
			const size_t slash = entry.menu_path.find('/', start);
			const bool is_last = slash == std::string::npos;
			const std::string segment = entry.menu_path.substr(start, is_last ? std::string::npos : slash - start);

			auto found = std::find_if(cur->children.begin(), cur->children.end(),
									  [&](const AddMenuNode& n) { return n.name == segment; });
			if (found == cur->children.end()) {
				cur->children.push_back(AddMenuNode{segment});
				found = std::prev(cur->children.end());
			}
			cur = &(*found);
			if (is_last) {
				cur->leaf = &entry;
				break;
			}
			start = slash + 1;
		}
	}
	return root;
}

void draw_add_menu_node(const AddMenuNode& node, EditorDoc& doc, const glm::mat4& transform) {
	for (auto& child : node.children) {
		if (child.leaf) {
			if (ImGui::MenuItem(child.name.c_str()))
				doc.command_mgr->add_command(new CreateCppClassCommand(doc, child.leaf->classname, transform,
																		 EntityPtr(), child.leaf->is_component));
		} else if (ImGui::BeginMenu(child.name.c_str())) {
			draw_add_menu_node(child, doc, transform);
			ImGui::EndMenu();
		}
	}
}
} // namespace

void EditorDoc::draw_add_menu_tree(const std::vector<EditorAddMenuEntry>& entries, const glm::mat4& transform) {
	const AddMenuNode root = build_add_menu_tree(entries);
	draw_add_menu_node(root, *this, transform);
}

// Right-click scene context menu. Opened by check_scene_context_menu_input() (EditorDocInput.cpp) on
// a still right-click; runs here (inside the "Scene viewport" imgui window, see GUISystemLocal.cpp)
// so OpenPopup/BeginPopup share the same window ID namespace. Spawns land at
// scene_context_menu_transform, the world position raycast under the cursor at click time.
void EditorDoc::draw_scene_context_menu() {
	if (want_open_scene_context_menu) {
		ImGui::OpenPopup("##scene_ctx_menu");
		want_open_scene_context_menu = false;
	}
	if (!ImGui::BeginPopup("##scene_ctx_menu")) {
		scene_ctx_menu_has_open_submenu = false;
		return;
	}

	// -- Blender-style auto-close: if the mouse has wandered far enough outside the popup's own
	// rect, close it -- unless a submenu (e.g. "Add") is open, since navigating into one can put the
	// mouse well outside the root popup's rect. scene_ctx_menu_has_open_submenu reflects last frame's
	// submenu state (set below, near the "Add" BeginMenu); a one-frame lag here is imperceptible.
	{
		const float close_margin_px = 70.0f;
		const ImVec2 wpos = ImGui::GetWindowPos();
		const ImVec2 wsize = ImGui::GetWindowSize();
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool outside = mouse.x < wpos.x - close_margin_px || mouse.x > wpos.x + wsize.x + close_margin_px ||
							  mouse.y < wpos.y - close_margin_px || mouse.y > wpos.y + wsize.y + close_margin_px;
		if (outside && !scene_ctx_menu_has_open_submenu) {
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			return;
		}
	}
	scene_ctx_menu_has_open_submenu = false;

	// Single selected prefab instance, if any -- used both for the header below and "Edit Prefab".
	PrefabAssetComponent* single_selected_prefab = nullptr;
	if (selection_state->has_only_one_selected()) {
		Entity* e = selection_state->get_only_one_selected().get();
		if (e)
			single_selected_prefab = e->get_component<PrefabAssetComponent>();
	}

	// -- Header: the prefab's name, when right-clicking a single prefab instance.
	if (single_selected_prefab) {
		const std::string& path = single_selected_prefab->prefab_path;
		const auto slash = path.find_last_of('/');
		const std::string name = (slash == std::string::npos) ? path : path.substr(slash + 1);
		ImGui::TextDisabled("%s", name.c_str());
		ImGui::Separator();
	}

	// -- Parenting: same actions as the Ctrl+P / Alt+P popups (draw_parenting_popups), parenting is
	// prefab-only.
	if (is_editing_prefab()) {
		std::vector<Entity*> selected;
		for (auto& ptr : selection_state->get_selection_as_vector())
			if (Entity* e = ptr.get())
				selected.push_back(e);
		Entity* active = selection_state->get_active().get();
		auto children_excluding = [&](Entity* target) {
			std::vector<Entity*> out;
			for (auto* e : selected)
				if (e != target)
					out.push_back(e);
			return out;
		};

		const bool can_parent_active = active && selected.size() >= 2;
		if (ImGui::MenuItem("Parent to Active", "Ctrl+P", false, can_parent_active))
			command_mgr->add_command(new ParentToCommand(*this, children_excluding(active), active, false, false));
		if (ImGui::MenuItem("Parent to New Empty", nullptr, false, !selected.empty()))
			command_mgr->add_command(new ParentToCommand(*this, selected, nullptr, true, false));
		if (ImGui::MenuItem("Clear Parent (Keep Transform)", "Alt+P", false, !selected.empty()))
			command_mgr->add_command(new ParentToCommand(*this, selected, nullptr, false, true));
		ImGui::Separator();
	}

	// -- Unpack Prefab: replaces every selected PrefabAssetComponent instance with the loose entities
	// from its referenced .tprefab (reverse of instantiating one). Non-prefab entities in a mixed
	// selection are skipped by the command. Greyed out unless at least one selected entity qualifies.
	{
		std::vector<EntityPtr> prefab_instances;
		for (auto& ptr : selection_state->get_selection_as_vector())
			if (Entity* e = ptr.get())
				if (e->get_component<PrefabAssetComponent>())
					prefab_instances.push_back(ptr);
		if (ImGui::MenuItem("Unpack Prefab", nullptr, false, !prefab_instances.empty()))
			command_mgr->add_command(new UnpackPrefabCommand(*this, std::move(prefab_instances)));
	}

	// -- Edit Prefab: opens the selected prefab instance's .tprefab in its own editor tab, same as
	// double-clicking it in the asset browser. Only offered for a single selected prefab instance.
	if (single_selected_prefab) {
		if (ImGui::MenuItem("Edit Prefab"))
			Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND,
									   ("open-editor " + single_selected_prefab->prefab_path).c_str());
	}

	// -- Make Prefab Using...: inverse of Unpack Prefab. Serializes the selection into a new
	// .tprefab and replaces it in-place with a single PrefabAssetComponent instance (undoable).
	// Not valid while already editing a prefab (would nest a prefab reference inside itself).
	{
		auto selected = selection_state->get_selection_as_vector();
		if (ImGui::MenuItem("Make Prefab Using...", nullptr, false, !selected.empty() && !is_editing_prefab())) {
			std::string folder = AssetBrowser::inst ? AssetBrowser::inst->selected_folder : std::string();
			make_prefab_path_popup.open("Make Prefab Using...", folder, "new_prefab", ".tprefab",
				[this, selected](const std::string& path) {
					command_mgr->add_command(new MakePrefabAndReplaceCommand(*this, selected, path));
				});
		}
	}

	ImGui::Separator();

	// -- Add...: common built-in component types, plus any game-specific classes registered via
	// REGISTER_EDITOR_ADD_MENU_ENTRY (Game/EditorAddMenu.h), which may nest into submenus
	// (e.g. "NPCs/Enemy").
	if (ImGui::BeginMenu("Add")) {
		scene_ctx_menu_has_open_submenu = true;
		auto spawn_component = [&](const char* classname) {
			command_mgr->add_command(
				new CreateCppClassCommand(*this, classname, scene_context_menu_transform, EntityPtr(), true));
		};

		if (ImGui::MenuItem("Point Light"))     spawn_component("PointLightComponent");
		if (ImGui::MenuItem("Spot Light"))      spawn_component("SpotLightComponent");
		if (ImGui::MenuItem("Particle System")) spawn_component("ParticleSystemComponent");
		if (ImGui::MenuItem("GI Volume"))       spawn_component("GiVolumeComponent");
		if (ImGui::MenuItem("Cubemap Volume"))  spawn_component("CubemapComponent");
		if (ImGui::MenuItem("Mesh"))            spawn_component("MeshComponent");
		if (ImGui::MenuItem("Audio Player"))    spawn_component("SoundComponent");

		const auto& registered = EditorAddMenuRegistry::get().get_entries();
		if (!registered.empty()) {
			ImGui::Separator();
			draw_add_menu_tree(registered, scene_context_menu_transform);
		}

		ImGui::EndMenu();
	}

	ImGui::EndPopup();
}

// ---------------------------------------------------------------------------
// Asset usage search
// ---------------------------------------------------------------------------

// Recurses into a component's reflected properties looking for an AssetPtr field whose asset's
// gamepath matches `target` exactly. Also recurses into List-typed sub-properties, since a
// component may hold an array of asset refs. Same walk as
// ObjectOutlinerFilter.cpp's check_props_for_assetptr_or_entityptr, but an exact-match query
// instead of a substring search.
static bool component_references_asset(void* inst, const PropertyInfoList* list, const std::string& target) {
	if (!list)
		return false;
	for (int i = 0; i < list->count; i++) {
		auto& prop = list->list[i];
		if (prop.type == core_type_id::AssetPtr) {
			IAsset** asset = (IAsset**)prop.get_ptr(inst);
			if (*asset && (*asset)->get_name() == target)
				return true;
		} else if (prop.type == core_type_id::List) {
			auto listptr = prop.get_ptr(inst);
			auto size = prop.list_ptr->get_size(listptr);
			for (int j = 0; j < size; j++) {
				auto elem_ptr = prop.list_ptr->get_index(listptr, j);
				if (component_references_asset(elem_ptr, prop.list_ptr->props_in_list, target))
					return true;
			}
		}
	}
	return false;
}

void EditorDoc::select_entities_using_asset(const std::string& asset_gamepath) {
	std::vector<EntityPtr> matches;
	auto& objs = eng->get_level()->get_all_objects();
	for (auto obj : objs) {
		Entity* e = obj->cast_to<Entity>();
		if (!e)
			continue;
		bool found = false;
		for (auto& c : e->get_components()) {
			for (auto type = &c->get_type(); type && !found; type = type->super_typeinfo)
				found = component_references_asset(c, type->props, asset_gamepath);
			if (found)
				break;
		}
		if (found)
			matches.push_back(e->get_self_ptr());
	}

	if (matches.empty()) {
		sys_print(Info, "No entities in the open level reference '%s'\n", asset_gamepath.c_str());
		return;
	}
	selection_state->clear_all_selected();
	selection_state->add_entities_to_selection(matches);
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
				// Transform the model-space AABB by the full world transform (incl. scale) rather than
				// scaling only the sphere center — a raw model-space radius under-/over-shoots on
				// non-unit-scaled entities.
				Bounds ws_bounds = transform_bounds(ptr->get_ws_transform(), mesh->get_model()->get_bounds());
				pos = ws_bounds.get_center();
				radius = glm::max(glm::length(ws_bounds.bmax - ws_bounds.bmin) * 0.5f, 0.5f);
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
		if (ImGui::MenuItem("Import Model...")) {
			if (AssetBrowser::inst)
				AssetBrowser::inst->import_model_dialog();
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
			start_play_process(get_asset_path(), /*lua_debug=*/false, /*cpp_debug=*/false);
		}
		// Opens the lua-debug socket immediately and leaves it listening for the whole session
		// (no --lua_debug_wait) -- attach VS Code's "lua-debug Attach" config at any point during
		// play, see docs/scripting/vscode_debugger.md.
		if (ImGui::MenuItem("Play with Lua Debugger")) {
			start_play_process(get_asset_path(), /*lua_debug=*/true, /*cpp_debug=*/false);
		}
		// Spawns App.exe with --wait-for-debugger, which spins until a debugger attaches, and
		// auto-attaches VS via Scripts/_vs_attach.ps1 (DTE, falling back to vsjitdebugger.exe).
		if (ImGui::MenuItem("Play with C++ Debugger")) {
			start_play_process(get_asset_path(), /*lua_debug=*/false, /*cpp_debug=*/true);
		}

		ImGui::Separator();
		// Lists __typename of every entity the loader stashed because the class wasn't found.
		// Greyed out + tagged with count when empty so users learn it exists before they hit a problem.
		{
			auto level = eng->get_level();
			const int n = level ? (int)level->preserved_unknown_objs.size() : 0;
			const std::string label = "Unresolved Entities (" + std::to_string(n) + ")";
			if (ImGui::BeginMenu(label.c_str(), n > 0)) {
				ImGui::TextDisabled("Preserved verbatim on save; not editable here.");
				ImGui::Separator();
				for (const auto& blob : level->preserved_unknown_objs) {
					const char* type = "<no __typename>";
					std::string buf;
					auto it = blob.find("__typename");
					if (it != blob.end() && it->is_string()) {
						buf = it->get<std::string>();
						type = buf.c_str();
					}
					ImGui::TextUnformatted(type);
				}
				ImGui::EndMenu();
			}
		}
		// Lists every JSON key the loader saw but no reflected property consumed.
		// Same UX as the unresolved-entities menu: count in the label, greyed when zero.
		{
			auto level = eng->get_level();
			const int nf = level ? (int)level->unknown_field_warnings.size() : 0;
			const std::string label = "Unknown Fields (" + std::to_string(nf) + ")";
			if (ImGui::BeginMenu(label.c_str(), nf > 0)) {
				ImGui::TextDisabled("Loaded value was dropped; likely a typo or stale field.");
				ImGui::Separator();
				for (const auto& w : level->unknown_field_warnings)
					ImGui::TextUnformatted(w.c_str());
				ImGui::EndMenu();
			}
		}

		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Tools")) {
		if (ImGui::MenuItem("Asset Size Viewer")) {
			AssetSizeViewer::get().open();
		}
		if (ImGui::MenuItem("Diagnostics")) {
			DiagnosticsWindow::get().open();
		}
		if (ImGui::MenuItem("Profiler", nullptr, stat_profiler.get_bool())) {
			stat_profiler.set_bool(!stat_profiler.get_bool());
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

void EditorDoc::hook_menu_bar_file_menu() {
	if (ImGui::BeginMenu("Open Recent")) {
		const int count = g_editor_recents.size();
		if (count == 0) {
			ImGui::MenuItem("(empty)", nullptr, false, false);
		} else {
			for (int slot = 1; slot <= count; ++slot) {
				auto entry = g_editor_recents.at_slot(slot);
				if (!entry) continue;
				// imgui-time tear-down of the editor doc isn't safe; defer the
				// open through the command buffer (same pattern as AssetBrowser
				// double-click). Camera restore happens inside the `recent` handler.
				if (ImGui::MenuItem(entry->path.c_str())) {
					std::string cmd = "recent " + std::to_string(slot);
					Cmd_Manager::inst->execute(Cmd_Execute_Mode::APPEND, cmd.c_str());
				}
			}
		}
		ImGui::EndMenu();
	}
}

#endif // EDITOR_BUILD
