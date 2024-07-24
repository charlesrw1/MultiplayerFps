#pragma once

#include <vector>
#include <string>
#include "Model.h"
#include "Framework/BVH.h"
#include "Framework/Config.h"
#include "Framework/Dict.h"
#include "Framework/DictParser.h"
#include "DrawPublic.h"

#include "Framework/Hashmap.h"

#include "Framework/InlineVec.h"
#include "Entity.h"

struct Texture;
struct StaticEnv
{
	std::string skyname;
	const Texture* sky_texture = nullptr;
};


class PhysicsActor;
CLASS_H(Level, IAsset)
public:
	Level();
	~Level() {
		free_level();
	}

	// only call once after initialization
	void init_entities_post_load();

	// will call spawn funcs
	void insert_unserialized_entities_into_level(std::vector<Entity*> ents, bool assign_new_ids = false);

	bool open_from_file(const std::string& path);
	bool is_editor_level() const {
		return bIsEditorLevel;
	}
	
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

	bool bIsEditorLevel=false;

	unique_ptr<Physics_Mesh> scollision;	// merged collision of all level_meshes
	// static lights/meshes/physics
	vector<handle<Render_Light>> slights;
	vector<handle<Render_Object>> smeshes;

	StaticEnv senv;
	int main_sun = -1;

	bool has_main_sun() const { return main_sun != -1; }
	uint64_t get_next_id_and_increment() {
		return ++last_id;	// prefix
	}
private:

	void free_level();
};

class SerializeEntityObjectContext;
class LevelSerialization
{
public:
	// doesnt call register/unregister on components, just creates everything
	static std::string serialize_entities_to_string(const std::vector<Entity*>& entities);
	static std::vector<Entity*> unserialize_entities_from_string(const std::string& str);
private:
	static void serialize_one_entity(Entity* e, DictWriter& out, SerializeEntityObjectContext& ctx);
	static bool unserialize_one_item(StringView tok, DictParser& in, SerializeEntityObjectContext& ctx);
};

// Does not manage the memory!
class LevelLoadManager
{
public:
	static LevelLoadManager& get() {
		static LevelLoadManager inst;
		return inst;
	}
	Level* load_level(const std::string& file, bool for_editor = false);
	void free_level(Level* level);
};