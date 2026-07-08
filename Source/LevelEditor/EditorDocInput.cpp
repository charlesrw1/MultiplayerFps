// Input handling and mouse selection for EditorDoc.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "EditorRecents.h"
#include "imgui.h"
#include "Framework/Files.h"
#include "Input/InputSystem.h"
#include "Render/DrawPublic.h"
#include "UI/GUISystemPublic.h"
#include "LevelEditor/Commands.h"
#include "Debug.h"
#include <SDL3/SDL_timer.h>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

const Entity* select_outermost_entity(const Entity* in) {
	ASSERT(in);
	const Entity* sel = in;
	while (sel) {
		if (!sel->dont_serialize_or_edit) {
			break;
		}
		sel = sel->get_parent();
	}
	return sel;
}

// ---------------------------------------------------------------------------
// Mouse-pick / selection
// ---------------------------------------------------------------------------

void EditorDoc::do_mouse_selection(MouseSelectionAction action, const Entity* e, bool select_rootmost_entity) {
	ASSERT(e);
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
	ASSERT(selection_state);
	if (select_root_most_entity) {
		// Match single-click picking: map each hit to its outermost editable ancestor, dropping
		// dont_serialize_or_edit entities (e.g. prefab-internal nodes) so box-select can't grab them.
		vector<EntityPtr> mapped;
		mapped.reserve(ents.size());
		for (auto& ep : ents) {
			const Entity* e = ep.get();
			if (!e)
				continue;
			if (const Entity* outer = select_outermost_entity(e))
				mapped.push_back(EntityPtr(outer));
		}
		ents = std::move(mapped);
	}
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
	ASSERT(selection_state);
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

// ---------------------------------------------------------------------------
// Right-click scene context menu (press/release tracking)
// ---------------------------------------------------------------------------

// A short, still right-click (released quickly, with barely any mouse movement while held) opens
// the scene context menu; a right-click-drag is the existing fly-camera behavior and must not open
// it. Movement is measured via accumulated relative motion (Input::get_mouse_delta()), not absolute
// screen position: while the fly camera is held it captures the mouse (relative/warped mode, see
// UiSystem::set_game_capture_mouse in EditorCamera::tick), so the OS cursor position barely changes
// even during a big look-around drag -- only the relative deltas reflect the real movement. Runs
// every frame independent of EditorInputs focus, since the camera grabs focus unconditionally on
// right-mouse-down (see EditorCamera::tick in LevelEditorCamera.cpp) and releases it again before
// this runs on the same frame the button comes back up.
void EditorDoc::check_scene_context_menu_input() {
	// Button index 2 is right mouse in this codebase's convention (0=left, 1=middle, 2=right; see
	// Input::tick()'s SDL_BUTTON_MASK(i+1) loop and fpsDebugCamera.cpp's `rmb = is_mouse_down(2)`).
	if (Input::was_mouse_pressed(2)) {
		// If the manipulate tool just used this same right-click to cancel a forced (G/R/S) transform
		// and reset it back (Blender-style), don't also treat it as a context-menu click.
		const bool used_by_manipulate = manipulate && manipulate->consume_right_click_cancel_flag();
		rmb_press_tracking = !used_by_manipulate && UiSystem::inst->is_vp_hovered();
		rmb_press_time_ms = SDL_GetTicks();
		rmb_drag_accum_px = glm::ivec2(0, 0);
	}
	if (rmb_press_tracking) {
		const glm::ivec2 d = Input::get_mouse_delta();
		rmb_drag_accum_px.x += std::abs(d.x);
		rmb_drag_accum_px.y += std::abs(d.y);
	}
	if (Input::was_mouse_released(2) && rmb_press_tracking) {
		rmb_press_tracking = false;

		const int drag_threshold_px = 4;
		const Uint64 click_duration_threshold_ms = 350;
		const bool held_still = rmb_drag_accum_px.x <= drag_threshold_px && rmb_drag_accum_px.y <= drag_threshold_px;
		const bool was_quick = (SDL_GetTicks() - rmb_press_time_ms) <= click_duration_threshold_ms;
		if (held_still && was_quick) {
			// Same raycast-to-world logic as the asset drag-drop drop point (hook_scene_viewport_draw).
			const glm::ivec2 release_pos = Input::get_mouse_pos();
			const auto vp_pos = UiSystem::inst->get_vp_rect().get_pos();
			const float scene_depth =
				idraw->get_scene_depth_for_editor(release_pos.x - vp_pos.x, release_pos.y - vp_pos.y);
			const glm::vec3 dir = unproject_mouse_to_ray(release_pos.x, release_pos.y).dir;
			const glm::vec3 worldpos = (abs(scene_depth) > 50.0f) ? vs_setup.origin - dir * 25.0f
																   : vs_setup.origin + dir * scene_depth;

			scene_context_menu_transform = glm::mat4(1.f);
			scene_context_menu_transform[3] = glm::vec4(worldpos, 1.0f);
			want_open_scene_context_menu = true;
		}
	}
}

// ---------------------------------------------------------------------------
// Keyboard input / shortcuts
// ---------------------------------------------------------------------------

// Ctrl+Tab quick-switcher (see want_open_recent_switcher in EditorDocLocal.h). Owns keyboard input
// for the frame while open so it doesn't fight with the shortcuts below (e.g. Ctrl+Z while cycling).
bool EditorDoc::check_recent_switcher_input(bool has_ctrl, bool has_shift) {
	if (!recent_switcher_open) {
		if (Input::was_key_pressed(SDL_SCANCODE_TAB) && has_ctrl && g_editor_recents.size() > 0) {
			recent_switcher_open = true;
			want_open_recent_switcher = true;
			recent_switcher_index = 0;
		}
		return recent_switcher_open;
	}

	const int count = g_editor_recents.size();
	if (count == 0) {
		recent_switcher_open = false;
		return false;
	}

	if (Input::was_key_pressed(SDL_SCANCODE_TAB))
		recent_switcher_index = has_shift ? (recent_switcher_index - 1 + count) % count
		                                   : (recent_switcher_index + 1) % count;
	else if (Input::was_key_pressed(SDL_SCANCODE_DOWN))
		recent_switcher_index = (recent_switcher_index + 1) % count;
	else if (Input::was_key_pressed(SDL_SCANCODE_UP))
		recent_switcher_index = (recent_switcher_index - 1 + count) % count;

	if (Input::was_key_pressed(SDL_SCANCODE_ESCAPE))
		recent_switcher_open = false; // cancel, no switch
	else if (Input::was_key_pressed(SDL_SCANCODE_RETURN) || !has_ctrl)
		confirm_recent_switcher(); // Enter, or Ctrl released alt-tab style

	return true;
}

void EditorDoc::check_inputs() {
	ASSERT(command_mgr);
	// Mouse-driven, not keyboard: must run even while a text field elsewhere has keyboard focus.
	check_scene_context_menu_input();

	const bool is_keyboard_blocked = UiSystem::inst->blocking_keyboard_inputs();
	if (is_keyboard_blocked)
		return;

	const bool has_shift = Input::is_shift_down();
	const bool has_ctrl = Input::is_ctrl_down();

	if (check_recent_switcher_input(has_ctrl, has_shift))
		return;

	if (Input::was_key_pressed(SDL_SCANCODE_Z) && has_ctrl) {
		command_mgr->undo();
	} else if (Input::was_key_pressed(SDL_SCANCODE_S) && has_ctrl) {
		save();
	} else if (Input::was_key_pressed(SDL_SCANCODE_P) && has_ctrl && is_editing_prefab()) {
		// Ctrl+P: open the "Set Parent" popup (parenting is prefab-only).
		if (selection_state->has_any_selected())
			want_open_parent_menu = true;
	} else if (Input::was_key_pressed(SDL_SCANCODE_P) && Input::is_alt_down() && is_editing_prefab()) {
		// Alt+P: open the "Clear Parent" popup.
		if (selection_state->has_any_selected())
			want_open_unparent_menu = true;
	} else if (ed_cam.handle_events()) {
	}
}

#endif // EDITOR_BUILD
