#ifdef EDITOR_BUILD
#include "Commands.h"
#include <unordered_set>
#include <limits>
#include "Framework/MapUtil.h"
#include "EditorDocLocal.h"
#include "Game/Prefab.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
void validate_remove_entities(EditorDoc& ed_doc, std::vector<EntityPtr>& input) {
	bool had_errors = false;
	for (int i = 0; i < input.size(); i++) {
		auto e = input[i];
		Entity* ent = e.get();
		if (!ent)
			continue; // whatever, doesnt matter

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
static void add_to_remove_list_R(vector<SavedCreateObj>& objs, Entity* e, std::unordered_set<BaseUpdater*>& seen) {
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
		objs.push_back(created);
	}
	SavedCreateObj created;
	created.eng_handle = e->get_instance_id();
	// created.spawn_type = e->get_object_prefab_spawn_type();

	objs.push_back(created);
	for (auto c : e->get_children()) {
		add_to_remove_list_R(objs, c, seen);
	}
}

RemoveEntitiesCommand::RemoveEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles) : ed_doc(ed_doc) {
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
			add_to_remove_list_R(removed_objs, ent, seen);
		else {
			sys_print(Warning, "RemoveEntitiesCommand(): handle invalid: %lld\n", e.handle);
		}
	}

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
	assert(seen.size() == removed_objs.size());
	// assert(removed_objs.size() == scene->path_to_instance_handle.size());

	this->handles = handles;
}
#include "Framework/Log.h"

void RemoveEntitiesCommand::execute() {
	ASSERT(is_valid());
	for (auto h : handles) {
		h->destroy();
	}
	ed_doc.post_node_changes.invoke();
}

void RemoveEntitiesCommand::undo() {
	ASSERT(is_valid());

	auto restored = unserialize_entities_from_text("remove_entities_undo", scene->text, AssetDatabase::loader,
												   true /* restore id*/);
	// auto& extern_parents = scene->extern_parents;

	ed_doc.insert_unserialized_into_scene(restored);
	// eng->get_level()->insert_unserialized_entities_into_level(restored, scene.get());	// pass in scene so handles
	// get set to what they were

	for (SavedCreateObj c : removed_objs) {
		BaseUpdater* obj = eng->get_level()->get_entity(c.eng_handle);
		if (!obj) {
			sys_print(Warning, "RemoveEntitiesCommand::undo: object cant be found to put back %lld\n", c.eng_handle);
			continue;
		}

		assert(obj->get_instance_id() == c.eng_handle);
	}

	// refresh handles i guess ? fixme
	handles.clear();

	ed_doc.post_node_changes.invoke();
}

CreateStaticMeshCommand::CreateStaticMeshCommand(EditorDoc& ed_doc, const std::string& modelname,
												 const glm::mat4& transform, EntityPtr parent)
	: ed_doc(ed_doc) {

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
		} else
			sys_print(Warning, "CreateStaticMeshCommand: ent handle invalid in async callback\n");
	}
}

void CreateStaticMeshCommand::undo() {
	handle->destroy();
	ed_doc.post_node_changes.invoke();
	handle = {};
}

CreateCppClassCommand::CreateCppClassCommand(EditorDoc& ed_doc, const std::string& cppclassname,
											 const glm::mat4& transform, EntityPtr parent, bool is_component)
	: ed_doc(ed_doc) {
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
		ent = ed_doc.spawn_entity();	  // eng->get_level()->spawn_entity();
		ed_doc.attach_component(ti, ent); // ent->create_component_type(ti);
	} else
		ent = ed_doc.spawn_entity();

	if (parent_to.get())
		ent->parent_to(parent_to.get());

	ent->set_ws_transform(transform);

	ASSERT(!ent->dont_serialize_or_edit);

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
	// auto level = eng->get_level();
	// level->destroy_entity(ent);
	ed_doc.post_node_changes.invoke();
	handle = {};
}

TransformCommand::TransformCommand(EditorDoc& ed_doc, const std::unordered_set<uint64_t>& selection,
								   const std::unordered_map<uint64_t, glm::mat4>& pre_transforms)
	: ed_doc(ed_doc) {
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
	ed_doc.selection_state->on_selection_changed.invoke(); // hack
}

void TransformCommand::undo() {
	for (auto& t : transforms) {
		if (t.ptr.get()) {
			t.ptr->set_ws_transform(t.pre_transform);
		}
	}
	ed_doc.selection_state->on_selection_changed.invoke(); // hack
}

DuplicateEntitiesCommand::DuplicateEntitiesCommand(EditorDoc& ed_doc, std::vector<EntityPtr> handles) : ed_doc(ed_doc) {

	if (handles.empty())
		is_valid_flag = false;

	if (!is_valid_flag)
		return;

	// todo: validation

	scene = CommandSerializeUtil::serialize_entities_text(ed_doc, handles);
}

void DuplicateEntitiesCommand::execute() {
	UnserializedSceneFile duplicated = unserialize_entities_from_text("duplicate_entities", scene->text,
																	  AssetDatabase::loader, false /* dont keep id*/);

	// auto& extern_parents = scene->extern_parents;

	// zero out file ids so new ones are set
	// for (auto o : duplicated.get_objects())
	//	if (o.second->creator_source == nullptr) // ==nullptr meaning that its created by level
	//		o.second->unique_file_id = 0;

	ed_doc.insert_unserialized_into_scene(duplicated);

	// eng->get_level()->insert_unserialized_entities_into_level(duplicated);	// since duplicating, DONT pass in scene

	ed_doc.selection_state->clear_all_selected();

	vector<EntityPtr> ents;
	for (auto e : duplicated.all_obj_vec)
		if (auto ent = e->cast_to<Entity>()) {
			ents.push_back(ent);
			handles.push_back(ent);
		}
	ed_doc.selection_state->add_entities_to_selection(ents);

	ed_doc.manipulate->set_force_op(ImGuizmo::TRANSLATE);
	ed_doc.manipulate->set_force_gizmo_on(true);

	ed_doc.post_node_changes.invoke();
}

void DuplicateEntitiesCommand::undo() {
	for (auto h : handles) {
		h->destroy();
	}

	ed_doc.post_node_changes.invoke();
}

MovePositionInHierarchy::MovePositionInHierarchy(EditorDoc& ed_doc, Entity* e, Cmd cmd) : ed_doc(ed_doc) {
	if (!e)
		return;
	const auto parent = e->get_parent();
	if (!parent)
		return;
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
void MovePositionInHierarchy::execute() {
	auto e = entPtr.get();
	if (!e || !e->get_parent())
		return;
	e->get_parent()->move_child_entity_index(e, to_position);

	ed_doc.post_node_changes.invoke();
}
void MovePositionInHierarchy::undo() {
	auto e = entPtr.get();
	if (!e || !e->get_parent())
		return;
	e->get_parent()->move_child_entity_index(e, from_position);

	ed_doc.post_node_changes.invoke();
}
#include "LevelSerialization/SerializeNew.h"
std::unique_ptr<SerializedSceneFile> CommandSerializeUtil::serialize_entities_text(EditorDoc& ed_doc,
																				   std::vector<EntityPtr> handles) {
	std::vector<Entity*> ents;
	for (auto h : handles) {
		ents.push_back(h.get());
	}
	ed_doc.validate_fileids_before_serialize();

	return std::make_unique<SerializedSceneFile>(
		NewSerialization::serialize_to_text("Command::serialize_entities_text", ents, true));
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
#endif

CreateSpawnerCommand::CreateSpawnerCommand(EditorDoc& ed_doc, const std::string& cppclassname,
										   const glm::mat4& transform)
	: ed_doc(ed_doc) {
	this->transform = transform;
	this->cppclassname = cppclassname;
}
#include "Game/Components/SpawnerComponenth.h"
void CreateSpawnerCommand::execute() {
	Entity* e = ed_doc.spawn_entity();
	e->set_ws_transform(transform);
	auto s = e->create_component<SpawnerComponent>();
	s->set(cppclassname);
	handle = e;
}

void CreateSpawnerCommand::undo() {
	if (handle.get())
		handle->destroy();
}
#ifdef EDITOR_BUILD

#include <SDL2/SDL_events.h>
UndoRedoSystem::UndoRedoSystem() {
	hist.resize(HIST_SIZE, nullptr);
}
void UndoRedoSystem::on_key_event(const SDL_KeyboardEvent& key) {
	if (key.keysym.scancode == SDL_SCANCODE_Z && key.keysym.mod & KMOD_CTRL)
		undo();
}

void UndoRedoSystem::clear_all() {
	for (int i = 0; i < hist.size(); i++) {
		delete hist[i];
		hist[i] = nullptr;
	}
}

// returns number of errord commands
//

int UndoRedoSystem::execute_queued_commands() {

	int errored_command_count = 0;
	for (auto& [c, callback] : queued_commands) {

		if (!c->is_valid()) {
			sys_print(Warning, "execute_queued_commands: command not valid %s\n", c->to_string().c_str());
			if (callback)
				callback(false);
			delete c;
			errored_command_count++;
			continue;
		}
		sys_print(Debug, "execute_queued_commands: executing: %s\n", c->to_string().c_str());
		try {
			c->execute();
			if (callback)
				callback(true);

			if (hist[index]) {
				delete hist[index];
			}
			hist[index] = c;
			index += 1;
			index %= HIST_SIZE;
			eng->log_to_fullscreen_gui(Info, c->to_string().c_str());
		}
		catch (std::runtime_error er) {
			sys_print(Error, "execute_queued_commands: command threw exception: %s\n", er.what());
			errored_command_count++;

			if (callback)
				callback(false);
			delete c;
		}
	}
	bool had_commands = !queued_commands.empty();
	queued_commands.clear();
	if (had_commands)
		on_command_execute_or_undo.invoke();

	return errored_command_count;
}
void UndoRedoSystem::undo() {
	index -= 1;
	if (index < 0)
		index = HIST_SIZE - 1;
	if (hist[index]) {

		sys_print(Debug, "Undoing: %s\n", hist[index]->to_string().c_str());

		eng->log_to_fullscreen_gui(Info, "Undo");

		hist[index]->undo();
		delete hist[index];
		hist[index] = nullptr;

		on_command_execute_or_undo.invoke();
	} else {
		eng->log_to_fullscreen_gui(Warning, "Nothing to undo");

		sys_print(Debug, "nothing to undo\n");
	}
}
#endif

void CreateComponentCommand::execute() {
	ASSERT(comp_handle == 0);
	auto e = ent.get();
	if (!e) {
		sys_print(Warning, "no entity in createcomponentcommand\n");
		return;
	}
	auto ec = ed_doc.attach_component(info, e);
	comp_handle = ec->get_instance_id();
	post_create(ec);

	ed_doc.on_component_created.invoke(ec);
}

void CreateComponentCommand::post_create(Component* ec) {}

void CreateComponentCommand::undo() {
	ASSERT(comp_handle != 0);
	auto obj = eng->get_object(comp_handle);
	ASSERT(obj->is_a<Component>());
	auto ec = (Component*)obj;
	auto id = ec->get_instance_id();
	ec->destroy();
	ed_doc.on_component_deleted.invoke(id);
	comp_handle = 0;
}

void CreateEntityCommand::execute() {
	Entity* e = doc.spawn_entity();
	ptr = e;
	post_create(e);
}

#include "Game/Components/PrefabAssetComponent.h"
static Entity* spawn_prefab(EditorDoc& ed_doc, const std::string& path, const glm::mat4 transform) {
	auto ent = ed_doc.spawn_entity()->create_component<PrefabAssetComponent>();
	ent->update_path(path);
	ent->get_owner()->set_ws_transform(transform);
	return ent->get_owner();
}

InstantiatePrefabCommand::InstantiatePrefabCommand(EditorDoc& ed_doc, const std::string& prefab_path,
												   const glm::mat4& transform)
	: ed_doc(ed_doc), prefab_path(prefab_path), transform(transform) {}
void InstantiatePrefabCommand::execute() {

	try {
		auto e = spawn_prefab(ed_doc, prefab_path, transform);
		handles.push_back(e);
	}
	catch (const std::exception& e) {
		sys_print(Warning, "Failed to deserialize prefab %s: %s\n", prefab_path.c_str(), e.what());
	}
}

void InstantiatePrefabCommand::undo() {
	for (auto& h : handles) {
		if (auto entity = h.get()) {
			entity->destroy();
		}
	}
	handles.clear();
}

MakePrefabFromSelectionCommand::MakePrefabFromSelectionCommand(EditorDoc& ed_doc,
															   const std::vector<EntityPtr>& selection,
															   const std::string& save_path)
	: ed_doc(ed_doc), save_path(save_path), selection(selection) {}

void MakePrefabFromSelectionCommand::execute() {
	std::vector<Entity*> entities;
	for (auto& ptr : selection) {
		if (auto entity = ptr.get()) {
			entities.push_back(entity);
		}
	}

	if (entities.empty()) {
		sys_print(Warning, "No entities to save to prefab\n");
		return;
	}

	// Serialize entities to text (no IDs - fresh instantiation)
	try {
		// Extract filename from path for display in JSON metadata
		std::string prefab_name = save_path;
		size_t last_slash = prefab_name.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			prefab_name = prefab_name.substr(last_slash + 1);
		}
		auto serialized = NewSerialization::serialize_to_text("make_prefab", entities, false, prefab_name.c_str());
		prefab_text = std::make_unique<SerializedSceneFile>(serialized);

		// Save to file
		if (!PrefabFile::save_text(save_path, serialized.text)) {
			sys_print(Warning, "Failed to save prefab to: %s\n", save_path.c_str());
			prefab_text = nullptr;
		}
	}
	catch (const std::exception& e) {
		sys_print(Warning, "Failed to serialize prefab: %s\n", e.what());
	}
	ed_doc.post_node_changes.invoke();
}

void MakePrefabFromSelectionCommand::undo() {
	// Note: File undo is not easily reversible, so we just log it.
	// In a more robust system, we might want to back up the original file first.
	sys_print(Info, "Undo make prefab: file %s persists (manual deletion needed if unwanted)\n", save_path.c_str());
}

MakePrefabAndReplaceCommand::MakePrefabAndReplaceCommand(EditorDoc& ed_doc, const std::vector<EntityPtr>& selection,
														 const std::string& prefab_path)
	: ed_doc(ed_doc), prefab_path(prefab_path), selection(selection) {}

void MakePrefabAndReplaceCommand::execute() {
	// Step 1: Collect entity pointers and serialize
	std::vector<Entity*> entities;
	for (auto& ptr : selection) {
		if (auto entity = ptr.get()) {
			entities.push_back(entity);
		}
	}

	if (entities.empty()) {
		sys_print(Warning, "No entities to save to prefab\n");
		return;
	}

	// Step 2: Calculate bounding box and center of all entities
	glm::vec3 bbox_min(std::numeric_limits<float>::infinity());
	glm::vec3 bbox_max(-std::numeric_limits<float>::infinity());

	for (auto entity : entities) {
		glm::vec3 pos = entity->get_ws_position();
		bbox_min = glm::min(bbox_min, pos);
		bbox_max = glm::max(bbox_max, pos);
	}

	bbox_center = (bbox_min + bbox_max) * 0.5f;

	// Step 3: Store original positions and offset entities to center at origin
	std::vector<glm::vec3> original_positions;
	for (auto entity : entities) {
		original_positions.push_back(entity->get_ws_position());
		glm::vec3 new_pos = entity->get_ws_position() - bbox_center;
		entity->set_ws_position(new_pos);
	}

	// Step 4: Serialize and save to prefab file (with centered entities)
	try {
		// Extract filename from path for display in JSON metadata
		std::string prefab_name = prefab_path;
		size_t last_slash = prefab_name.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			prefab_name = prefab_name.substr(last_slash + 1);
		}
		auto serialized =
			NewSerialization::serialize_to_text("make_prefab_replace", entities, false, prefab_name.c_str());
		original_selection = std::make_unique<SerializedSceneFile>(serialized);

		if (!PrefabFile::save_text(prefab_path, serialized.text)) {
			sys_print(Warning, "Failed to save prefab to: %s\n", prefab_path.c_str());
			original_selection = nullptr;
			// Restore positions on failure
			for (size_t i = 0; i < entities.size(); ++i) {
				entities[i]->set_ws_position(original_positions[i]);
			}
			return;
		}
	}
	catch (const std::exception& e) {
		sys_print(Warning, "Failed to serialize prefab: %s\n", e.what());
		// Restore positions on failure
		for (size_t i = 0; i < entities.size(); ++i) {
			entities[i]->set_ws_position(original_positions[i]);
		}
		return;
	}

	// Step 5: Delete the original entities (which were offset)
	for (auto& ptr : selection) {
		if (auto entity = ptr.get()) {
			entity->destroy();
		}
	}

	// Step 6: Spawn prefab at the original center point (to restore the placement)
	glm::mat4 spawn_transform = glm::translate(glm::mat4(1.0f), bbox_center);
	auto e = spawn_prefab(ed_doc, prefab_path, spawn_transform);
	spawned_prefab_instances.push_back(e);

	ed_doc.post_node_changes.invoke();
}

void MakePrefabAndReplaceCommand::undo() {
	// Destroy spawned instances
	for (auto& h : spawned_prefab_instances) {
		if (auto entity = h.get()) {
			entity->destroy();
		}
	}
	spawned_prefab_instances.clear();

	// Restore original entities from serialization
	if (original_selection) {
		try {
			// Clone the serialization for deserialization
			UnserializedSceneFile unserialized = unserialize_entities_from_text(
				"undo_prefab_replace", original_selection->text, AssetDatabase::loader, true);
			ed_doc.insert_unserialized_into_scene(unserialized);

			// Apply bbox_center offset to restored entities to place them back at original positions
			for (auto obj : unserialized.all_obj_vec) {
				if (auto entity = dynamic_cast<Entity*>(obj)) {
					glm::vec3 current_pos = entity->get_ws_position();
					entity->set_ws_position(current_pos + bbox_center);
				}
			}
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to restore original entities during undo: %s\n", e.what());
		}
	}
}
