#pragma once
#include "LevelSerialization/SerializationAPI.h"
#include "Assets/IAsset.h"


CLASS_H(SceneAsset, IAsset)
public:
	// IAsset overrides
	void sweep_references() const override {}
	bool load_asset(ClassBase*& user) override;
	void post_load(ClassBase*) override {}
	void uninstall() override {}
	void move_construct(IAsset*) override {}

	std::string text;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
};


CLASS_H(PrefabAsset, IAsset)
public:
	// IAsset overrides
	void sweep_references() const override {}
	bool load_asset(ClassBase*& user) override;
	void post_load(ClassBase*) override {}
	void uninstall() override;
	void move_construct(IAsset*) override {}

	std::string text;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
};