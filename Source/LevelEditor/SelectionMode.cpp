#include "EditorModes.h"
#include "EditorDocLocal.h"

void SelectionMode::tick(EditorInputs& inputs) {
	doc.dragger.tick(inputs,true);

	const bool mouse1rel = Input::was_mouse_released(0);
	const bool has_shift = Input::is_shift_down();
	const bool has_ctrl = Input::is_ctrl_down();

	if (inputs.get_focused())
		return;

	auto selection_state = doc.selection_state.get();
	auto command_mgr = doc.command_mgr.get();
	if (mouse1rel && UiSystem::inst->is_vp_hovered() && inputs.can_use_mouse_click()) {
		doc.on_mouse_pick();
		//	ASSERT(!doc.inputs.can_use_mouse_click());
		// return;
	}

	if (!UiSystem::inst->is_vp_focused()) {
		return;
	}

	if (Input::was_key_pressed(SDL_SCANCODE_DELETE)) {
		if (doc.selection_state->has_any_selected()) {
			auto selected_handles = doc.selection_state->get_selection_as_vector();
			if (!selected_handles.empty()) {
				RemoveEntitiesCommand* cmd = new RemoveEntitiesCommand(doc, selected_handles);
				doc.command_mgr->add_command(cmd);
			}
		}
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_D) && has_shift) {
		if (selection_state->has_any_selected()) {
			auto selected_handles = selection_state->get_selection_as_vector();
			;
			DuplicateEntitiesCommand* cmd = new DuplicateEntitiesCommand(doc, selected_handles);
			command_mgr->add_command(cmd);
		}
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_A)) {
		// select all objects
		auto& objs = eng->get_level()->get_all_objects();
		std::vector<EntityPtr> selectThese;
		for (auto o : objs) {
			if (auto as_ent = o->cast_to<Entity>()) {
				if (as_ent->get_hidden_in_editor())
					continue;
				selectThese.push_back(as_ent);
			}
		}
		selection_state->add_entities_to_selection(selectThese);
	}
	else if (Input::was_key_pressed(SDL_SCANCODE_I) && has_ctrl) {
		// invert selection
		auto& objs = eng->get_level()->get_all_objects();
		std::vector<EntityPtr> selectThese;
		for (auto o : objs) {
			if (auto as_ent = o->cast_to<Entity>()) {
				if (as_ent->get_hidden_in_editor())
					continue;
				if (!selection_state->is_entity_selected(as_ent)) {
					selectThese.push_back(as_ent);
				}
			}
		}
		selection_state->clear_all_selected();
		selection_state->add_entities_to_selection(selectThese);
	}
}
