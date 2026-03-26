#pragma once
#include <vector>
#include <unordered_set>
#include "Framework/Hashmap.h"
#include "Framework/Hashset.h"
#include "Framework/ScopedBoolean.h"
#include "Framework/InlineVec.h"
#include "Framework/ConsoleCmdGroup.h"
#include "Game/EntityPtr.h"
class SceneAsset;
class Entity;
class GameMode;
class UnserializedSceneFile;
class SerializedSceneFile;
class BaseUpdater;
class ClassTypeInfo;
class Component;
using std::string;
class Level
{
public:
	// constructed in GameEngineLocal::on_map_change_callback
	Level(bool is_editor);
	void start(string source_name, UnserializedSceneFile* source); // called right after ctor
	~Level();
	void insert_unserialized_entities_into_level(UnserializedSceneFile& scene); // was bool assign_new_ids=false
	// ends the level
	void close_level();
	void update_level();
	void sync_level_render_data();
	// this adds to the sync list to defer if in game task, or immedetaly calls on_sync() otherwise
	void add_to_sync_render_data_list(Component* ec);
	void add_to_update_list(Component* ec);
	void remove_from_update_list(Component* ec);
	// destroy an entity, calls desroy_internal(), deletes, removes from list
	void destroy_entity(Entity* e);
	void destroy_component(Component* e);
	Entity* spawn_entity();
	void add_and_init_created_runtime_component(Component* c);
	const ScopedBooleanValue& get_is_in_update() const { return b_is_in_update_tick; }
	BaseUpdater* get_entity(int64_t handle) { return all_world_ents.find(handle); }
	string get_source_asset_name() const {
		return source_name;
		;
	}
	const hash_map<BaseUpdater*>& get_all_objects() const { return all_world_ents; }
	// appends object to list that will be destroyed at end of frame, instead of instantly
	void queue_deferred_delete(BaseUpdater* e);

	void validate();
	Entity* find_initial_entity_by_name(const string& name) const;
	Component* find_first_component(const ClassTypeInfo* type) const;

private:
	void insert_unserialized_entities_into_level_internal(UnserializedSceneFile& scene, bool addSpawnNames);
	std::unordered_map<string, obj<Entity>> spawnNameToEntity;
	// all entities/components in the map
	hash_map<BaseUpdater*> all_world_ents;
	hash_set<Component> tick_list;
	// last instance id, incremented to add objs
	int64_t last_id = 0;
	std::vector<Component*> wantsToAddToUpdate;
	hash_set<Component> wants_sync_update;
	std::unordered_set<int64_t> deferred_delete_list;
	ScopedBooleanValue b_is_in_update_tick;
	ScopedBooleanValue b_is_in_level_startup;
	string source_name = "";

	void insert_new_native_entity_into_hashmap_R(Entity* e);
	void initialize_new_entity_safe(Entity* e);
	Entity* spawn_entity_class_deferred_internal(const ClassTypeInfo& ti);
	int64_t get_next_id_and_increment() {
		return ++last_id; // prefix
	}

	friend class DeferredSpawnScope; // for initialize_new_entity_safe
};