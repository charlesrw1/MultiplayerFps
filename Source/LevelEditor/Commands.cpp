#include "Commands.h"

void validate_remove_entities(std::vector<EntityPtr>& input)
{
	bool had_errors = false;
	for (int i = 0; i < input.size(); i++)
	{
		auto e = input[i];
		Entity* ent = e.get();
		if (!ent) continue;	// whatever, doesnt matter

		if (!ed_doc.can_delete_this_object(ent)) {
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
			for (int i = 0; i < input.size(); i++) {
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

RemoveEntitiesCommand::RemoveEntitiesCommand(std::vector<EntityPtr> handles) {
	validate_remove_entities(handles);
	for (auto e : handles) {
		is_valid_flag &= ed_doc.can_delete_this_object(e.get());
	}
	if (handles.empty())
		is_valid_flag = false;
	if (!is_valid_flag)
		return;


	scene = CommandSerializeUtil::serialize_entities_text(handles);

	this->handles = handles;
}

void RemoveEntitiesCommand::undo() {
	ASSERT(is_valid());
	auto restored = unserialize_entities_from_text(scene->text);
	auto& extern_parents = scene->extern_parents;
	for (auto& ep : extern_parents) {
		auto e = restored.get_objects().find(ep.child_path);
		ASSERT(e->second->is_a<Entity>());
		if (e != restored.get_objects().end()) {
			EntityPtr parent(ep.external_parent_handle);
			if (parent.get()) {
				auto ent = (Entity*)e->second;
				ent->parent_to(parent.get());
			}
			else
				sys_print(Warning, "restored parent doesnt exist\n");
		}
		else
			sys_print(Warning, "restored obj doesnt exist\n");
	}

	eng->get_level()->insert_unserialized_entities_into_level(restored, scene.get());	// pass in scene so handles get set to what they were
	auto& objs = restored.get_objects();

	// refresh handles i guess ? fixme
	handles.clear();
	for (auto& o : objs) {
		if (o.second->is_a<Entity>()) {
			auto ent = (Entity*)o.second;
			handles.push_back(ent->get_self_ptr());
		}
	}

	for (auto& o : objs) {
		if (auto e = o.second->cast_to<Entity>())
			ed_doc.on_entity_created.invoke(e->get_self_ptr());
	}
	ed_doc.post_node_changes.invoke();
}

ParentToCommand::ParentToCommand(std::vector<Entity*> ents, Entity* parent_to, bool create_new_parent, bool delete_parent) {

	if (delete_parent && ents.size() != 1) {
		is_valid_flag = false;
		return;
	}
	else if (delete_parent) {
		auto p = ents[0];
		if (p) {
			ents.clear();
			for (auto e : p->get_children())
				ents.push_back(e);

			parent_to = p->get_parent();
		}
		else
			is_valid_flag = false;
	}


	if (!parent_to) {
		if (ed_doc.is_editing_prefab()) {
			if (create_new_parent) {
				is_valid_flag = false;
				return;
			}

			parent_to = ed_doc.get_prefab_root_entity();
			assert(parent_to);
		}
	}

	auto validate = [&]() -> bool {
		if (ents.empty())
			return false;
		for (auto e : ents) {
			if (!e) return false;
			if (!ed_doc.is_this_object_not_inherited(e)) return false;
			if (e == parent_to) return false;
		}
		return true;
	};

	is_valid_flag = validate();
	if (!is_valid_flag)
		return;

	for (auto e : ents)
		this->entities.push_back(e->get_self_ptr());
	for (auto e : ents)
		this->prev_parents.push_back(EntityPtr(e->get_parent()));
	this->parent_to = EntityPtr(parent_to);
	parent_to_prev_parent = (parent_to) ? EntityPtr(parent_to->get_parent()) : EntityPtr();

	ASSERT(!create_new_parent || !parent_to);	// if create_new_parent, parent_to is false
	this->create_new_parent = create_new_parent;

	auto all_parents_equal = [&]() -> bool {
		auto p = this->prev_parents[0];
		for (int i = 1; i < this->prev_parents.size(); i++) {
			if (this->prev_parents[i] != p) return false;
		}
		return true;
	};

	if (!delete_parent || (all_parents_equal()))	// if delete_parent, all_parents_equal==true
		this->delete_the_parent = delete_parent;
	else
		is_valid_flag = false;
}

void ParentToCommand::execute() {

	if (create_new_parent) {
		ASSERT(!parent_to);
		auto newparent = eng->get_level()->spawn_entity();
		glm::vec3 pos(0.f);
		int count = 0;
		for (auto e : entities) {
			auto ent = e.get();
			if (ent) {
				pos += ent->get_ws_position();
				count++;
			}
		}
		pos /= (float)count;
		newparent->set_ws_position(pos);
		parent_to = newparent->get_self_ptr();

		ed_doc.selection_state->set_select_only_this(newparent);
	}

	auto parent_to_ent = parent_to.get();
	for (auto ent : entities) {
		Entity* e = ent.get();
		if (!e) {
			sys_print(Warning, "Couldnt find entity for parent to command\n");
			return;
		}
		auto ws_transform = e->get_ws_transform();
		e->parent_to(parent_to_ent);
		e->set_ws_transform(ws_transform);
	}


	if (delete_the_parent) {
		ASSERT(this->prev_parents[0].get());
		if (!remove_the_parent_cmd)	// do this here because we want to serialize the parent when the child entities are removed
			remove_the_parent_cmd = std::make_unique<RemoveEntitiesCommand>(std::vector<EntityPtr>{ this->prev_parents[0]->get_self_ptr() });
		//ASSERT(remove_the_parent_cmd->handles.size() == 1);
		if (!remove_the_parent_cmd->is_valid())
			throw std::runtime_error("RemoveEntitiesCommand not valid in ParentToCommand");
		remove_the_parent_cmd->execute();
		ASSERT(remove_the_parent_cmd->handles.size() == 1);
	}

	ed_doc.post_node_changes.invoke();
}

void ParentToCommand::undo() {
	if (delete_the_parent) {
		ASSERT(remove_the_parent_cmd->handles.size() == 1);
		remove_the_parent_cmd->undo();
		ASSERT(remove_the_parent_cmd->handles.size() == 1);
		ASSERT(remove_the_parent_cmd->handles[0].get());
		// set prev parent...
		for (int i = 0; i < prev_parents.size(); i++) {
			prev_parents[i] = remove_the_parent_cmd->handles[0]->get_self_ptr();
		}
	}

	ed_doc.selection_state->clear_all_selected();

	for (int i = 0; i < entities.size(); i++) {
		Entity* e = entities[i].get();
		Entity* prev = prev_parents[i].get();
		if (!e) {
			sys_print(Warning, "Couldnt find entity for parent to command\n");
			return;
		}
		auto ws_transform = e->get_ws_transform();
		e->parent_to(prev);
		e->set_ws_transform(ws_transform);

		ed_doc.selection_state->add_to_entity_selection(e);
	}

	if (parent_to_prev_parent) {
		if (parent_to) {
			parent_to->parent_to(parent_to_prev_parent.get());
		}
	}

	if (create_new_parent) {
		auto p = parent_to.get();
		if (!p) {
			sys_print(Warning, "create_new_parent ptr was null??\n");
		}
		eng->get_level()->destroy_entity(p);
		parent_to = EntityPtr();
	}

	ed_doc.post_node_changes.invoke();
}

CreatePrefabCommand::CreatePrefabCommand(const std::string& prefab_name, const glm::mat4& transform, EntityPtr parent) {
	this->prefab_name = prefab_name;
	this->transform = transform;
	this->parent_to = parent;

	if (ed_doc.is_editing_prefab()) {
		auto root = ed_doc.get_prefab_root_entity();
		if (root)
			this->parent_to = root->get_self_ptr();
	}

}

void CreatePrefabCommand::execute() {
	auto l = eng->get_level();
	auto p = g_assets.find_sync<PrefabAsset>(prefab_name);
	if (p) {
		auto ent = l->spawn_prefab(p.get());
		if (ent) {
			handle = ent->get_self_ptr();
			if (parent_to.get())
				ent->parent_to(parent_to.get());
			else
				ent->set_ws_transform(transform);
			ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
			ed_doc.on_entity_created.invoke(handle);
			ed_doc.post_node_changes.invoke();
		}
	}
}

CreateStaticMeshCommand::CreateStaticMeshCommand(const std::string& modelname, const glm::mat4& transform, EntityPtr parent) {

	this->transform = transform;
	this->modelname = modelname;
	this->parent_to = parent;

	if (ed_doc.is_editing_prefab()) {
		auto root = ed_doc.get_prefab_root_entity();
		if (root)
			this->parent_to = root->get_self_ptr();
	}
}

void CreateStaticMeshCommand::execute() {
	auto ent = eng->get_level()->spawn_entity();
	ent->create_component<MeshComponent>();
	if (parent_to.get())
		ent->parent_to(parent_to.get());
	else
		ent->set_ws_transform(transform);

	handle = ent->get_self_ptr();

	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());

	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();


	g_assets.find_async<Model>(modelname.c_str(), [the_handle = ent->get_instance_id()](GenericAssetPtr p) {
		if (p) {
			auto modelP = p.cast_to<Model>();

			if (modelP) {
				auto ent = eng->get_entity(the_handle);
				if (ent) {
					auto mesh_ent = ent->cast_to<Entity>();
					ASSERT(mesh_ent);
					auto firstmesh = mesh_ent->get_component<MeshComponent>();
					if (firstmesh)
						firstmesh->set_model(modelP.get());
					else
						sys_print(Warning, "CreateStaticMeshCommand couldnt find mesh component\n");
				}
				else
					sys_print(Warning, "CreateStaticMeshCommand: ent handle invalid in async callback\n");
			}
		}
	});
}

CreateCppClassCommand::CreateCppClassCommand(const std::string& cppclassname, const glm::mat4& transform, EntityPtr parent, bool is_component) {
	auto find = cppclassname.rfind('/');
	auto types = cppclassname.substr(find == std::string::npos ? 0 : find + 1);
	ti = ClassBase::find_class(types.c_str());
	this->transform = transform;
	this->parent_to = parent;
	is_component_type = is_component;

	if (ed_doc.is_editing_prefab()) {
		auto root = ed_doc.get_prefab_root_entity();
		if (root && !this->parent_to)
			this->parent_to = root->get_self_ptr();
	}
}

void CreateCppClassCommand::execute() {
	assert(ti);
	Entity* ent{};
	if (is_component_type) {
		ent = eng->get_level()->spawn_entity();
		ent->create_component_type(ti);
	}
	else
		ent = eng->get_level()->spawn_entity();
	if (parent_to.get())
		ent->parent_to(parent_to.get());
	else
		ent->set_ws_transform(transform);
	handle = ent->get_self_ptr();
	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();
}

void CreateCppClassCommand::undo() {

	auto ent = handle.get();
	auto level = eng->get_level();
	level->destroy_entity(ent);
	ed_doc.post_node_changes.invoke();
	handle = {};
}

TransformCommand::TransformCommand(const std::unordered_set<uint64_t>& selection, const std::unordered_map<uint64_t, glm::mat4>& pre_transforms) {
	for (auto& pair : selection) {
		auto find = pre_transforms.find(pair);
		if (find != pre_transforms.end()) {

			EntityPtr e(find->first);
			if (e.get()) {
				pre_and_post pp;
				pp.ptr = e;
				pp.pre_transform = find->second;
				pp.post_transform = e->get_ws_transform();
				transforms.push_back(pp);
			}
		}
	}
	skip_this_time = true;
}

void TransformCommand::execute() {
	// already have ws transforms
	if (skip_this_time) {
		skip_this_time = false;
		return;
	}
	for (auto& t : transforms) {
		if (t.ptr.get()) {
			t.ptr->set_ws_transform(t.post_transform);
		}
	}
	ed_doc.selection_state->on_selection_changed.invoke();//hack
}

InstantiatePrefabCommand::InstantiatePrefabCommand(Entity* e) {
	if (ed_doc.is_editing_prefab() && ed_doc.get_prefab_root_entity() == e)
		return;	// is_valid == false


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

void InstantiatePrefabCommand::execute_R(Entity* e) {
	for (auto c : e->get_components()) {
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
	for (auto c : e->get_children()) {
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

void InstantiatePrefabCommand::execute() {
	ASSERT(created_objs.size() == 0);
	ASSERT(me->what_prefab);
	me->what_prefab = nullptr;
	ASSERT(me->is_root_of_prefab);
	me->is_root_of_prefab = false;
	me->creator_source = nullptr;
	execute_R(me.get());

	ed_doc.post_node_changes.invoke();
}

void InstantiatePrefabCommand::undo() {
	if (!me) {
		sys_print(Warning, "couldnt undo instantiate prefab command\n");
		return;
	}

	me->what_prefab = asset;
	me->is_root_of_prefab = true;
	me->creator_source = creator_source.get();
	for (auto c : created_objs) {
		auto obj = eng->get_level()->get_entity(c.eng_handle);
		obj->creator_source = me.get();
		if (!obj->what_prefab)
			obj->what_prefab = asset;
		obj->unique_file_id = c.unique_file_id;
	}
	created_objs.clear();

	ed_doc.post_node_changes.invoke();
}

DuplicateEntitiesCommand::DuplicateEntitiesCommand(std::vector<EntityPtr> handles) {

	if (handles.empty())
		is_valid_flag = false;
	if (ed_doc.is_editing_prefab()) {
		auto root = ed_doc.get_prefab_root_entity();
		for (auto h : handles) {
			if (h.get() == root) {
				sys_print(Warning, "cant duplicate root in prefab mode\n");
				is_valid_flag = false;
				return;
			}
		}
	}
	if (!is_valid_flag)
		return;

	// todo: validation

	scene = CommandSerializeUtil::serialize_entities_text(handles);
}

void DuplicateEntitiesCommand::execute() {
	auto duplicated = unserialize_entities_from_text(scene->text);

	auto& extern_parents = scene->extern_parents;
	for (auto ep : extern_parents) {
		auto e = duplicated.get_objects().find(ep.child_path);
		if (e != duplicated.get_objects().end()) {
			ASSERT(e->second->is_a<Entity>());
			EntityPtr parent(ep.external_parent_handle);
			if (parent.get()) {
				auto ent = (Entity*)e->second;
				ent->parent_to(parent.get());
			}
			else
				sys_print(Warning, "duplicated parent doesnt exist\n");
		}
		else
			sys_print(Warning, "duplicated obj doesnt exist\n");
	}

	// zero out file ids so new ones are set
	for (auto o : duplicated.get_objects())
		if (o.second->creator_source == nullptr) // ==nullptr meaning that its created by level
			o.second->unique_file_id = 0;

	eng->get_level()->insert_unserialized_entities_into_level(duplicated);	// since duplicating, DONT pass in scene


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

MovePositionInHierarchy::MovePositionInHierarchy(Entity* e, Cmd cmd) {
	if (!e) return;
	const auto parent = e->get_parent();
	if (!parent) return;
	from_position = parent->get_child_entity_index(e);
	if (from_position == -1)
		return;
	auto& children = parent->get_children();
	const int last_idx = children.size() - 1;
	switch (cmd) {
	case Cmd::Next:
		to_position = std::min((from_position + 1), last_idx);
		break;
	case Cmd::Prev:
		to_position = std::max((from_position - 1), 0);
		break;
	case Cmd::Last:
		to_position = last_idx;
		break;
	case Cmd::First:
		to_position = 0;
		break;
	}


	entPtr = e->get_self_ptr();
}

std::unique_ptr<SerializedSceneFile> CommandSerializeUtil::serialize_entities_text(std::vector<EntityPtr> handles) {
	std::vector<Entity*> ents;
	for (auto h : handles) {
		ents.push_back(h.get());
	}
	ed_doc.validate_fileids_before_serialize();


	return std::make_unique<SerializedSceneFile>(serialize_entities_to_text(ents, ed_doc.get_editing_prefab()));
}

void RemoveComponentCommand::execute() {
	ASSERT(comp_handle != 0);
	auto obj = eng->get_object(comp_handle);
	ASSERT(obj->is_a<Component>());
	auto ec = (Component*)obj;
	auto id = ec->get_instance_id();
	ec->destroy();
	// dont move this!
	ed_doc.on_component_deleted.invoke(id);
	comp_handle = 0;
}

void RemoveComponentCommand::undo() {
	ASSERT(comp_handle == 0);
	auto e = ent.get();
	if (!e) {
		sys_print(Warning, "no entity in RemoveComponentCommand\n");
		return;
	}
	auto ec = e->create_component_type(info);
	comp_handle = ec->get_instance_id();
}
