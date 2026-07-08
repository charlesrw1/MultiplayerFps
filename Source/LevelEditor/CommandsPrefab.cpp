#ifdef EDITOR_BUILD
#include "Commands.h"
#include "EditorDocLocal.h"
#include "GameEnginePublic.h"
#include "Framework/Log.h"
#include "LevelSerialization/SerializeNew.h"
#include "Game/Prefab.h"
#include "Game/Components/PrefabAssetComponent.h"
#include "Game/Components/MeshComponent.h"
#include "Render/Model.h"
#include "Assets/AssetDatabase.h"
#include "Level.h"
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

static Entity* spawn_prefab(EditorDoc& ed_doc, const std::string& path, const glm::mat4 transform) {
	ASSERT(!path.empty());
	auto ent = ed_doc.spawn_entity()->create_component<PrefabAssetComponent>();
	ent->update_path(path);
	ent->get_owner()->set_ws_transform(transform);
	return ent->get_owner();
}

// Materializes a prefab's serialized text as loose, top-level entities positioned at `transform`.
// Shared by InstantiatePrefabCommand's prefab-in-prefab flatten and UnpackPrefabCommand: both turn a
// prefab reference into its literal contents, just triggered from different starting points (a
// dropped asset vs. an existing PrefabAssetComponent instance). Throws on malformed prefab text.
static std::vector<EntityPtr> flatten_prefab_into_scene(EditorDoc& ed_doc, const std::string& prefab_text,
														 const glm::mat4& transform) {
	std::vector<EntityPtr> out;
	UnserializedSceneFile unserialized = NewSerialization::unserialize_from_text("prefab_flatten", prefab_text, false);
	ed_doc.insert_unserialized_into_scene(unserialized);

	for (auto base_updater : unserialized.all_obj_vec) {
		if (auto entity = base_updater->cast_to<Entity>()) {
			if (entity->get_parent())
				continue; // inner-prefab children inherit through their root; skip
			entity->set_ws_transform(transform * entity->get_ls_transform());
			out.push_back(entity);
		}
	}
	return out;
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

		auto prefab_asset = g_assets.find<PrefabAsset>(prefab_path);
		if (!prefab_asset) {
			sys_print(Warning, "Failed to load prefab for flatten: %s\n", prefab_path.c_str());
			return;
		}
		const std::string& prefab_text = prefab_asset->get_text();

		try {
			handles = flatten_prefab_into_scene(ed_doc, prefab_text, transform);
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to flatten prefab %s: %s\n", prefab_path.c_str(), e.what());
		}
		ed_doc.post_node_changes.invoke();
		return;
	}

	// Scene (level) mode: spawn a live PrefabAssetComponent instance. Its internal parenting is
	// rebuilt at runtime and works, but a level file can't serialize parent links (parenting is a
	// prefab-only feature), so warn the user that the hierarchy is only live and would be lost if
	// the instance were unpacked into the level.
	{
		auto prefab_asset = g_assets.find<PrefabAsset>(prefab_path);
		const std::string prefab_text = prefab_asset ? prefab_asset->get_text() : std::string();
		if (prefab_text.find("\"__parent\"") != std::string::npos) {
			sys_print(Warning,
					  "Instantiating prefab '%s' which contains entity parenting: parent links live only "
					  "inside the prefab instance and are not preserved if unpacked into the level.\n",
					  prefab_path.c_str());
			eng->log_to_fullscreen_gui(Warning, "Prefab parenting links are not editable/saved in a level");
		}
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

UnpackPrefabCommand::UnpackPrefabCommand(EditorDoc& ed_doc, std::vector<EntityPtr> targets)
	: ed_doc(ed_doc), targets(std::move(targets)) {}

bool UnpackPrefabCommand::is_valid() {
	for (auto& t : targets) {
		Entity* ent = t.get();
		if (ent && ent->get_component<PrefabAssetComponent>())
			return true;
	}
	return false;
}

void UnpackPrefabCommand::execute() {
	std::vector<EntityPtr> all_unpacked;
	for (auto& t : targets) {
		Entity* ent = t.get();
		PrefabAssetComponent* pac = ent ? ent->get_component<PrefabAssetComponent>() : nullptr;
		if (!pac)
			continue; // not (or no longer) a prefab instance -- skip rather than assert, since
					  // targets may be a mixed selection (see draw_scene_context_menu)

		const std::string path = pac->prefab_path;
		const glm::mat4 transform = ent->get_ws_transform();

		// Save the instance entity so undo can restore it exactly (same pattern as
		// MakePrefabAndReplaceCommand::undo).
		{
			std::vector<Entity*> orig{ent};
			auto serialized =
				NewSerialization::serialize_to_text("unpack_prefab_undo", orig, false, nullptr, nullptr, false);
			original_entities.push_back(std::make_unique<SerializedSceneFile>(serialized));
		}

		auto prefab_asset = g_assets.find<PrefabAsset>(path);
		if (!prefab_asset) {
			sys_print(Warning, "Failed to load prefab for unpack: %s\n", path.c_str());
			original_entities.pop_back();
			continue;
		}

		try {
			auto unpacked = flatten_prefab_into_scene(ed_doc, prefab_asset->get_text(), transform);
			all_unpacked.insert(all_unpacked.end(), unpacked.begin(), unpacked.end());
			unpacked_entities_per_target.push_back(std::move(unpacked));
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to unpack prefab %s: %s\n", path.c_str(), e.what());
			original_entities.pop_back();
			continue;
		}

		ent->destroy();
	}

	ed_doc.selection_state->clear_all_selected();
	ed_doc.selection_state->add_entities_to_selection(all_unpacked);
	ed_doc.post_node_changes.invoke();
}

void UnpackPrefabCommand::undo() {
	for (auto& group : unpacked_entities_per_target) {
		for (auto& h : group) {
			if (auto entity = h.get())
				entity->destroy();
		}
	}
	unpacked_entities_per_target.clear();

	for (auto& original : original_entities) {
		try {
			UnserializedSceneFile unserialized =
				NewSerialization::unserialize_from_text("unpack_prefab_restore", original->text, true);
			ed_doc.insert_unserialized_into_scene(unserialized);
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to restore prefab instance during undo: %s\n", e.what());
		}
	}
	original_entities.clear();
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
		auto serialized = NewSerialization::serialize_to_text("make_prefab", entities, false, prefab_name.c_str(),
															  nullptr, /*serialize_hierarchy*/ true);
		prefab_text = std::make_unique<SerializedSceneFile>(serialized);

		if (!PrefabFile::save_text(save_path, serialized.text)) {
			sys_print(Warning, "Failed to save prefab to: %s\n", save_path.c_str());
			prefab_text = nullptr;
		}
		else if (g_assets.is_asset_loaded(save_path)) {
			// Keep the cached PrefabAsset in sync with what was just written to disk, rather
			// than waiting on the async file watcher to notice the change.
			g_assets.reload<PrefabAsset>(g_assets.find<PrefabAsset>(save_path));
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

	// Calculate the true geometric bounding box of the selection (mesh AABBs, transformed by each
	// entity's world transform incl. scale; entities without a mesh contribute just their origin
	// point) and pivot on its bottom-center -- x/z centered, y at the box's bottom -- so the prefab's
	// local origin lands on the ground plane under the content instead of floating at the geometric
	// center (matches the "F" focus-camera bounds fix; see EditorDoc::set_camera_target_to_sel).
	Bounds selection_bounds;
	for (auto entity : entities) {
		auto mesh = entity->get_component<MeshComponent>();
		if (mesh && mesh->get_model())
			selection_bounds = bounds_union(selection_bounds, transform_bounds(entity->get_ws_transform(), mesh->get_model()->get_bounds()));
		else
			selection_bounds = bounds_union(selection_bounds, entity->get_ws_position());
	}
	const glm::vec3 center = selection_bounds.get_center();
	pivot_ws = glm::vec3(center.x, selection_bounds.bmin.y, center.z);

	// Store original positions and offset entities so the pivot lands at the origin
	std::vector<glm::vec3> original_positions;
	for (auto entity : entities) {
		original_positions.push_back(entity->get_ws_position());
		glm::vec3 new_pos = entity->get_ws_position() - pivot_ws;
		entity->set_ws_position(new_pos);
	}

	// Serialize and save to prefab file (with centered entities)
	try {
		std::string prefab_name = prefab_path;
		size_t last_slash = prefab_name.find_last_of("/\\");
		if (last_slash != std::string::npos) {
			prefab_name = prefab_name.substr(last_slash + 1);
		}
		auto serialized = NewSerialization::serialize_to_text("make_prefab_replace", entities, false,
															  prefab_name.c_str(), nullptr, /*serialize_hierarchy*/ true);
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
		else if (g_assets.is_asset_loaded(prefab_path)) {
			// Keep the cached PrefabAsset in sync with what was just written to disk (this
			// path may overwrite an already-loaded prefab, unlike MakePrefabFromSelection).
			g_assets.reload<PrefabAsset>(g_assets.find<PrefabAsset>(prefab_path));
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

	// Spawn prefab at the pivot point (to restore the placement)
	glm::mat4 spawn_transform = glm::translate(glm::mat4(1.0f), pivot_ws);
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
			UnserializedSceneFile unserialized = NewSerialization::unserialize_from_text(
				"undo_prefab_replace", original_selection->text, true);
			ed_doc.insert_unserialized_into_scene(unserialized);

			// Apply the pivot offset to restored entities to place them back at original positions
			for (auto obj : unserialized.all_obj_vec) {
				if (auto entity = dynamic_cast<Entity*>(obj)) {
					glm::vec3 current_pos = entity->get_ws_position();
					entity->set_ws_position(current_pos + pivot_ws);
				}
			}
		}
		catch (const std::exception& e) {
			sys_print(Warning, "Failed to restore original entities during undo: %s\n", e.what());
		}
	}
}
#endif
