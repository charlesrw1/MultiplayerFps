#pragma once
class IEditorCameraApi
{
public:
	virtual void set_look_at(glm::vec3 pos, glm::vec3 look) = 0;
	virtual glm::vec3 get_positon() const = 0;
	virtual Ray unproject_ray(int x, int y) const = 0;
	virtual bool is_ortho() const = 0;
	virtual View_Setup get_view_setup() const = 0;
};
class ISelectionApi
{
public:
	// INTERFACE
	virtual std::vector<EntityPtr> get_selected() const = 0;
	virtual viewMulticastDelegate<> on_selection_changed() const = 0;
	virtual void clear_selected() = 0;
	virtual void add_select(EntityPtr ptr) = 0;
	virtual void remove_select(EntityPtr ptr) = 0;
	virtual bool is_selected(EntityPtr ptr) const = 0;

	// HELPERS
	void do_selection(MouseSelectionAction action, EntityPtr ptr) { do_selection(action, std::vector<EntityPtr>{ptr}); }
	void do_selection(MouseSelectionAction action, std::vector<EntityPtr> ptrs);
};
class IDocumentApi
{
public:
	virtual void save() = 0;
	virtual void undo() = 0;
	virtual void redo() = 0;
	virtual std::string get_document_name() const = 0;
};

class EntitySnapshot{
public:
private:
	std::string serialized;
};
class ICommandApi
{
public:
	// spawn your entity, intialize it, then call this.
	virtual void add_spawned_entity_command(Entity* e) = 0;
	virtual void remove_entity(Entity* e) = 0;
	virtual EntitySnapshot make_entity_snapshot(Entity* e) = 0;
	virtual void commit_entity_changes(Entity* e) = 0;
	virtual void set_editor_hidden(Entity* e) = 0;
};
class IEditorApi2
{
public:
	virtual IEditorCameraApi* camera() = 0;
	virtual ISelectionApi* selection() = 0;
	virtual IDocumentApi* document() = 0;
};
