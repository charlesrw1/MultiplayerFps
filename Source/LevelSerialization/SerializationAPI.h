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
		all_obj_vec = std::move(other.all_obj_vec);
		file_id_to_obj = std::move(other.file_id_to_obj);

		root_entity = other.root_entity;
		num_roots = other.num_roots;
	}
	UnserializedSceneFile& operator=(const UnserializedSceneFile&) = delete;
	UnserializedSceneFile(const UnserializedSceneFile&) = delete;

	BaseUpdater* find(int fileId);
	void delete_objs();

	Entity* get_root_entity() const {
		return root_entity;
	}
	void set_root_entity(Entity* root) {
		root_entity = root;
	}

	void unserialize_post_assign_ids();

	std::vector<BaseUpdater*> all_obj_vec;
	std::unordered_map<int, BaseUpdater*> file_id_to_obj;

	Entity* root_entity = nullptr;
	int num_roots = 0;
private:
	void add_components_and_children_from_entity_R(const std::string& path, Entity* e, Entity* source);
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
UnserializedSceneFile unserialize_entities_from_text(const char* debug_tag, const std::string& text, IAssetLoadingInterface* load);
#include "Framework/SerializedForDiffing.h"


class SerializedSceneFile
{
public:
	std::string text;

	// for putting back serialized data into the scene
	std::unordered_map<int, uint64_t> path_to_instance_handle;

	// list of parents that didnt get serialized with set, for putting back in scene
	struct external_parent {
		uint64_t external_parent_handle;
		int child_id = 0;
		//std::string child_path;
	};
	std::vector<external_parent> extern_parents;
};

// rules:
// * if a object is part of a prefab, then entire prefab has to be selected
// * all children of a selected object are selected
// * if root component is selectd, then entity is selected
// * prefab: if provided, then will serialize like a prefab, with root heirarchy

SerializedSceneFile serialize_entities_to_text(const char* debug_tag, const std::vector<Entity*>& input_objs);


// helper utils for editor
void add_to_extern_parents(const BaseUpdater* obj, const BaseUpdater* parent, const PrefabAsset* for_prefab, SerializedSceneFile& output);
std::string serialize_build_relative_path(const char* from, const char* to);
std::string unserialize_relative_to_absolute(const char* relative,const char* root);
bool this_is_a_serializeable_object(const BaseUpdater* b);
bool serialize_this_objects_children(const Entity* b);


uint32_t parse_fileid(const std::string& path);
Entity* unserialize_entities_from_text_internal(UnserializedSceneFile& scene, const std::string& text, const std::string& rootpath, PrefabAsset* prefab, Entity* starting_root, IAssetLoadingInterface* load);
std::vector<Entity*> root_objects_to_write(const std::vector<Entity*>& input_objs);
using std::vector;
using std::string;
class IAsset;

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