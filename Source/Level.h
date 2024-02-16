#ifndef LEVEL_H
#define LEVEL_H
#include <vector>
#include "BVH.h"
#include "Model.h"
#include "EnvProbe.h"

struct Level_Light
{
	enum { POINT, SPOT, DIRECTIONAL };
	int type;
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
	struct Box_Cubemap
	{
		glm::vec3 position;
		glm::vec3 boxmin;
		glm::vec3 boxmax;
		int priority = 0;
		EnvCubemap cube;
		bool has_probe_pos = false;
	};

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
		std::vector<std::vector<std::string>> key_values;
	};

	Physics_Mesh collision;	// union of all level_meshes collision data

	std::vector<Box_Cubemap> cubemaps;
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
