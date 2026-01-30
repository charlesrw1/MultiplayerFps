#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"


// lots of junk, clean up after rewrite

class BaseUpdater;
class PrefabAsset;
class Entity;
class EntityComponent;
class Level;
class UnserializedSceneFile
{
public:
	UnserializedSceneFile() = default;
	~UnserializedSceneFile() = default;
	UnserializedSceneFile(UnserializedSceneFile&& other) {
		all_obj_vec = std::move(other.all_obj_vec);
	}
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;


	void delete_objs();

	std::vector<BaseUpdater*> all_obj_vec;
private:
	friend class Level;
};

class IAssetLoadingInterface;
UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load, bool keepid);
#include "Framework/SerializedForDiffing.h"

class SerializedSceneFile
{
public:
	std::string text;
};

bool this_is_a_serializeable_object(const BaseUpdater* b);
bool serialize_this_objects_children(const Entity* b);
