#pragma once
#include "SerializationAPI.h"
#include <unordered_set>

class SerializeEntitiesContainer : public ClassBase
{
public:
	CLASS_BODY(SerializeEntitiesContainer);
	void serialize(Serializer& s);
	std::unordered_set<BaseUpdater*> objects;
};
class NewSerialization
{
public:
	static SerializedSceneFile serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs, PrefabAsset* opt_prefab = nullptr);
	static UnserializedSceneFile unserialize_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface& load, PrefabAsset* opt_source_prefab);
	static void add_objects_to_write(const char* debug_tag, SerializeEntitiesContainer& con, Entity& e, PrefabAsset* for_prefab);
	static void add_objects_to_container(const char* debug_tag, const std::vector<Entity*>& input_objs, SerializeEntitiesContainer& container, PrefabAsset* for_prefab, SerializedSceneFile& output);
};