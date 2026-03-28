#pragma once

class SelectionState
{
public:
	SelectionState(EditorDoc& ed_doc);

	MulticastDelegate<> on_selection_changed;

<<<<<<< HEAD
	bool has_any_selected() const { return !selected_entity_handles.empty(); }

	int num_entities_selected() const { return selected_entity_handles.size(); }

	bool has_only_one_selected() const { return num_entities_selected() == 1; }

	EntityPtr get_only_one_selected() const {
		ASSERT(has_only_one_selected());
		return EntityPtr(*selected_entity_handles.begin());
	}

	const std::unordered_set<uint64_t>& get_selection() const { return selected_entity_handles; }
	std::vector<EntityPtr> get_selection_as_vector() const {
		std::vector<EntityPtr> out;
		for (auto e : selected_entity_handles) {
			out.push_back(EntityPtr(e));
			ASSERT(eng->get_object(e)->is_a<Entity>());
		}
		return out;
	}

	void add_entities_to_selection(const std::vector<EntityPtr>& ptrs) {
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
		}
		if (had_changes) {
			on_selection_changed.invoke();
		}
	}
	void add_to_entity_selection(EntityPtr ptr) { add_entities_to_selection({ ptr }); }
	void add_to_entity_selection(const Entity* e) { return add_to_entity_selection(e->get_self_ptr()); }
	void remove_from_selection(std::vector<EntityPtr> ptrs) {

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
	void remove_from_selection(EntityPtr ptr) {

		auto e = ptr.get();
		if (e) { // can be null
			e->selected_in_editor = false;
			e->set_ws_transform(e->get_ws_transform());
		}
		selected_entity_handles.erase(ptr.handle);
		on_selection_changed.invoke();
	}
	void remove_from_selection(const Entity* e) { remove_from_selection(e->get_self_ptr()); }

	void validate_selection() {
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

	void clear_all_selected() {
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
	void set_select_only_this(EntityPtr ptr) {
		clear_all_selected();
		add_to_entity_selection(ptr);
	}
	void set_select_only_this(const Entity* e) { set_select_only_this(e->get_self_ptr()); }

	bool is_entity_selected(EntityPtr ptr) const {
		return selected_entity_handles.find(ptr.handle) != selected_entity_handles.end();
	}
	bool is_entity_selected(const Entity* e) const { return is_entity_selected(e->get_self_ptr()); }
=======
	bool has_any_selected() const;

	int num_entities_selected() const;

	bool has_only_one_selected() const;

	EntityPtr get_only_one_selected() const;

	const std::unordered_set<uint64_t>& get_selection() const;
	std::vector<EntityPtr> get_selection_as_vector() const;

	void add_entities_to_selection(const std::vector<EntityPtr>& ptrs);
	void add_to_entity_selection(EntityPtr ptr);
	void add_to_entity_selection(const Entity* e);
	void remove_from_selection(std::vector<EntityPtr> ptrs);
	void remove_from_selection(EntityPtr ptr);
	void remove_from_selection(const Entity* e);

	void validate_selection();

	void clear_all_selected();
	void set_select_only_this(EntityPtr ptr);
	void set_select_only_this(const Entity* e);

	bool is_entity_selected(EntityPtr ptr) const;
	bool is_entity_selected(const Entity* e) const;
>>>>>>> temp-ssr-branch

private:
	void on_node_deleted() { validate_selection(); }

	void on_close() {
		selected_entity_handles.clear();
		on_selection_changed.invoke();
	}

	std::unordered_set<uint64_t> selected_entity_handles;
};