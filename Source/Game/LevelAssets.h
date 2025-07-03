#pragma once

#include "Assets/IAsset.h"
#include "Framework/Hashmap.h"
#include "Framework/Reflection2.h"
#include <memory>
#include <glm/glm.hpp>
#include "Framework/ConsoleCmdGroup.h"
#include "Framework/SerializedForDiffing.h"

class BaseUpdater;
class UnserializedSceneFile;
class Entity;
class SceneAsset : public IAsset {
public:
	CLASS_BODY(SceneAsset);
	SceneAsset();
	~SceneAsset();
	// IAsset overrides
	void sweep_references(IAssetLoadingInterface* load) const override {}
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override;
	void uninstall() override;
	void move_construct(IAsset*) override;

	UnserializedSceneFile unserializeStep2();

	uptr<SerializedForDiffing> halfUnserialized;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
};

struct SceneSerialized;
class PrefabAsset : public IAsset {
public:
	CLASS_BODY(PrefabAsset);
	PrefabAsset();
	~PrefabAsset();

	REF static PrefabAsset* load(string name);

	Entity& instantiate(const glm::vec3& position, const glm::quat& rot) const;
	const Entity& get_default_object() const;

	UnserializedSceneFile unserialize(IAssetLoadingInterface* load) const;

	std::unique_ptr<UnserializedSceneFile> sceneFile;
	hash_map<BaseUpdater*> instance_ids_for_diffing;
	uptr<SerializedForDiffing> halfUnserialized;
private:
	// IAsset overrides
	void sweep_references(IAssetLoadingInterface* load) const override;
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override;
	void uninstall() override;
	void move_construct(IAsset*) override;
};