#pragma once
#include <vector>
#include <unordered_set>
#include "Framework/Hashmap.h"
#include "Framework/Hashset.h"
#include "Framework/ScopedBoolean.h"
#include "Framework/InlineVec.h"
#include "DeferredSpawnScope.h"
#include "Framework/ConsoleCmdGroup.h"
#include "Game/EntityPtr.h"
class SceneAsset;
class PrefabAsset;
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
	void start(SceneAsset* source);	// called right after ctor
	~Level();
	void insert_unserialized_entities_into_level(UnserializedSceneFile& scene, const SerializedSceneFile* reassign_ids = nullptr); // was bool assign_new_ids=false
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
	Entity* spawn_prefab(const PrefabAsset* asset);
	Entity* spawn_entity();
	template<typename T>
	DeferredSpawnScope spawn_entity_class_deferred(T*& ptrOut) {
		auto ptr = spawn_entity_class_deferred_internal(T::StaticType);
		ptrOut = (T*)ptr;
		return DeferredSpawnScope(ptr);
	}
	void add_and_init_created_runtime_component(Component* c);
	BaseUpdater* get_entity(uint64_t handle) {
		return all_world_ents.find(handle);
	}
	string get_source_asset_name() const {
		return sourceAssetName;
	}
	bool is_editor_level() const {
		return b_is_editor_level;
	}
	const hash_map<BaseUpdater*>& get_all_objects() const {
		return all_world_ents;
	}
	// appends object to list that will be destroyed at end of frame, instead of instantly
	void queue_deferred_delete(BaseUpdater* e);

#ifdef EDITOR_BUILD
	Entity* editor_spawn_prefab_but_dont_set_spawned_by(const PrefabAsset* asset);
#endif
	void validate();
	Entity* find_initial_entity_by_name(const string& name) const;
	Component* find_first_component(const ClassTypeInfo* type) const;
private:
	void insert_unserialized_entities_into_level_internal(UnserializedSceneFile& scene, const SerializedSceneFile* reassign_ids, bool addSpawnNames);
	static void set_prefab_spawned(Entity& root, const PrefabAsset& asset, UnserializedSceneFile& file);
	Entity* spawn_prefab_shared(const PrefabAsset* asset, bool set_vars);
	string sourceAssetName;
	std::unordered_map<string, obj<Entity>> spawnNameToEntity;
	// all entities/components in the map
	hash_map<BaseUpdater*> all_world_ents;
	hash_set<Component> tick_list;
	// last instance id, incremented to add objs
	uint64_t last_id = 0;
	ScopedBooleanValue b_is_in_update_tick;
	ScopedBooleanValue b_is_in_level_startup;
	std::vector<Component*> wantsToAddToUpdate;
	hash_set<Component> wants_sync_update;
	std::unordered_set<uint64_t> deferred_delete_list;
	// is this an editor level
	bool b_is_editor_level =false;

	void insert_new_native_entity_into_hashmap_R(Entity* e);
	void initialize_new_entity_safe(Entity* e);
	Entity* spawn_entity_class_deferred_internal(const ClassTypeInfo& ti);
	uint64_t get_next_id_and_increment() {
		return ++last_id;	// prefix
	}

	friend class DeferredSpawnScope;	// for initialize_new_entity_safe
};