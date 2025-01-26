#pragma once
#include "EditorDocLocal.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/MeshComponent.h"
#include <memory>

#include "Game/BaseUpdater.h"
#include "LevelSerialization/SerializationAPI.h"

extern EditorDoc ed_doc;

class CommandSerializeUtil
{
public:
	static std::unique_ptr<SerializedSceneFile> serialize_entities_text(std::vector<EntityPtr<Entity>> handles) {
		std::vector<Entity*> ents;
		for (auto h : handles) {
			ents.push_back(h.get());
		}
		ed_doc.validate_fileids_before_serialize();


		return std::make_unique<SerializedSceneFile>(serialize_entities_to_text(ents, ed_doc.get_editing_prefab()));
	}
};

class ParentToCommand : public Command
{
public:
	ParentToCommand(std::vector<Entity*> ents, Entity* parent_to) {
		for (auto e : ents)
			this->entities.push_back(e->get_self_ptr());
		for (auto e : ents)
			this->prev_parents.push_back(e->get_entity_parent() ? e->get_entity_parent()->get_self_ptr() : EntityPtr<Entity>());
		this->parent_to = parent_to ? parent_to->get_self_ptr() : EntityPtr<Entity>();
	}
	void execute() {

		auto parent_to_ent = parent_to.get();
		for (auto ent : entities) {
			Entity* e = ent.get();
			if (!e) {
				sys_print(Warning, "Couldnt find entity for parent to command\n");
				return;
			}
			auto ws_transform = e->get_ws_transform();
			e->parent_to_entity(parent_to_ent);
			e->set_ws_transform(ws_transform);
		}

		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		for (int i = 0; i < entities.size();i++) {
			Entity* e = entities[i].get();
			Entity* prev = prev_parents[i].get();
			if (!e) {
				sys_print(Warning, "Couldnt find entity for parent to command\n");
				return;
			}
			auto ws_transform = e->get_ws_transform();
			e->parent_to_entity(prev);
			e->set_ws_transform(ws_transform);
		}

		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "Parent To";
	}

	std::vector<EntityPtr<Entity>> prev_parents;
	std::vector<EntityPtr<Entity>> entities;
	EntityPtr<Entity> parent_to;
};

class CreatePrefabCommand : public Command
{
public:
	CreatePrefabCommand(const std::string& prefab_name, const glm::mat4& transform, EntityPtr<Entity> parent) {
		this->prefab_name = prefab_name;
		this->transform = transform;
		this->parent_to = parent;
	}
	~CreatePrefabCommand() override {
	}
	void execute() {
		auto l = eng->get_level();
		auto p = GetAssets().find_sync<PrefabAsset>(prefab_name);
		if (p) {
			auto ent = l->spawn_prefab(p.get());
			if (ent) {
				handle = ent->get_self_ptr();
				if (parent_to.get())
					ent->parent_to_entity(parent_to.get());
				else
					ent->set_ws_transform(transform);
				ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
				ed_doc.on_entity_created.invoke(handle);
				ed_doc.post_node_changes.invoke();
			}
		}
	}
	void undo() {
		handle->destroy();
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "Create Prefab";
	}
	EntityPtr<Entity> parent_to;
	EntityPtr<Entity> handle;
	std::string prefab_name;
	glm::mat4 transform;
};

class CreateStaticMeshCommand : public Command
{
public:
	CreateStaticMeshCommand(const std::string& modelname, const glm::mat4& transform, EntityPtr<Entity> parent) {

		this->transform = transform;
		this->modelname = modelname;
		this->parent_to = parent;
	}
	~CreateStaticMeshCommand() override {
	}

	bool is_valid() override { return true; }

	void execute() {
		auto ent = eng->get_level()->spawn_entity_class<Entity>();
		ent->create_and_attach_component_type<MeshComponent>();
		if (parent_to.get())
			ent->parent_to_entity(parent_to.get());
		else
			ent->set_ws_transform(transform);

		handle = ent->get_self_ptr();

		ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());

		ed_doc.on_entity_created.invoke(handle);
		ed_doc.post_node_changes.invoke();


		GetAssets().find_async<Model>(modelname.c_str(), [the_handle = ent->get_instance_id()](GenericAssetPtr p) {
			if (p) {
				auto modelP = p.cast_to<Model>();
				
				if (modelP) {
					auto ent = eng->get_entity(the_handle);
					if (ent) {
						auto mesh_ent = ent->cast_to<Entity>();
						ASSERT(mesh_ent);
						auto firstmesh = mesh_ent->get_first_component<MeshComponent>();
						if (firstmesh)
							firstmesh->set_model(modelP.get());
						else
							sys_print(Warning, "CreateStaticMeshCommand couldnt find mesh component\n");
					}
					else
						sys_print(Warning,"CreateStaticMeshCommand: ent handle invalid in async callback\n");
				}
			}
			});
	}
	void undo() {
		handle->destroy();
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "Create StaticMesh";
	}
	EntityPtr<Entity> parent_to;
	EntityPtr<Entity> handle;
	glm::mat4 transform;
	std::string modelname{};
};
class CreateCppClassCommand : public Command
{
public:
	CreateCppClassCommand(const std::string& cppclassname, const glm::mat4& transform, EntityPtr<Entity> parent) {
		auto find = cppclassname.rfind('/');
		auto types = cppclassname.substr(find==std::string::npos ? 0 : find+1);
		ti = ClassBase::find_class(types.c_str());
		this->transform = transform;
		this->parent_to = parent;
	}
	bool is_valid() override { return ti != nullptr; }

	void execute() {
		assert(ti);
		auto ent = eng->get_level()->spawn_entity_from_classtype(*ti);
		if (parent_to.get())
			ent->parent_to_entity(parent_to.get());
		else
			ent->set_ws_transform(transform);
		ent->editor_name = ent->get_type().classname;
		handle = ent->get_self_ptr();
		ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
		ed_doc.on_entity_created.invoke(handle);
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		eng->get_level()->destroy_entity(eng->get_entity(handle));
		ed_doc.post_node_changes.invoke();
		handle = {};
	}
	std::string to_string() override {
		return "Create Class";
	}
	const ClassTypeInfo* ti = nullptr;
	glm::mat4 transform;
	EntityPtr<Entity> handle;
	EntityPtr<Entity> parent_to;
};

class InstantiatePrefabCommand : public Command
{
public:
	InstantiatePrefabCommand(Entity* e) {
		me = e->get_self_ptr();
		asset = me->what_prefab;
		if (!asset || !me->is_root_of_prefab) {
			asset = nullptr;
			sys_print(Error, "Cant instantiate non-prefab, non-root object\n");
			return;
		}
		if (me->creator_source)
			creator_source = me->creator_source->get_self_ptr();
	}

	bool is_valid() override {
		return asset != nullptr;
	}

	void execute_R(Entity* e) {
		for (auto c : e->get_all_components()) {
			if (this_is_newly_created(c, asset)) {
				created_obj created;
				created.eng_handle = c->get_instance_id();
				created.unique_file_id = c->unique_file_id;
				created_objs.push_back(created);

				c->creator_source = nullptr;
				if (c->what_prefab == asset)
					c->what_prefab = nullptr;
				c->unique_file_id = ed_doc.get_next_file_id();
			}
		}
		for (auto c : e->get_all_children()) {
			if (this_is_newly_created(c, asset)) {
				created_obj created;
				created.eng_handle = c->get_instance_id();
				created.unique_file_id = c->unique_file_id;
				created_objs.push_back(created);
				c->creator_source = nullptr;
				if (c->what_prefab == asset)
					c->what_prefab = nullptr;
				c->unique_file_id = ed_doc.get_next_file_id();
			}
			execute_R(c);
		}
	}

	void execute() override {
		ASSERT(created_objs.size() == 0);
		me->what_prefab = nullptr;
		me->is_root_of_prefab = false;
		me->creator_source = nullptr;
		execute_R(me.get());
	}
	void undo() override {
		if (!me) {
			sys_print(Warning, "couldnt undo instantiate prefab command\n");
			return;
		}

		me->what_prefab = asset;
		me->is_root_of_prefab = false;
		me->creator_source = creator_source.get();
		for (auto c : created_objs) {
			auto obj = eng->get_level()->get_entity(c.eng_handle);
			obj->creator_source = me.get();
			if (!obj->what_prefab)
				obj->what_prefab = asset;
			obj->unique_file_id = c.unique_file_id;
		}
		created_objs.clear();

	}
	std::string to_string() override {
		return "Instantiate Prefab";
	}
	struct created_obj {
		uint64_t eng_handle = 0;
		uint64_t unique_file_id = 0;
	};
	std::vector<created_obj> created_objs;
	EntityPtr<Entity> me;
	PrefabAsset* asset = nullptr;
	EntityPtr<Entity> creator_source;
};

class DuplicateEntitiesCommand : public Command
{
public:
	DuplicateEntitiesCommand(std::vector<EntityPtr<Entity>> handles) {

		scene = CommandSerializeUtil::serialize_entities_text(handles);
	}

	void execute() {
		auto duplicated = unserialize_entities_from_text(scene->text);

		auto& extern_parents = scene->extern_parents;
		for (auto ep : extern_parents) {
			auto e = duplicated.get_objects().find(ep.child_path);
			ASSERT(e->second->is_a<Entity>());
			if (e != duplicated.get_objects().end()) {
				EntityPtr<Entity> parent = { ep.external_parent_handle };
				if (parent.get()) {
					auto ent = (Entity*)e->second;
					ent->parent_to_entity(parent.get());
				}
				else
					sys_print(Warning, "duplicated parent doesnt exist\n");
			}
			else
				sys_print(Warning, "duplicated obj doesnt exist\n");
		}

		// zero out file ids so new ones are set
		for (auto o : duplicated.get_objects())
			if(o.second->creator_source == nullptr) // ==nullptr meaning that its created by level
				o.second->unique_file_id = 0;

		eng->get_level()->insert_unserialized_entities_into_level(duplicated);


		handles.clear();
		auto& objs = duplicated.get_objects();
		for (auto& o : objs) {
			if (auto e = o.second->cast_to<Entity>())
			{
				ed_doc.on_entity_created.invoke(e->get_self_ptr());
				handles.push_back(e->get_self_ptr());
			}
		}
		ed_doc.selection_state->clear_all_selected();
		for (auto e : handles) {
			ed_doc.selection_state->add_to_entity_selection(e);
		}

		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		for (auto h : handles) {
			h->destroy();
		}

		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "Duplicate Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr<Entity>> handles;
};


void validate_remove_entities(std::vector<EntityPtr<Entity>>& input)
{
	bool had_errors = false;
	for (int i = 0; i < input.size(); i++)
	{
		auto e = input[i];
		Entity* ent = e.get();
		if (!ent) continue;	// whatever, doesnt matter

		if (!ed_doc.can_delete_or_move_this(ent)) {
			had_errors = true;
			input.erase(input.begin() + i);
			i--;
		}
	}
	if (had_errors)
		eng->log_to_fullscreen_gui(Error, "Cant remove inherited entities");
	had_errors = false;
	if (ed_doc.is_editing_prefab()) {
		auto root = ed_doc.get_prefab_root_entity();
		if (root) {
			for (int i = 0; i < input.size();i++) {
				auto e = input[i];
				if (e.get() == root) {
					had_errors = true;
					input.erase(input.begin() + i);
					i--;
				}
			}
		}

		if (had_errors)
			eng->log_to_fullscreen_gui(Error, "Cant remove root prefab entity");
	}
}

class RemoveEntitiesCommand : public Command
{
public:
	RemoveEntitiesCommand(std::vector<EntityPtr<Entity>> handles) {

		scene = CommandSerializeUtil::serialize_entities_text(handles);

		this->handles = handles;
	}

	void execute() {

		for (auto h : handles) {
			h->destroy();
		}
		ed_doc.post_node_changes.invoke();
	}
	void undo() {
		auto restored = unserialize_entities_from_text(scene->text);
		auto& extern_parents = scene->extern_parents;
		for (auto ep : extern_parents) {
			auto e = restored.get_objects().find(ep.child_path);
			ASSERT(e->second->is_a<Entity>());
			if (e != restored.get_objects().end()) {
				EntityPtr<Entity> parent = { ep.external_parent_handle };
				if (parent.get()) {
					auto ent = (Entity*)e->second;
					ent->parent_to_entity(parent.get());
				}
				else
					sys_print(Warning, "restored parent doesnt exist\n");
			}
			else
				sys_print(Warning, "restored obj doesnt exist\n");
		}

		eng->get_level()->insert_unserialized_entities_into_level(restored);



		auto& objs = restored.get_objects();
		for (auto& o : objs) {
			if (auto e = o.second->cast_to<Entity>())
				ed_doc.on_entity_created.invoke(e->get_self_ptr());
		}
		ed_doc.post_node_changes.invoke();
	}
	std::string to_string() override {
		return "Remove Entity";
	}

	std::unique_ptr<SerializedSceneFile> scene;
	std::vector<EntityPtr<Entity>> handles;
};

class CreateComponentCommand : public Command
{
public:
	CreateComponentCommand(Entity* e, const ClassTypeInfo* component_type) {
		ent = e->get_self_ptr();
		ASSERT(component_type->is_a(EntityComponent::StaticType));
		info = component_type;
	}
	void execute() {
		ASSERT(comp_handle == 0);
		auto e = ent.get();
		if (!e) {
			sys_print(Warning, "no entity in createcomponentcommand\n");
			return;
		}
		auto ec = e->create_and_attach_component_type(info);
		comp_handle = ec->get_instance_id();

		ed_doc.on_component_created.invoke(ec);
	}
	void undo() {
		ASSERT(comp_handle != 0);
		auto obj = eng->get_object(comp_handle);
		ASSERT(obj->is_a<EntityComponent>());
		auto ec = (EntityComponent*)obj;
		auto id = ec->get_instance_id();
		ec->destroy();
		ed_doc.on_component_deleted.invoke(id);
		comp_handle = 0;
	}
	std::string to_string() override {
		return "Create Component";
	}
	EntityPtr<Entity> ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};
class RemoveComponentCommand  : public Command
{
public:
	RemoveComponentCommand(Entity* e, EntityComponent* which) {
		ent = e->get_self_ptr();
		comp_handle = which->get_instance_id();
		info = &which->get_type();
	}
	void execute() {
		ASSERT(comp_handle != 0);
		auto obj = eng->get_object(comp_handle);
		ASSERT(obj->is_a<EntityComponent>());
		auto ec = (EntityComponent*)obj;
		auto id = ec->get_instance_id();
		ec->destroy();
		ed_doc.on_component_deleted.invoke(id);
		comp_handle = 0;
	}
	void undo() {
		ASSERT(comp_handle == 0);
		auto e = ent.get();
		if (!e) {
			sys_print(Warning, "no entity in RemoveComponentCommand\n");
			return;
		}
		auto ec = e->create_and_attach_component_type(info);
		comp_handle = ec->get_instance_id();
	}
	std::string to_string() override {
		return "Remove Component";
	}
	EntityPtr<Entity> ent;
	uint64_t comp_handle = 0;
	const ClassTypeInfo* info = nullptr;
};

