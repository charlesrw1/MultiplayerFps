#pragma once

#include <unordered_set>
#include <stdexcept>

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include "Framework/SerializedForDiffing.h"

// lots of junk, clean up after rewrite 1/30

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

class SerializedSceneFile
{
public:
	std::string text;
};

bool this_is_a_serializeable_object(const BaseUpdater* b);
bool serialize_this_objects_children(const Entity* b);


class SerializeInputError : public std::runtime_error {
public:
	SerializeInputError(string er_str) :std::runtime_error("SerializeInvalidInput: " + er_str) {
	}
};
using std::string;
using std::unordered_map;
class MakePathForObjectNew;
struct SerializedForDiffing;
class NewSerialization
{
public:
	// throws SerializeInputError on bad input
	static SerializedSceneFile serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs, bool write_ids);
	static UnserializedSceneFile unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load, bool keepid);
	static UnserializedSceneFile unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, IAssetLoadingInterface& load, bool keepid);

};
