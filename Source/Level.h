#ifndef LEVEL_H
#define LEVEL_H
#include <vector>
#include <string>
#include "BVH.h"
#include "Model.h"
#include "Config.h"
#include "Dict.h"

enum {
	LIGHT_DIRECTIONAL, 
	LIGHT_POINT, 
	LIGHT_SPOT,
};

struct Level_Light
{
	int type = 0;
	glm::vec3 position;
	glm::vec3 direction;
	glm::vec3 color; // rgb*intensity
	float spot_angle;
};

class Model;
class Texture;
class Level
{
public:
	struct StaticInstance
	{
		int model_index;
		glm::mat4 transform;
		bool collision_only = false;
	};
	struct Entity_Spawn
	{
		std::string name;
		std::string classname;
		glm::vec3 position;
		glm::vec3 rotation;
		glm::vec3 scale;
		Dict spawnargs;
	};

	Physics_Mesh collision;	// union of all level_meshes collision data
	std::vector<Level_Light> lights;
	std::vector<Model*> linked_meshes;	// custom embedded meshes that are linked to an entity like doors, not included in static_meshes
	std::vector<StaticInstance> instances;	// instances, index into static_meshes
	std::vector<Model*> static_meshes;
	Texture* lightmap;

	std::vector<Entity_Spawn> espawns;
	
	std::string name;
	uint32_t skybox_cubemap;
};

Level* LoadLevelFile(const char* level);
void FreeLevel(const Level* level);

#endif // !LEVEL_H
