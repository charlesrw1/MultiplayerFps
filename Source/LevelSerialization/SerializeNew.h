#pragma once
#include "SerializationAPI.h"
#include <unordered_set>
#include <stdexcept>
class SerializeEntitiesContainer : public ClassBase
{
public:
	CLASS_BODY(SerializeEntitiesContainer);
	void serialize(Serializer& s);
	std::unordered_set<BaseUpdater*> objects;
};
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
	static SerializedSceneFile serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs);
	static UnserializedSceneFile unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load);
	static UnserializedSceneFile unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, IAssetLoadingInterface& load);
	static void unserialize_shared(const char* debug_tag, UnserializedSceneFile& out, ReadSerializerBackendJson& backend);

	static void add_objects_to_write(const char* debug_tag, SerializeEntitiesContainer& con, Entity& e);
	static void add_objects_to_container(const char* debug_tag, const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, SerializedSceneFile& output);
};