#include "SelectionState.h"
#include "EditorDocLocal.h"

SelectionState::SelectionState(EditorDoc& ed_doc) {
	ed_doc.post_node_changes.add(this, &SelectionState::on_node_deleted);
	ed_doc.on_close.add(this, &SelectionState::on_close);
}

 bool SelectionState::has_any_selected() const { return !selected_entity_handles.empty(); }

 int SelectionState::num_entities_selected() const { return selected_entity_handles.size(); }

 bool SelectionState::has_only_one_selected() const { return num_entities_selected() == 1; }

 EntityPtr SelectionState::get_only_one_selected() const {
	ASSERT(has_only_one_selected());
	return EntityPtr(*selected_entity_handles.begin());
}

 EntityPtr SelectionState::get_active() const {
	if (last_active.get() && is_entity_selected(last_active))
		return last_active;
	// stale/unset — fall back to any selected entity
	if (!selected_entity_handles.empty())
		return EntityPtr(*selected_entity_handles.begin());
	return EntityPtr();
}

 const std::unordered_set<uint64_t>& SelectionState::get_selection() const { return selected_entity_handles; }

 std::vector<EntityPtr> SelectionState::get_selection_as_vector() const {
	std::vector<EntityPtr> out;
	for (auto e : selected_entity_handles) {
		auto* obj = eng->get_object(e);
		if (!obj) continue; // stale handle — selection not yet validated; skip safely
		ASSERT(obj->is_a<Entity>());
		out.push_back(EntityPtr(e));
	}
	return out;
}

 void SelectionState::add_entities_to_selection(const std::vector<EntityPtr>& ptrs) {
	bool had_changes = false;
	for (EntityPtr ptr : ptrs) {
		ASSERT(eng->get_object(ptr.handle) && eng->get_object(ptr.handle)->is_a<Entity>());
		bool already_selected = is_entity_selected(ptr);
		if (!already_selected) {
			auto e = eng->get_entity(ptr.handle);
			e->selected_in_editor = true;
			e->set_ws_transform(e->get_ws_transform());
			selected_entity_handles.insert(ptr.handle);
			had_changes = true;
		}
		last_active = ptr; // most-recently referenced becomes the active target
	}
	if (had_changes) {
		on_selection_changed.invoke();
	}
}

 void SelectionState::add_to_entity_selection(EntityPtr ptr) { add_entities_to_selection({ ptr }); }

 void SelectionState::add_to_entity_selection(const Entity* e) { return add_to_entity_selection(e->get_self_ptr()); }

 void SelectionState::remove_from_selection(std::vector<EntityPtr> ptrs) {

	for (auto ptr : ptrs) {
		auto e = ptr.get();
		if (e) { // can be null
			e->selected_in_editor = false;
			e->set_ws_transform(e->get_ws_transform());
		}
		selected_entity_handles.erase(ptr.handle);
	}
	on_selection_changed.invoke();
}

 void SelectionState::remove_from_selection(EntityPtr ptr) {

	auto e = ptr.get();
	if (e) { // can be null
		e->selected_in_editor = false;
		e->set_ws_transform(e->get_ws_transform());
	}
	selected_entity_handles.erase(ptr.handle);
	on_selection_changed.invoke();
}

 void SelectionState::remove_from_selection(const Entity* e) { remove_from_selection(e->get_self_ptr()); }

 void SelectionState::validate_selection() {
	auto presize = selected_entity_handles.size();
	for (auto it = selected_entity_handles.begin(); it != selected_entity_handles.end();) {
		EntityPtr ptr(*it);
		auto ent = ptr.get();
		if (ent == nullptr) {
			it = selected_entity_handles.erase(it);
		}
		else {
			++it;
		}
	}
	if (selected_entity_handles.size() != presize)
		on_selection_changed.invoke();
}

 void SelectionState::clear_all_selected() {
	for (auto o : selected_entity_handles) {
		auto e = eng->get_entity(o);
		if (e) {
			e->selected_in_editor = false;
			e->set_ws_transform(e->get_ws_transform());
		}
	}
	selected_entity_handles.clear();

	on_selection_changed.invoke();
}

 void SelectionState::set_select_only_this(EntityPtr ptr) {
	clear_all_selected();
	add_to_entity_selection(ptr);
}

 void SelectionState::set_select_only_this(const Entity* e) { set_select_only_this(e->get_self_ptr()); }

 bool SelectionState::is_entity_selected(EntityPtr ptr) const {
	return selected_entity_handles.find(ptr.handle) != selected_entity_handles.end();
}

 bool SelectionState::is_entity_selected(const Entity* e) const { return is_entity_selected(e->get_self_ptr()); }
