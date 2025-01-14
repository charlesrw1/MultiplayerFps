#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
class BaseUpdater;
class PrefabAsset;
class Entity;
class EntityComponent;
class UnserializedSceneFile
{
public:
	UnserializedSceneFile() = default;
	~UnserializedSceneFile() = default;
	UnserializedSceneFile(UnserializedSceneFile&& other) {
		all_objs = std::move(other.all_objs);
		objs_with_extern_references = std::move(other.objs_with_extern_references);
	}
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;


	void add_obj(const std::string& path, Entity* parent_ent, BaseUpdater* e, Entity* opt_source_owner=nullptr, PrefabAsset* opt_prefab=nullptr);
	BaseUpdater* find(const std::string& path);
	void delete_objs();

	std::unordered_map<std::string, BaseUpdater*>& get_objects() {
		return all_objs;
	}
private:
	void add_components_and_children_from_entity_R(const std::string& path, Entity* e, Entity* source);
	std::unordered_map<std::string, BaseUpdater*> all_objs;	// fileID/.../fileID -> entity/component
	std::unordered_set<BaseUpdater*> objs_with_extern_references;
	friend class Level;
};

// API:
// serialize entity collection
// unserialize string to entity collection
// text and binary forms

// with UnserializedSceneFile, you have a collection of entities that havent been initalized yet. 
// * need to assign instance handles to them
// * any EntHandle or CompHandle are set to POINTERS! after setting instance handles, go through obj_with_extern_references and update handles to integer values
// * needs init to be called

UnserializedSceneFile unserialize_entities_from_text(const std::string& text);

class SerializedSceneFile
{
public:
	std::string text;

	// for putting back serialized data into the scene
	// serialized text references paths ("205/1"), handles are for locating instances in scene
	std::unordered_map<std::string, int64_t> path_to_instance_handle;

	// list of parents that didnt get serialized with set, for putting back in scene
	struct external_parent {
		int64_t external_parent_handle;
		std::string child_path;
	};
	std::vector<external_parent> extern_parents;
};

// rules:
// * if a object is part of a prefab, then entire prefab has to be selected
// * all children of a selected object are selected
// * if root component is selectd, then entity is selected

SerializedSceneFile serialize_entities_to_text(const std::vector<Entity*>& input_objs);