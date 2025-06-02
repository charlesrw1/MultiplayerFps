#pragma once
#ifdef EDITOR_BUILD
#include "EditorDocLocal.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/MeshComponent.h"
#include <memory>

#include "Game/BaseUpdater.h"
#include "LevelSerialization/SerializationAPI.h"
#include "Scripting/ScriptComponent.h"
#include "Game/EntityPtr.h"

class CommandSerializeUtil
{
public:
	static std::unique_ptr<SerializedSceneFile> serialize_entities_text(std::vector<EntityPtr> handles);
};


class MakePrefabEditable : public Command
{
public:
	MakePrefabEditable(Entity* me, bool editable) {
		is_valid_flag = me && me->is_root_of_prefab && me->what_prefab && me->what_prefab != ed_doc.get_editing_prefab();
		if (!is_valid_flag) return;
		ptr = me->get_self_ptr();
		value = editable;
	}
	void execute() final {
		if (ptr) {
			ptr->set_prefab_editable(value);
			ed_doc.post_node_changes.invoke();
		}
	}
	void undo() final {
		if (ptr) {
			ptr->set_prefab_editable(!value);
			ed_doc.post_node_changes.invoke();
		}
	}
	std::string to_string() final {
		return "MakePrefabEditable";
	}
	bool is_valid_flag = true;
	EntityPtr ptr;
	bool value = false;
	bool is_valid() final { return is_valid_flag; }
};

class RemoveEntitiesCommand : public Command
{
public:
	RemoveEntitiesCommand(std::vector<EntityPtr> handles);
	bool is_valid_flag = true;
	bool is_valid() final {
		return is_valid_flag;
	}

	void execute() final {
		ASSERT(is_valid());
		for (auto h : handles) {
			h->destroy();
		}
		ed_doc.post_node_changes.invoke();
	}
	void undo() final;
	std::string to_string() final {
		return "Remove Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr> handles;
};

class ParentToCommand : public Command
{
public:

	ParentToCommand(std::vector<Entity*> ents, Entity* parent_to, bool create_new_parent, bool delete_parent);
	bool is_valid_flag = true;
	bool is_valid() final {
		return is_valid_flag;
	}


	void execute() final;
	void undo() final;
	std::string to_string() override {
		return "Parent To";
	}

	std::vector<EntityPtr> prev_parents;
	std::vector<EntityPtr> entities;
	EntityPtr parent_to;
	EntityPtr parent_to_prev_parent;
	bool create_new_parent = false;
	bool delete_the_parent = false;

	// anti pattern?
	std::unique_ptr<RemoveEntitiesCommand> remove_the_parent_cmd;
};

class CreatePrefabCommand : public Command
{
public:
	CreatePrefabCommand(const std::string& prefab_name, const glm::mat4& transform, EntityPtr parent = EntityPtr());
	~CreatePrefabCommand() override {
	}

	void execute();
	void undo() {
		handle->destroy();
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "Create Prefab";
	}
	EntityPtr parent_to;
	EntityPtr handle;
	std::string prefab_name;
	glm::mat4 transform;
};

class CreateStaticMeshCommand : public Command
{
public:
	CreateStaticMeshCommand(const std::string& modelname, const glm::mat4& transform, EntityPtr parent = EntityPtr());
	~CreateStaticMeshCommand() override {
	}

	bool is_valid() final { return true; }

	void execute() final;
	void undo() final {
		handle->destroy();
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() final {
		return "Create StaticMesh";
	}
	EntityPtr parent_to;
	EntityPtr handle;
	glm::mat4 transform;
	std::string modelname{};
};
class CreateCppClassCommand : public Command
{
public:
	CreateCppClassCommand(const std::string& cppclassname, const glm::mat4& transform, EntityPtr parent, bool is_component);
	bool is_valid() final { return ti != nullptr; }

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Create Class";
	}
	const ClassTypeInfo* ti = nullptr;
	glm::mat4 transform;
	EntityPtr handle;
	EntityPtr parent_to;
	bool is_component_type = false;
};


class TransformCommand : public Command
{
public:
	TransformCommand(const std::unordered_set<uint64_t>& selection, const std::unordered_map<uint64_t, glm::mat4>& pre_transforms);
	void execute() final;
	void undo() final {
		for (auto& t : transforms) {
			if (t.ptr.get()) {
				t.ptr->set_ws_transform(t.pre_transform);
			}
		}
		ed_doc.selection_state->on_selection_changed.invoke();	// hack
	}
	std::string to_string() final {
		return "Transform Entities";
	}
	bool skip_this_time = false;
	struct pre_and_post {
		EntityPtr ptr;
		glm::mat4 pre_transform;
		glm::mat4 post_transform;
	};
	std::vector<pre_and_post> transforms;
};
class InstantiatePrefabCommand : public Command
{
public:
	InstantiatePrefabCommand(Entity* e);

	bool is_valid() final {
		return asset != nullptr;
	}

	void execute_R(Entity* e);

	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Instantiate Prefab";
	}
	struct created_obj {
		uint64_t eng_handle = 0;
		uint64_t unique_file_id = 0;
	};
	std::vector<created_obj> created_objs;
	EntityPtr me;
	PrefabAsset* asset = nullptr;
	EntityPtr creator_source;
};

class DuplicateEntitiesCommand : public Command
{
public:
	DuplicateEntitiesCommand(std::vector<EntityPtr> handles);
	bool is_valid_flag = true;
	bool is_valid() final {
		return is_valid_flag;
	}

	void execute() final;
	void undo() final {
		for (auto h : handles) {
			h->destroy();
		}

		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() final {
		return "Duplicate Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr> handles;
};

class MovePositionInHierarchy : public Command
{
public:
	enum class Cmd {
		Next,
		Prev,
		First,
		Last
	};

	MovePositionInHierarchy(Entity* e, Cmd cmd);
	bool is_valid() final {
		return entPtr;
	}
	void execute() final {
		auto e = entPtr.get();
		if (!e || !e->get_parent()) return;
		e->get_parent()->move_child_entity_index(e, to_position);

		ed_doc.post_node_changes.invoke();
	}
	void undo() final {
		auto e = entPtr.get();
		if (!e || !e->get_parent()) return;
		e->get_parent()->move_child_entity_index(e, from_position);

		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() final {
		return "MovePositionInHeirarchy";
	}
	int to_position = 0;
	int from_position = 0;
	EntityPtr entPtr{};
};

class CreateComponentCommand : public Command
{
public:
	CreateComponentCommand(Entity* e, const ClassTypeInfo* component_type) {
		ent = e->get_self_ptr();
		ASSERT(component_type->is_a(Component::StaticType));
		info = component_type;
	}
	void execute() final {
		ASSERT(comp_handle == 0);
		auto e = ent.get();
		if (!e) {
			sys_print(Warning, "no entity in createcomponentcommand\n");
			return;
		}
		auto ec = e->create_component_type(info);
		comp_handle = ec->get_instance_id();
		post_create(ec);

		ed_doc.on_component_created.invoke(ec);
	}
	virtual void post_create(Component* ec) {
	}
	void undo() final {
		ASSERT(comp_handle != 0);
		auto obj = eng->get_object(comp_handle);
		ASSERT(obj->is_a<Component>());
		auto ec = (Component*)obj;
		auto id = ec->get_instance_id();
		ec->destroy();
		ed_doc.on_component_deleted.invoke(id);
		comp_handle = 0;
	}
	std::string to_string() override {
		return "Create Component";
	}
	EntityPtr ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};
class CreateScriptComponentCommand : public CreateComponentCommand
{
public:
	CreateScriptComponentCommand(Entity* e, Script* s) 
		: CreateComponentCommand(e,&ScriptComponent::StaticType) {
		this->s = s;
	}
	void post_create(Component* ec)override {
		auto sc = ec->cast_to<ScriptComponent>();
		ASSERT(sc);
		sc->script = s;
	}

	Script* s = nullptr;
};
class CreateMeshComponentCommand : public CreateComponentCommand
{
public:
	CreateMeshComponentCommand(Entity* e, Model* s)
		: CreateComponentCommand(e, &MeshComponent::StaticType) {
		this->s = s;
	}
	void post_create(Component* ec)override {
		auto sc = ec->cast_to<MeshComponent>();
		ASSERT(sc);
		sc->set_model(s);
	}

	Model* s = nullptr;
};

class RemoveComponentCommand  : public Command
{
public:
	RemoveComponentCommand(Entity* e, Component* which) {
		ent = e->get_self_ptr();
		comp_handle = which->get_instance_id();
		info = &which->get_type();
	}
	void execute() final;
	void undo() final;
	std::string to_string() final {
		return "Remove Component";
	}
	EntityPtr ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};

#endif