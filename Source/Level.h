#ifndef LEVEL_H
#define LEVEL_H
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

class MapEntity
{
public:
	MapEntity() = default;
	MapEntity(const Dict& d) {
		dict = d;
		type = dict.get_string("_schema_name");
	}
	MapEntity(Dict&& d) {
		dict = std::move(d);
		type = dict.get_string("_schema_name");
	}
	StringName type;
	Dict dict;
};

class MapLoadFile
{
public:
	const std::string& get_name() const { return mapname; }
	
	bool parse(const std::string name);

	// for map editing
	void write_to_disk(const std::string name);
	void clear_all() { spawners.clear(); }
	void add_spawner(const Dict& d) { 
		spawners.push_back(d); 
	}

	string mapname;
	std::vector<MapEntity> spawners;
};

class WorldLocator
{
public:
	StringName hashed_name;
	const Dict* dict = nullptr;
};

class PhysicsActor;
class Level
{
public:
	Level();
	~Level() {
		free_level();
	}

	bool open_from_file(const std::string& path);
	bool is_editor_level() const {
		return bIsEditorLevel;
	}
	bool is_game_level() const {
		return !bIsEditorLevel;
	}

	std::string name;
	

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
	uint64_t get_next_id_and_increment() {
		return ++last_id;	// prefix
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
	vector<PhysicsActor*> sphysics;

	// use for world locator points like player spawns
	MapEntity* find_by_schema_name(StringName name, MapEntity* start = nullptr);

	StaticEnv senv;
	int main_sun = -1;

	bool has_main_sun() const { return main_sun != -1; }

	MapLoadFile loadfile;
private:
	void free_level();
};
#endif // !LEVEL_H
