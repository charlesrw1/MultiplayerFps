#pragma once

class SelectionState
{
public:
	SelectionState(EditorDoc& ed_doc);

	MulticastDelegate<> on_selection_changed;

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

private:
	void on_node_deleted() { validate_selection(); }

	void on_close() {
		selected_entity_handles.clear();
		on_selection_changed.invoke();
	}

	std::unordered_set<uint64_t> selected_entity_handles;
};