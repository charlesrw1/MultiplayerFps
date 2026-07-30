#include "PrefabAssetComponent.h"
#include "Game/Prefab.h"
#include "Game/Entity.h"
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
void PrefabAssetComponent::update_path(std::string new_path) {
	PrefabAssetComponent::stop();
	this->prefab_path = new_path;

	Entity* owner = get_owner();
	if (!owner) {
		sys_print(Warning, "PrefabAssetComponent: owner entity is null\n");
		return;
	}

	// Place roots using the prefab's authored local transforms relative to owner (matches the
	// old parent_to()-only behavior), then parent them to owner.
	std::vector<EntityPtr> spawned = PrefabAsset::spawn(prefab_path, owner->get_ws_transform(), owner);
	for (auto& entity_ptr : spawned) {
		if (auto entity = entity_ptr.get())
			entity->dont_serialize_or_edit = true;
	}
}
