#pragma once

#include <vector>
#include <string>
#include "Render/Model.h"
#include "Framework/BVH.h"
#include "Framework/Config.h"
#include "Framework/Dict.h"
#include "Framework/DictParser.h"
#include "Render/DrawPublic.h"

#include "Framework/Hashmap.h"

#include "Framework/InlineVec.h"
#include "Game/Entity.h"

#include "Framework/Hashset.h"

class PhysicsActor;
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
	
	hash_set<Entity> tick_list;

	// all entities in the map
	hash_map<Entity> all_world_ents;
	uint64_t last_id = 0;
	uint64_t local_player_id = 0;

	template<typename T, size_t COUNT>
	bool find_all_entities_of_class(InlineVec<T*, COUNT>& out) {
		static_assert(std::is_base_of<Entity, T>::value, "find_all_entities_of_class needs T=Entity subclass");
		for (auto e : all_world_ents) {
			if (e->is_a<T>())
				out.push_back((T*)e);
		}
		return out.size() != 0;
	}

	void insert_entity_into_hashmap(Entity* e) {
		ASSERT(e);
		ASSERT(!e->self_id.is_valid());
		e->self_id.handle = get_next_id_and_increment();
		ASSERT(all_world_ents.find(e->self_id.handle) == nullptr);

		all_world_ents.insert(e->self_id.handle, e);
	}
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
private:
	bool bIsEditorLevel=false;

	friend class LevelSerialization;
};

class SerializeEntityObjectContext;
class LevelSerialization
{
public:
	// Does not manage the memory!
	// Doesnt call init_entities_post_load() !
	static Level* unserialize_level(const std::string& file, bool for_editor = false);

	static Level* create_empty_level(const std::string& file, bool for_editor = false);

	static std::string serialize_level(Level* l);

	// doesnt call register/unregister on components, just creates everything
	static std::string serialize_entities_to_string(const std::vector<Entity*>& entities);
	static std::vector<Entity*> unserialize_entities_from_string(const std::string& str);
private:
	static void serialize_one_entity(Entity* e, DictWriter& out, SerializeEntityObjectContext& ctx);
	static bool unserialize_one_item(StringView tok, DictParser& in, SerializeEntityObjectContext& ctx);
};