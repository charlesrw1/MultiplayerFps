#pragma once

#include <vector>
#include <string>
#include "Render/Model.h"
#include "Framework/BVH.h"
#include "Framework/Config.h"

#include "Framework/DictParser.h"
#include "Render/DrawPublic.h"

#include "Framework/Hashmap.h"

#include "Framework/InlineVec.h"
#include "Game/Entity.h"

#include "Framework/Hashset.h"
#include "Framework/ScopedBoolean.h"
#include "DeferredSpawnScope.h"

#include "LevelSerialization/SerializationAPI.h"



class PhysicsActor;
class WorldSettings;
class SceneAsset;
class PrefabAsset;
class Entity;
class GameMode;
class Level
{
public:

	// constructed in GameEngineLocal::on_map_change_callback
	// this starts the level, essentially

	Level();
	~Level();

	void create(SceneAsset* source, bool is_editor);


	void insert_unserialized_entities_into_level(UnserializedSceneFile& scene, const SerializedSceneFile* reassign_ids = nullptr); // was bool assign_new_ids=false

	// ends the level
	void close_level();

	void update_level();
	
	void add_to_update_list(BaseUpdater* b) {
		if (b_is_in_update_tick.get_value())
			wantsToAddToUpdate.push_back(b);
		else
			tick_list.insert(b);
	}

	void remove_from_update_list(BaseUpdater* b);

	template<typename T, size_t COUNT>
	bool find_all_entities_of_class(InlineVec<T*, COUNT>& out) {
		static_assert(std::is_base_of<Entity, T>::value, "find_all_entities_of_class needs T=Entity subclass");
		for (auto e : all_world_ents) {
			if (e->is_a<T>())
				out.push_back((T*)e);
		}
		return out.size() != 0;
	}

	template<typename T>
	T* find_first_of() {
		static_assert(std::is_base_of<Entity, T>::value, "find_first_of needs T=Entity subclass");
		for (auto e : all_world_ents) {
			if (e->is_a<T>())
				return (T*)e;
		}
		return nullptr;
	}

	// destroy an entity, calls desroy_internal(), deletes, removes from list
	void destroy_entity(Entity* e);
	
	void destroy_component(EntityComponent* e);

	Entity* spawn_entity_from_classtype(const ClassTypeInfo& ti);
	
	Entity* spawn_prefab(PrefabAsset* asset);
	
	template<typename T>
	T* spawn_entity_class() {
		static_assert(std::is_base_of<Entity, T>::value, "spawn_entity_class not derived from Entity");
		Entity* e = spawn_entity_from_classtype(T::StaticType);
		return (T*)e;
	}

	template<typename T>
	DeferredSpawnScope spawn_entity_class_deferred(T*& ptrOut) {
		auto ptr = spawn_entity_class_deferred_internal(T::StaticType);
		ptrOut = (T*)ptr;
		return DeferredSpawnScope(ptr);
	}

	void add_and_init_created_runtime_component(EntityComponent* c);


	BaseUpdater* get_entity(uint64_t handle) {
		return all_world_ents.find(handle);
	}

	SceneAsset* get_source_asset() const {
		return source_asset;
	}

	bool is_editor_level() const {
		return b_is_editor_level;
	}

	const hash_map<BaseUpdater*>& get_all_objects() const {
		return all_world_ents;
	}

	// appends object to list that will be destroyed at end of frame, instead of instantly
	void queue_deferred_delete(Entity* e);

private:
	SceneAsset* source_asset = nullptr;

	// all entities/components in the map
	hash_map<BaseUpdater*> all_world_ents;

	hash_set<BaseUpdater> tick_list;

	// last instance id, incremented to add objs
	uint64_t last_id = 0;

	ScopedBooleanValue b_is_in_update_tick;

	std::vector<BaseUpdater*> wantsToAddToUpdate;

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