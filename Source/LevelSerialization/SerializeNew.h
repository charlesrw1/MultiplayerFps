#pragma once

#include <stdexcept>
#include <string>
#include <vector>
#include <json.hpp>
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
#include "Framework/SerializedForDiffing.h"

class BaseUpdater;
class Entity;
class Level;

// A deferred parent/child (and prefab-only flag) link captured while unserializing a prefab.
// Applied by Level::insert_* AFTER every entity is inserted+initialized, never during unserialize:
// the insert contract requires all scene-file entities to be unparented on entry (see the
// EDITOR_BUILD assert in Level::insert_unserialized_entities_into_level_internal).
struct SceneHierarchyLink
{
	Entity* child = nullptr;
	Entity* parent = nullptr; // null => only the flags below apply (a root that is top-level/bone-attached)
	bool is_top_level = false;
	bool has_bone = false;
	std::string parent_bone;
};

// Owns the BaseUpdater* in all_obj_vec until ownership is transferred elsewhere.
// Call mark_ownership_transferred() once a consumer (e.g. Level::insert_*) has
// taken the pointers, so the destructor doesn't double-free. Call delete_objs()
// on error paths to release immediately.
class UnserializedSceneFile
{
public:
	UnserializedSceneFile() = default;
	~UnserializedSceneFile() { delete_objs(); }
	UnserializedSceneFile(UnserializedSceneFile&& other) noexcept
		: all_obj_vec(std::move(other.all_obj_vec)), unknown_objs(std::move(other.unknown_objs)),
		  unknown_field_warnings(std::move(other.unknown_field_warnings)), hierarchy(std::move(other.hierarchy)),
		  ownership_transferred(other.ownership_transferred) {
		other.ownership_transferred = true; // moved-from owns nothing
	}
	UnserializedSceneFile& operator=(UnserializedSceneFile&& other) noexcept {
		if (this != &other) {
			delete_objs();
			all_obj_vec = std::move(other.all_obj_vec);
			unknown_objs = std::move(other.unknown_objs);
			unknown_field_warnings = std::move(other.unknown_field_warnings);
			hierarchy = std::move(other.hierarchy);
			ownership_transferred = other.ownership_transferred;
			other.ownership_transferred = true;
		}
		return *this;
	}
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;

	// Frees every pointer and clears the vector. Safe to call multiple times.
	void delete_objs();
	// Caller (typically Level::insert_*) signals it now owns the pointers; dtor will skip free.
	void mark_ownership_transferred() { ownership_transferred = true; }

	std::vector<BaseUpdater*> all_obj_vec;
	// Raw entity JSON for objs whose __typename couldn't be instantiated. The loader
	// preserves them so they round-trip through save without being lost.
	std::vector<nlohmann::json> unknown_objs;
	// One pre-formatted "<typename>.<field>" string per JSON key the reflection readers
	// didn't consume on an otherwise-resolved entity. Surfaced in the editor so stale/
	// typo'd fields aren't only logged.
	std::vector<std::string> unknown_field_warnings;
	// Deferred prefab hierarchy links. Empty for level files (which never serialize parenting).
	// Applied post-insert by Level::insert_*; see SceneHierarchyLink.
	std::vector<SceneHierarchyLink> hierarchy;

private:
	bool ownership_transferred = false;
	friend class Level;
};

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
	// throws SerializeInputError on bad input.
	// serialize_hierarchy: when true (prefab saves) parent/child links are persisted via a
	// `__parent` array-index reference (plus `__is_top_level` / `__parent_bone`) and children
	// are emitted instead of skipped. When false (level saves) the legacy flat format is kept:
	// parented entities are skipped exactly as before, so .tmap files are byte-for-byte unchanged.
	static SerializedSceneFile serialize_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs,
												 bool write_ids, const char* prefab_name = nullptr,
												 const std::vector<nlohmann::json>* preserved_unknown_objs = nullptr,
												 bool serialize_hierarchy = false);
	static UnserializedSceneFile unserialize_from_text(const char* debug_tag, const std::string& text, bool keepid);
	static UnserializedSceneFile unserialize_from_json(const char* debug_tag, SerializedForDiffing& json, bool keepid);

	// Strips the leading "!json" marker (see serialize_to_text) and parses the remainder.
	// Throws SerializeInputError on a missing marker or malformed JSON. Shared by
	// unserialize_from_text and any caller (e.g. SetEntityStateCommand) that needs the raw
	// per-object JSON without going through the full scene unserialize/instantiate path.
	static nlohmann::json parse_scene_json(const char* debug_tag, const std::string& text);
};
