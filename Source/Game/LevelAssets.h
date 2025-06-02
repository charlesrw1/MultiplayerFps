#pragma once

#include "Assets/IAsset.h"
#include "Framework/Hashmap.h"
#include <memory>

class BaseUpdater;
class UnserializedSceneFile;
CLASS_H(SceneAsset, IAsset)
public:
	~SceneAsset();
	// IAsset overrides
	void sweep_references(IAssetLoadingInterface* load) const override {}
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override {}
	void uninstall() override;
	void move_construct(IAsset*) override;

	std::string text;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
};


CLASS_H(PrefabAsset, IAsset)
public:
	~PrefabAsset();
	// IAsset overrides
	void sweep_references(IAssetLoadingInterface* load) const override;
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override {}
	void uninstall() override;
	void move_construct(IAsset*) override;

	BaseUpdater* find_entity(uint64_t handle) {
		return instance_ids_for_diffing.find(handle);
	}
	std::string text;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
	hash_map<BaseUpdater*> instance_ids_for_diffing;
};