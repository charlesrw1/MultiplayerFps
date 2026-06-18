// Scene object management, DrawHandlesObject, EntityVisiblityFilter, and API impls.
// Logical split from EditorDocLocal.cpp.
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "imgui.h"
#include "Input/InputSystem.h"
#include "Render/DrawPublic.h"
#include "UI/GUISystemPublic.h"
#include "Game/EntityComponent.h"
#include "Game/Components/LightComponents.h"
#include "Game/Components/DecalComponent.h"
#include "Game/Components/SpawnerComponenth.h"
#include "Debug.h"

// ---------------------------------------------------------------------------
// Scene entity helpers
// ---------------------------------------------------------------------------

Entity* EditorDoc::spawn_entity() {
	ASSERT(eng->get_level());
	Entity* e = eng->get_level()->spawn_entity();
	instantiate_into_scene(e);
	return e;
}

Component* EditorDoc::attach_component(const ClassTypeInfo* ti, Entity* e) {
	ASSERT(ti);
	ASSERT(e);
	Component* c = e->create_component(ti);
	instantiate_into_scene(c);
	return c;
}

void EditorDoc::remove_scene_object(BaseUpdater* u) {
	ASSERT(u);
	u->destroy_deferred();
}

void EditorDoc::insert_unserialized_into_scene(UnserializedSceneFile& file) {
	ASSERT(eng->get_level());
	eng->get_level()->insert_unserialized_entities_into_level(file);
}

void EditorDoc::instantiate_into_scene(BaseUpdater* u) {
	ASSERT(u || !u); // u may legitimately be null in some call paths
}

// ---------------------------------------------------------------------------
// DrawHandlesObject
// ---------------------------------------------------------------------------

void DrawHandlesObject::tick() {
	ASSERT(&doc);

	// draw gizmos for selected entities' components
	for (auto& sel : doc.selection_state->get_selection_as_vector()) {
		if (auto* e = sel.get()) {
			for (auto* comp : e->get_components())
				comp->editor_on_draw_gizmos_selected();
		}
	}

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

// ---------------------------------------------------------------------------
// EntityVisiblityFilter
// ---------------------------------------------------------------------------

void EntityVisiblityFilter::tick() {
	ASSERT(&doc);
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

// ---------------------------------------------------------------------------
// IEditorTool factory
// ---------------------------------------------------------------------------

IEditorTool* IEditorTool::create(string mapname) {
	ASSERT(!mapname.empty() || mapname.empty()); // mapname may be empty for new scenes
	return EditorDoc::create_scene(mapname);
}

// ---------------------------------------------------------------------------
// ISelectionApi helpers
// ---------------------------------------------------------------------------

void ISelectionApi::do_selection(MouseSelectionAction action, std::vector<EntityPtr> ptrs) {
	ASSERT(ptrs.empty() || !ptrs.empty()); // always valid to pass an empty list
	if (action == MouseSelectionAction::ADD_SELECT) {
		for (EntityPtr p : ptrs)
			add_select(p);
	} else if (action == MouseSelectionAction::SELECT_ONLY) {
		clear_selected();
		for (EntityPtr p : ptrs)
			add_select(p);
	} else {
		for (EntityPtr p : ptrs)
			remove_select(p);
	}
}

// ---------------------------------------------------------------------------
// DocumentApiImpl
// ---------------------------------------------------------------------------

void DocumentApiImpl::save() {
	ASSERT(doc);
	doc->save();
}

void DocumentApiImpl::undo() {
	ASSERT(doc);
	if (doc->command_mgr)
		doc->command_mgr->undo();
}

void DocumentApiImpl::redo() {
	ASSERT(doc);
	// TODO: Implement redo when UndoRedoSystem supports it
}

std::string DocumentApiImpl::get_document_name() const {
	ASSERT(doc);
	return doc->get_doc_name();
}

#endif // EDITOR_BUILD
