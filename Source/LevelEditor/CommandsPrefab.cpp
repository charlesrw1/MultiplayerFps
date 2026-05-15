#ifdef EDITOR_BUILD
#include "Commands.h"
#include "EditorDocLocal.h"
#include "GameEnginePublic.h"
#include "Framework/Log.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/Prefab.h"
#include "Game/Components/PrefabAssetComponent.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <limits>

static Entity* spawn_prefab(EditorDoc& ed_doc, const std::string& path, const glm::mat4 transform) {
	ASSERT(!path.empty());
	auto ent = ed_doc.spawn_entity()->create_component<PrefabAssetComponent>();
	ent->update_path(path);
	ent->get_owner()->set_ws_transform(transform);
	return ent->get_owner();
}

CreateSpawnerCommand::CreateSpawnerCommand(EditorDoc& ed_doc, const std::string& cppclassname,
										   const glm::mat4& transform)
	: ed_doc(ed_doc) {
	ASSERT(!cppclassname.empty());
	this->transform = transform;
	this->cppclassname = cppclassname;
}
#include "Game/Components/SpawnerComponenth.h"
void CreateSpawnerCommand::execute() {
	ASSERT(!cppclassname.empty());
	Entity* e = ed_doc.spawn_entity();
	e->set_ws_transform(transform);
	auto s = e->create_component<SpawnerComponent>();
	s->set(cppclassname);
	handle = e;
}

void CreateSpawnerCommand::undo() {
	ASSERT(handle.get() != nullptr || true); // handle may be null if execute wasn't called
	if (handle.get())
		handle->destroy();
}

InstantiatePrefabCommand::InstantiatePrefabCommand(EditorDoc& ed_doc, const std::string& prefab_path,
												   const glm::mat4& transform)
	: ed_doc(ed_doc), prefab_path(prefab_path), transform(transform) {
	ASSERT(true); // empty path allowed; is_valid() returns false in that case
}

void InstantiatePrefabCommand::execute() {
	ASSERT(!prefab_path.empty());
	// Prefab-in-prefab: flatten the dropped prefab into root-level entities of the outer prefab.
	// Nested prefab references can't survive serialization (SerializeNew skips parented entities),
	// so we materialize the inner prefab's contents in-place instead.
	if (ed_doc.is_editing_prefab()) {
		sys_print(Warning,
				  "Prefab-in-prefab not supported: flattening '%s' into separate entities of the outer prefab\n",
				  prefab_path.c_str());

		std::string prefab_text = PrefabFile::load_text(prefab_path);
		if (prefab_text.empty()) {
			sys_print(Warning, "Failed to load prefab for flatten: %s\n", prefab_path.c_str());
			return;
		}

		try {
			UnserializedSceneFile unserialized = unserialize_entities_from_text(
				"prefab_flatten", prefab_text, false);
			ed_doc.insert_unserialized_into_scene(unserialized);

			for (auto base_updater : unserialized.all_obj_vec) {
				if (auto entity = base_updater->cast_to<Entity>()) {
					if (entity->get_parent())
						continue; // inner-prefab children inherit through their root; skip
					entity->set_ws_transform(transform * entity->get_ls_transform());
					handles.push_back(entity);
				}
			}
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to flatten prefab %s: %s\n", prefab_path.c_str(), e.what());
		}
		ed_doc.post_node_changes.invoke();
		return;
	}

	try {
		auto e = spawn_prefab(ed_doc, prefab_path, transform);
		handles.push_back(e);
	}
	catch (const std::exception& e) {
		sys_print(Warning, "Failed to deserialize prefab %s: %s\n", prefab_path.c_str(), e.what());
	}
	ed_doc.post_node_changes.invoke();
}

void InstantiatePrefabCommand::undo() {
	ASSERT(true); // handles may be empty if execute failed
	for (auto& h : handles) {
		if (auto entity = h.get()) {
			entity->destroy();
		}
	}
	handles.clear();
	ed_doc.post_node_changes.invoke();
}

MakePrefabFromSelectionCommand::MakePrefabFromSelectionCommand(EditorDoc& ed_doc,
															   const std::vector<EntityPtr>& selection,
															   const std::string& save_path)
	: ed_doc(ed_doc), save_path(save_path), selection(selection) {
	ASSERT(!save_path.empty());
}

void MakePrefabFromSelectionCommand::execute() {
	ASSERT(!save_path.empty());
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

	try {
		std::string prefab_name = save_path;
		size_t last_slash = prefab_name.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			prefab_name = prefab_name.substr(last_slash + 1);
		}
		auto serialized = NewSerialization::serialize_to_text("make_prefab", entities, false, prefab_name.c_str());
		prefab_text = std::make_unique<SerializedSceneFile>(serialized);

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
	ASSERT(!save_path.empty());
	// Note: File undo is not easily reversible, so we just log it.
	sys_print(Info, "Undo make prefab: file %s persists (manual deletion needed if unwanted)\n", save_path.c_str());
}

MakePrefabAndReplaceCommand::MakePrefabAndReplaceCommand(EditorDoc& ed_doc, const std::vector<EntityPtr>& selection,
														 const std::string& prefab_path)
	: ed_doc(ed_doc), prefab_path(prefab_path), selection(selection) {
	ASSERT(!prefab_path.empty());
}

bool MakePrefabAndReplaceCommand::is_valid() {
	return !prefab_path.empty() && !selection.empty() && !ed_doc.is_editing_prefab();
}

void MakePrefabAndReplaceCommand::execute() {
	ASSERT(!prefab_path.empty());
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

	// Calculate bounding box and center of all entities
	glm::vec3 bbox_min(std::numeric_limits<float>::infinity());
	glm::vec3 bbox_max(-std::numeric_limits<float>::infinity());

	for (auto entity : entities) {
		glm::vec3 pos = entity->get_ws_position();
		bbox_min = glm::min(bbox_min, pos);
		bbox_max = glm::max(bbox_max, pos);
	}

	bbox_center = (bbox_min + bbox_max) * 0.5f;

	// Store original positions and offset entities to center at origin
	std::vector<glm::vec3> original_positions;
	for (auto entity : entities) {
		original_positions.push_back(entity->get_ws_position());
		glm::vec3 new_pos = entity->get_ws_position() - bbox_center;
		entity->set_ws_position(new_pos);
	}

	// Serialize and save to prefab file (with centered entities)
	try {
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

	// Delete the original entities (which were offset)
	for (auto& ptr : selection) {
		if (auto entity = ptr.get()) {
			entity->destroy();
		}
	}

	// Spawn prefab at the original center point (to restore the placement)
	glm::mat4 spawn_transform = glm::translate(glm::mat4(1.0f), bbox_center);
	auto e = spawn_prefab(ed_doc, prefab_path, spawn_transform);
	spawned_prefab_instances.push_back(e);

	ed_doc.post_node_changes.invoke();
}

void MakePrefabAndReplaceCommand::undo() {
	ASSERT(!prefab_path.empty());
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
			UnserializedSceneFile unserialized = unserialize_entities_from_text(
				"undo_prefab_replace", original_selection->text, true);
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
#endif
