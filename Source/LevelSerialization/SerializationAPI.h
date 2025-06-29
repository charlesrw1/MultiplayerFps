#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
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
		all_objs = std::move(other.all_objs);
		root_entity = other.root_entity;
	}
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;


	void add_obj(const std::string& path, Entity* parent_ent, BaseUpdater* e, Entity* opt_source_owner=nullptr, PrefabAsset* opt_prefab=nullptr);
	BaseUpdater* find(const std::string& path);
	void delete_objs();

	std::unordered_map<std::string, BaseUpdater*>& get_objects() {
		return all_objs;
	}
	Entity* get_root_entity() const {
		return root_entity;
	}
	void set_root_entity(Entity* root) {
		root_entity = root;
	}

	void unserialize_post_assign_ids();

private:
	void add_components_and_children_from_entity_R(const std::string& path, Entity* e, Entity* source);

	Entity* root_entity = nullptr;
	std::unordered_map<std::string, BaseUpdater*> all_objs;	// fileID/.../fileID -> entity/component
	friend class Level;
};

// API:
// serialize entity collection
// unserialize string to entity collection
// text and binary forms

// with UnserializedSceneFile, you have a collection of entities that havent been initalized yet. 
// * need to assign instance handles to them
// * any EntHandle  are set to POINTERS! after setting instance handles, go through obj_with_extern_references and update handles to integer values
// * needs init to be called
class IAssetLoadingInterface;
UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load, PrefabAsset* opt_source_prefab);

class SerializedSceneFile
{
public:
	std::string text;

	// for putting back serialized data into the scene
	// serialized text references paths ("205/1"), handles are for locating instances in scene
	std::unordered_map<std::string, uint64_t> path_to_instance_handle;

	// list of parents that didnt get serialized with set, for putting back in scene
	struct external_parent {
		uint64_t external_parent_handle;
		std::string child_path;
	};
	std::vector<external_parent> extern_parents;
};

// rules:
// * if a object is part of a prefab, then entire prefab has to be selected
// * all children of a selected object are selected
// * if root component is selectd, then entity is selected
// * prefab: if provided, then will serialize like a prefab, with root heirarchy

SerializedSceneFile serialize_entities_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs, PrefabAsset* opt_prefab = nullptr);


// helper utils for editor
void add_to_extern_parents(const BaseUpdater* obj, const BaseUpdater* parent, const PrefabAsset* for_prefab, SerializedSceneFile& output);
bool this_is_newly_created(const BaseUpdater* b, const PrefabAsset* for_prefab);
bool am_i_the_root_prefab_node(const Entity* b, const PrefabAsset* for_prefab);
std::string serialize_build_relative_path(const char* from, const char* to);
std::string unserialize_relative_to_absolute(const char* relative,const char* root);
std::string build_path_for_object(const BaseUpdater* obj, const PrefabAsset* for_prefab);
const char* get_type_for_new_serialized_item(const BaseUpdater* b, PrefabAsset* for_prefab);
bool this_is_a_serializeable_object(const BaseUpdater* b, const PrefabAsset* for_prefab);
bool serialize_this_objects_children(const Entity* b, const PrefabAsset* for_prefab);
uint32_t parse_fileid(const std::string& path);
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load);
std::vector<Entity*> root_objects_to_write(const std::vector<Entity*>& input_objs);
using std::vector;
using std::string;
class IAsset;

class PrefabToolsUtil {
public:
	static bool is_this_the_root_of_the_prefab(const Entity& e);
	static const Entity& find_root_of_this_prefab(const BaseUpdater& e);
	static const Entity& find_root_of_this_one_prefab(const BaseUpdater& e, const PrefabAsset& asset);
	static bool is_part_of_a_prefab(const BaseUpdater& e);
	static bool is_newly_created(const BaseUpdater& e, const PrefabAsset* editing_prefab);

	static bool is_newly_created_nested(const BaseUpdater& e, const PrefabAsset* editing_prefab);

	static const Entity* get_outer_prefab(const BaseUpdater& e);
	static bool am_i_the_root_prefab_node_for_this_prefab(const Entity& b, const PrefabAsset* for_prefab);
	static bool validate_object_collection(const vector<BaseUpdater*>& objects);
};


// Passed down to serializers

class LevelSerializationContext : public ClassBase {
public:
	CLASS_BODY(LevelSerializationContext);

	SerializedSceneFile* out = nullptr;
	UnserializedSceneFile* in = nullptr;
	BaseUpdater* cur_obj = nullptr;
	std::string* in_root = nullptr;
	PrefabAsset* for_prefab = nullptr;
	PrefabAsset* diffprefab = nullptr;
	BaseUpdater* get_object(uint64_t handle);
};