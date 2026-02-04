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

uptr<UnserializedSceneFile> load_level_asset(string path);

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
#include <json.hpp>
class SchemaManager {
public:
	static SchemaManager& get() {
		static SchemaManager inst;
		return inst;
	}
	void init();
	nlohmann::json schema_file;
};
