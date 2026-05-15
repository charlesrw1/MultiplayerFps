#include "PrefabAssetComponent.h"
#include "Game/Prefab.h"
#include "Game/Entity.h"
#include "LevelSerialization/SerializeNew.h"
#include "Assets/AssetDatabase.h"
#include "Framework/Util.h"

void PrefabAssetComponent::start() {

	if (prefab_path.empty()) {
		sys_print(Warning, "PrefabAssetComponent: prefab_path is empty\n");
		return;
	}
	update_path(prefab_path);
}

void PrefabAssetComponent::stop() {
	// Destroy all spawned entities
	for (auto& entity_ptr : spawned_entities) {
		if (auto entity = entity_ptr.get()) {
			entity->destroy();
		}
	}
	spawned_entities.clear();
}
#include "Level.h"
void PrefabAssetComponent::update_path(std::string new_path) {
	PrefabAssetComponent::stop();
	this->prefab_path = new_path;

	// Load prefab file
	std::string prefab_text = PrefabFile::load_text(prefab_path);
	if (prefab_text.empty()) {
		sys_print(Warning, "PrefabAssetComponent: failed to load prefab: %s\n", prefab_path.c_str());
		return;
	}

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
			if (auto entity = base_updater->cast_to<Entity>()) {
				// Parent the entity to this component's owner
				entity->parent_to(owner);
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
