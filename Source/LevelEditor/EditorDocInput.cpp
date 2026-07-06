// Input handling and mouse selection for EditorDoc.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "imgui.h"
#include "Framework/Files.h"
#include "Input/InputSystem.h"
#include "Render/DrawPublic.h"
#include "UI/GUISystemPublic.h"
#include "LevelEditor/Commands.h"
#include "Debug.h"

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
// Keyboard input / shortcuts
// ---------------------------------------------------------------------------

void EditorDoc::check_inputs() {
	ASSERT(command_mgr);
	const bool is_keyboard_blocked = UiSystem::inst->blocking_keyboard_inputs();
	if (is_keyboard_blocked)
		return;

	const bool has_shift = Input::is_shift_down();
	const bool has_ctrl = Input::is_ctrl_down();

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
