#pragma once

#include "Assets/IAsset.h"
#include "Framework/Hashmap.h"
#include "Framework/Reflection2.h"
#include <memory>
#include <glm/glm.hpp>
#include "Framework/ConsoleCmdGroup.h"
#include "Framework/SerializedForDiffing.h"
#include <unordered_set>

class BaseUpdater;
class UnserializedSceneFile;
class Entity;
class SceneAsset : public IAsset {
public:
	CLASS_BODY(SceneAsset);
	SceneAsset();
	~SceneAsset();
	// IAsset overrides
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override;
	void uninstall() override;
	void move_construct(IAsset*) override;

	UnserializedSceneFile unserializeStep2();

	uptr<SerializedForDiffing> halfUnserialized;
	std::unique_ptr<UnserializedSceneFile> sceneFile;
};

class IPrefabFactory : public ClassBase{
public:
	CLASS_BODY(IPrefabFactory, scriptable);
	REF virtual void start() {}
	REF virtual bool create(Entity* e, string name) { return false; }
	REF void define_prefab(string s) {
		defined_prefabs.insert(s);
	}
	std::unordered_set<std::string> defined_prefabs;
};

struct SceneSerialized;
class PrefabAsset : public IAsset {
public:
	CLASS_BODY(PrefabAsset);
	PrefabAsset();
	~PrefabAsset();

	REF static PrefabAsset* load(string name);
	// fixme refactoring

	void finish_prefab_setup(Entity* me) const;

	static void init_prefab_factory();
	static IPrefabFactory* factory;
private:
	// IAsset overrides
	bool load_asset(IAssetLoadingInterface* load) override;
	void post_load() override;
	void uninstall() override;
	void move_construct(IAsset*) override;

};