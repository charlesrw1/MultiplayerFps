#pragma once
#include "SerializationAPI.h"
#include <unordered_set>
#include <stdexcept>

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
