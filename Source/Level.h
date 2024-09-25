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

class PhysicsActor;
class WorldSettings;
CLASS_H(Level, IAsset)
public:
	Level();
	~Level();

	// only call once after initialization
	void init_entities_post_load();

	// will call spawn funcs
	void insert_unserialized_entities_into_level(std::vector<Entity*> ents, bool assign_new_ids = false);

	bool is_editor_level() const {
		return bIsEditorLevel;
	}
	
	hash_set<BaseUpdater> tick_list;

	void add_to_update_list(BaseUpdater* b) {
		if (b_is_in_update_tick.get_value())
			wantsToAddToUpdate.push_back(b);
		else
			tick_list.insert(b);
	}
	void remove_from_update_list(BaseUpdater* b);


	void update_level() {
		{
			BooleanScope scope(b_is_in_update_tick);

			for (auto updater : tick_list)
				updater->update();
		}

		for (auto want : wantsToAddToUpdate) {
			if (want)
				tick_list.insert(want);
		}
		wantsToAddToUpdate.clear();
	}


	// all entities in the map
	hash_map<Entity*> all_world_ents;
	uint64_t last_id = 0;
	uint64_t local_player_id = 0;
	const WorldSettings* world_settings = nullptr;	// the world settings entity, shouldnt be nullptr

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

	void insert_entity_into_hashmap(Entity* e);
	void remove_entity_handle(uint64_t handle) {
#ifdef _DEBUG
		auto ent = all_world_ents.find(handle);
		if (!ent) {
			sys_print("??? remove_entity_handle: entity does not exist in hashmap, double delete?\n");
		}
#endif // _DEBUG
		all_world_ents.remove(handle);
	}

	Entity* get_local_player() const {
		return all_world_ents.find(local_player_id);
	}
	Entity* get_entity(uint64_t handle) {
		return all_world_ents.find(handle);
	}


	uint64_t get_next_id_and_increment() {
		return ++last_id;	// prefix
	}


	// IAsset overrides
	void sweep_references() const override {}
	bool load_asset(ClassBase*& user) override;
	void post_load(ClassBase*) override {}
	void uninstall() override {}
	void move_construct(IAsset*) override {}

	void set_editor_level(bool isEditorLevel) {
		bIsEditorLevel = isEditorLevel;
	}
private:
	ScopedBooleanValue b_in_in_level_initialize;
	ScopedBooleanValue b_is_in_update_tick;

	std::vector<BaseUpdater*> wantsToAddToUpdate;
	std::vector<Entity*> deferredSpawnList;

	bool bIsEditorLevel=false;

	friend class LevelSerialization;
};

class FileWriter;
class BinaryReader;
class SerializeEntityObjectContext;
class LevelSerialization
{
public:
	// Does not manage the memory!
	// Doesnt call init_entities_post_load() !
	static Level* unserialize_level(const std::string& file, bool for_editor = false);

	static Level* create_empty_level(const std::string& file, bool for_editor = false);

	static std::string serialize_level(Level* l);

	static void serialize_level_binary(Level* l, FileWriter& writer);
	static void unserialize_level_binary(Level* l, BinaryReader& writer);


	// doesnt call register/unregister on components, just creates everything
	static std::string serialize_entities_to_string(const std::vector<Entity*>& entities);
	static std::vector<Entity*> unserialize_entities_from_string(const std::string& str);
private:

	static void serialize_one_entity_binary(Entity* e, FileWriter& out, SerializeEntityObjectContext& ctx);

	static bool unserialize_one_item_binary(BinaryReader& in, SerializeEntityObjectContext& ctx);

	static void serialize_one_entity(Entity* e, DictWriter& out, SerializeEntityObjectContext& ctx);
	static bool unserialize_one_item(StringView tok, DictParser& in, SerializeEntityObjectContext& ctx);
};