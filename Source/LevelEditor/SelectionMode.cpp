#include "EditorModes.h"
#include "EditorDocLocal.h"
#include "Game/Components/BillboardComponent.h"

void SelectionMode::tick(EditorInputs& inputs) {
	dragger.tick(inputs);

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

void SelectionMode::draw_ui()
{
	if (dragger.get_is_dragging()) {
		auto rect = dragger.get_drag_rect();
		// builder.draw_solid_rect({ rect.x,rect.y }, { rect.w,rect.h }, { 200,200,200,50 });
		rect.x -= UiSystem::inst->get_vp_rect().get_pos().x;
		rect.y -= UiSystem::inst->get_vp_rect().get_pos().y;

		RectangleShape shape;
		shape.rect = rect;
		Uint8 c = 200;// test1.get_integer();
		Uint8 a = 200;// test2.get_integer();
		shape.color = { c, c, c, a };
		UiSystem::inst->window.draw(shape);
	}
}
SelectionMode::SelectionMode(EditorDoc& doc) : doc(doc) {
	dragger.on_drag_end().add(this, [&](Rect2d rect) {
		auto* selection_state = doc.selection_state.get();
		auto type = MouseSelectionAction::ADD_SELECT;
		if (Input::is_shift_down())
			type = MouseSelectionAction::ADD_SELECT;
		else if (Input::is_ctrl_down())
			type = MouseSelectionAction::UNSELECT;
		else {
			selection_state->clear_all_selected();
		}

		auto newRect = doc.gui->convert_rect(rect);
		vector<EntityPtr> ents;
		if (doc.ed_cam.get_is_using_ortho()) {

			const Bounds camb = doc.ed_cam.get_ortho_selection_bounds(newRect);

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

				}
				else if (auto b = obj->cast_to<BillboardComponent>()) {
					if (!b->get_owner()->get_hidden_in_editor()) {
						auto thisbounds =
							Bounds(b->get_ws_position() - glm::vec3(0.5), b->get_ws_position() + glm::vec3(0.5));
						if (thisbounds.intersect(camb))
							ents.push_back(b->get_owner());
					}
				}
			}
			// rect.x to world space:

		}
		else {
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
		doc.do_mouse_selection(type, ents, true);

		doc.gui->do_box_select(type, dragger.get_drag_rect());
		});
}
