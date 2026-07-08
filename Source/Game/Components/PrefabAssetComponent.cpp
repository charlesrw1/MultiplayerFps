#include "PrefabAssetComponent.h"
#include "Game/Prefab.h"
#include "Game/Entity.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Framework/Util.h"

void PrefabAssetComponent::refresh_after_prefab_reload(PrefabAsset* reloaded) {
	if (reloaded && reloaded->get_name() == prefab_path)
		update_path(prefab_path);
}

void PrefabAssetComponent::start() {

	if (prefab_path.empty()) {
		sys_print(Warning, "PrefabAssetComponent: prefab_path is empty\n");
		return;
	}
	update_path(prefab_path);
}

void PrefabAssetComponent::stop() {
	// Destroy the entities this component spawned -- but only the ones still actually
	// parented to us. Callers like the prefab-in-prefab auto-flatten (EditorDocLocal.cpp)
	// deliberately reparent our children elsewhere *before* destroying the owner entity, so
	// they survive independently; destroying them here regardless of current parent would
	// undo that promotion out from under the caller.
	/*
	Entity* owner = get_owner();
	for (auto& entity_ptr : spawned_entities) {
		if (auto entity = entity_ptr.get()) {
			if (entity->get_parent() == owner)
				entity->destroy();
		}
	}
	spawned_entities.clear();
	*/
}
#include "Level.h"
void PrefabAssetComponent::update_path(std::string new_path) {
	PrefabAssetComponent::stop();
	this->prefab_path = new_path;

	// Load prefab text via the cached, hot-reloadable PrefabAsset (not a raw disk read).
	auto asset = g_assets.find<PrefabAsset>(prefab_path);
	if (!asset) {
		sys_print(Warning, "PrefabAssetComponent: failed to load prefab: %s\n", prefab_path.c_str());
		return;
	}
	const std::string& prefab_text = asset->get_text();

	// Deserialize entities from the prefab file
	// keepid=false: assign new instance IDs (this is runtime instantiation, not persistence)
	try {
		UnserializedSceneFile unserialized =
			NewSerialization::unserialize_from_text("prefab_instantiate", prefab_text, false);

		// Instantiate all deserialized entities and parent them to this component's owner
		Entity* owner = get_owner();
		if (!owner) {
			sys_print(Warning, "PrefabAssetComponent: owner entity is null\n");
			unserialized.delete_objs();
			return;
		}
		eng->get_level()->insert_unserialized_entities_into_level(unserialized);
		for (auto base_updater : unserialized.all_obj_vec) {
			if (!base_updater)	// if entities were stripped (in game builds, then they are nullptr here). strippping only happens in game builds, not editor builds. 
				continue;
			if (auto entity = base_updater->cast_to<Entity>()) {
				// Only the prefab's root entities attach to the owner. Entities that already have a
				// parent were linked to another entity *inside* the prefab by unserialize_from_text;
				// reparenting them here would flatten (and thus break) the prefab's own hierarchy.
				if (!entity->get_parent()) {
					entity->parent_to(owner);
					//spawned_entities.push_back(entity);
				}
				entity->dont_serialize_or_edit = true;
			}
		}
	}
	catch (const std::exception& e) {
		sys_print(Warning, "PrefabAssetComponent: failed to deserialize prefab %s: %s\n", prefab_path.c_str(),
				  e.what());
		return;
	}
}
