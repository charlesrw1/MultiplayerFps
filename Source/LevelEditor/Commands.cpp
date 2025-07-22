#include "Commands.h"
#include <unordered_set>
#include "Framework/MapUtil.h"

void validate_remove_entities(EditorDoc& ed_doc, std::vector<EntityPtr>& input)
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
}
#include "LevelSerialization/SerializationAPI.h"
static void add_to_remove_list_R(vector<SavedCreateObj>& objs, Entity* e, std::unordered_set<BaseUpdater*>& seen)
{
	if (!this_is_a_serializeable_object(e))
		return;
	if (SetUtil::contains(seen, (BaseUpdater*)e))
		return;
	SetUtil::insert_test_exists(seen, (BaseUpdater*)e);

	for (auto c : e->get_components()) {
		if (!this_is_a_serializeable_object(c))
			continue;
		if (SetUtil::contains(seen, (BaseUpdater*)c))
			continue;
		SetUtil::insert_test_exists(seen, (BaseUpdater*)c);

		SavedCreateObj created;
		created.eng_handle = c->get_instance_id();
		created.unique_file_id = c->unique_file_id;
		objs.push_back(created);
	}
	SavedCreateObj created;
	created.eng_handle = e->get_instance_id();
	created.unique_file_id = e->unique_file_id;
	created.spawn_type = e->get_object_prefab_spawn_type();

	objs.push_back(created);
	for (auto c : e->get_children()) {
		add_to_remove_list_R(objs,c,seen);
	}
}

RemoveEntitiesCommand::RemoveEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles):ed_doc(ed_doc) {
	validate_remove_entities(ed_doc, handles);
	for (EntityPtr e : handles) {
		is_valid_flag &= ed_doc.can_delete_this_object(e.get());
	}
	if (handles.empty())
		is_valid_flag = false;
	if (!is_valid_flag)
		return;

	ed_doc.validate_fileids_before_serialize();
	std::unordered_set<BaseUpdater*> seen;
	for (EntityPtr e : handles) {
		Entity* ent = e.get();
		if (ent)
			add_to_remove_list_R(removed_objs, ent,seen);
		else {
			sys_print(Warning, "RemoveEntitiesCommand(): handle invalid: %lld\n", e.handle);
		}
	}

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
	assert(seen.size() == removed_objs.size());
	assert(removed_objs.size() == scene->path_to_instance_handle.size());

	this->handles = handles;
}
#include "Framework/Log.h"

void RemoveEntitiesCommand::undo() {
	ASSERT(is_valid());

	auto restored = unserialize_entities_from_text("remove_entities_undo",scene->text, AssetDatabase::loader);
	auto& extern_parents = scene->extern_parents;
	for (auto& ep : extern_parents) {
		auto e = restored.file_id_to_obj.find(ep.child_id);
		ASSERT(e->second->is_a<Entity>());
		if (e != restored.file_id_to_obj.end()) {
			EntityPtr parent(ep.external_parent_handle);
			if (parent.get()) {
				auto ent = (Entity*)e->second;
				ent->parent_to(parent.get());
			}
			else
				sys_print(Warning, "RemoveEntitiesCommand::undo: restored parent doesnt exist\n");
		}
		else
			sys_print(Warning, "RemoveEntitiesCommand::undo: restored obj doesnt exist\n");
	}

	ed_doc.insert_unserialized_into_scene(restored, scene.get());
	//eng->get_level()->insert_unserialized_entities_into_level(restored, scene.get());	// pass in scene so handles get set to what they were
	auto& objs = restored.file_id_to_obj;

	for (SavedCreateObj c : removed_objs) {
		BaseUpdater* obj = eng->get_level()->get_entity(c.eng_handle);
		if (!obj) {
			sys_print(Warning, "RemoveEntitiesCommand::undo: object cant be found to put back %lld\n", c.eng_handle);
			continue;
		}
		if (auto as_ent = obj->cast_to<Entity>()) {
			assert(as_ent->get_object_prefab_spawn_type() == c.spawn_type);
		}
		obj->unique_file_id = c.unique_file_id;
		assert(obj->get_instance_id() == c.eng_handle);
	}

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

ParentToCommand::ParentToCommand(EditorDoc& ed_doc, std::vector<Entity*> ents, Entity* parent_to, bool create_new_parent, bool delete_parent) 
	: ed_doc(ed_doc)
{

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
		Entity* newparent = ed_doc.spawn_entity();
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
			remove_the_parent_cmd = std::make_unique<RemoveEntitiesCommand>(ed_doc, std::vector<EntityPtr>{ this->prev_parents[0]->get_self_ptr() });
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
		ed_doc.remove_scene_object(p);
		//eng->get_level()->destroy_entity(p);
		parent_to = EntityPtr();
	}

	ed_doc.post_node_changes.invoke();
}

CreatePrefabCommand::CreatePrefabCommand(EditorDoc& ed_doc, const std::string& prefab_name, const glm::mat4& transform, EntityPtr parent)
	: ed_doc(ed_doc)
{
	this->prefab_name = prefab_name;
	this->transform = transform;
	this->parent_to = parent;

}

void CreatePrefabCommand::execute() {
	auto p = g_assets.find_sync<PrefabAsset>(prefab_name);
	if (p) {
		Entity* ent = ed_doc.spawn_prefab(p.get());
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

CreateStaticMeshCommand::CreateStaticMeshCommand(EditorDoc& ed_doc, const std::string& modelname, const glm::mat4& transform, EntityPtr parent) 
	: ed_doc(ed_doc)
{

	this->transform = transform;
	this->modelname = modelname;
	this->parent_to = parent;

}

void CreateStaticMeshCommand::execute() {
	auto ent = ed_doc.spawn_entity();
	ed_doc.attach_component(&MeshComponent::StaticType, ent);
	if (parent_to.get())
		ent->parent_to(parent_to.get());
	else
		ent->set_ws_transform(transform);

	handle = ent->get_self_ptr();

	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());

	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();


	Model* modelP = g_assets.find_sync<Model>(modelname).get();
	if (modelP) {
		if (ent) {
			auto mesh_ent = ent->cast_to<Entity>();
			ASSERT(mesh_ent);
			auto firstmesh = mesh_ent->get_component<MeshComponent>();
			if (firstmesh)
				firstmesh->set_model(modelP);
			else
				sys_print(Warning, "CreateStaticMeshCommand couldnt find mesh component\n");
		}
		else
			sys_print(Warning, "CreateStaticMeshCommand: ent handle invalid in async callback\n");
	}
}

CreateCppClassCommand::CreateCppClassCommand(EditorDoc& ed_doc, const std::string& cppclassname, const glm::mat4& transform, EntityPtr parent, bool is_component) 
	: ed_doc(ed_doc)
{
	auto find = cppclassname.rfind('/');
	auto types = cppclassname.substr(find == std::string::npos ? 0 : find + 1);
	ti = ClassBase::find_class(types.c_str());
	this->transform = transform;
	this->parent_to = parent;
	is_component_type = is_component;
}

void CreateCppClassCommand::execute() {
	assert(ti);
	Entity* ent{};
	if (is_component_type) {
		ent = ed_doc.spawn_entity();// eng->get_level()->spawn_entity();
		ed_doc.attach_component(ti, ent);// ent->create_component_type(ti);
	}
	else
		ent = ed_doc.spawn_entity();

	if (parent_to.get())
		ent->parent_to(parent_to.get());

	ent->set_ws_transform(transform);

	handle = ent->get_self_ptr();
	ed_doc.selection_state->set_select_only_this(ent->get_self_ptr());
	ed_doc.on_entity_created.invoke(handle);
	ed_doc.post_node_changes.invoke();
}

void CreateCppClassCommand::undo() {

	auto ent = handle.get();
	ed_doc.remove_scene_object(ent);
	if (ed_doc.selection_state->is_entity_selected(ent)) {
		ed_doc.selection_state->clear_all_selected();
	}
	//auto level = eng->get_level();
	//level->destroy_entity(ent);
	ed_doc.post_node_changes.invoke();
	handle = {};
}

TransformCommand::TransformCommand(EditorDoc& ed_doc, const std::unordered_set<uint64_t>& selection, const std::unordered_map<uint64_t, glm::mat4>& pre_transforms) 
	:ed_doc(ed_doc)
{
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

DuplicateEntitiesCommand::DuplicateEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles) 
	:ed_doc(ed_doc)
{

	if (handles.empty())
		is_valid_flag = false;
	
	if (!is_valid_flag)
		return;

	// todo: validation

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
}

void DuplicateEntitiesCommand::execute() {
	UnserializedSceneFile duplicated = unserialize_entities_from_text("duplicate_entities",scene->text, AssetDatabase::loader);

	auto& extern_parents = scene->extern_parents;
	for (auto ep : extern_parents) {
		auto e = duplicated.file_id_to_obj.find(ep.child_id);
		if (e != duplicated.file_id_to_obj.end()) {
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
	//for (auto o : duplicated.get_objects())
	//	if (o.second->creator_source == nullptr) // ==nullptr meaning that its created by level
	//		o.second->unique_file_id = 0;

	ed_doc.insert_unserialized_into_scene(duplicated, nullptr);

	//eng->get_level()->insert_unserialized_entities_into_level(duplicated);	// since duplicating, DONT pass in scene


	handles.clear();
	auto& objs = duplicated.file_id_to_obj;
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

MovePositionInHierarchy::MovePositionInHierarchy(EditorDoc& ed_doc, Entity* e, Cmd cmd) 
	:ed_doc(ed_doc)
{
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

std::unique_ptr<SerializedSceneFile> CommandSerializeUtil::serialize_entities_text(EditorDoc& ed_doc, std::vector<EntityPtr> handles) {
	std::vector<Entity*> ents;
	for (auto h : handles) {
		ents.push_back(h.get());
	}
	ed_doc.validate_fileids_before_serialize();


	return std::make_unique<SerializedSceneFile>(serialize_entities_to_text("Command::serialize_entities_text", ents));
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
	auto ec = e->create_component(info);
	comp_handle = ec->get_instance_id();
}
