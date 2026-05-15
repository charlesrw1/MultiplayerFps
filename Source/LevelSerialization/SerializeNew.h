#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include "Framework/SerializedForDiffing.h"

class BaseUpdater;
class Entity;
class Level;

class UnserializedSceneFile
{
public:
	UnserializedSceneFile() = default;
	~UnserializedSceneFile() = default;
	UnserializedSceneFile(UnserializedSceneFile&&) = default;
	UnserializedSceneFile& operator=(UnserializedSceneFile&&) = default;
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;

	void delete_objs();

	std::vector<BaseUpdater*> all_obj_vec;

private:
	friend class Level;
};

UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, bool keepid);

class SerializedSceneFile
{
public:
	std::string text;
};

bool this_is_a_serializeable_object(const BaseUpdater* b);
bool serialize_this_objects_children(const Entity* b);

class SerializeInputError : public std::runtime_error
{
public:
	SerializeInputError(const std::string& er_str) : std::runtime_error("SerializeInvalidInput: " + er_str) {}
};

struct SerializedForDiffing;
class NewSerialization
{
public:
	// throws SerializeInputError on bad input
	static SerializedSceneFile serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs,
												 bool write_ids, const char* prefab_name = nullptr);
	static UnserializedSceneFile unserialize_from_text(const char* debug_tag, const std::string& text, bool keepid);
	static UnserializedSceneFile unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, bool keepid);
};
