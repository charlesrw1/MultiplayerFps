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

enum {
	LIGHT_DIRECTIONAL, 
	LIGHT_POINT, 
	LIGHT_SPOT,
};

struct StaticLight
{
	int type = 0;
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 color; // rgb*intensity
	float spot_angle;
};

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
	Level() = default;
	Level(const Level& o) = delete;
	~Level() {
		free_level();
	}

	bool open_from_file(const std::string& path);

	std::string name;
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
