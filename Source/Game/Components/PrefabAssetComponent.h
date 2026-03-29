#pragma once
#include "Game/EntityComponent.h"
#include "Game/EntityPtr.h"
#include <string>
#include <vector>

// Component that loads and instantiates entities from a prefab file (.tprefab)
// Spawned entities become children of the entity owning this component.
// No hierarchies inside prefabs — all entities are spawned as flat list.
class PrefabAssetComponent : public Component
{
public:
	CLASS_BODY(PrefabAssetComponent);

	PrefabAssetComponent() { set_call_init_in_editor(true); }

	void start() override;
	void stop() override;
	// Path to the .tprefab file (relative to game dir, e.g., "Prefabs/my_prefab.tprefab")

	REFLECT(hide)
	std::string prefab_path;

	void update_path(std::string new_path);

private:
	// Runtime-only: entities spawned from the prefab
	// NOT reflected — not serialized
	std::vector<EntityPtr> spawned_entities;
};
