#ifndef LEVEL_H
#define LEVEL_H
#include <vector>
#include <string>
#include "BVH.h"
#include "Model.h"
#include "Config.h"
#include "Dict.h"

#include "DrawPublic.h"

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

struct Static_Mesh_Object
{
	glm::mat4x3 transform;
	bool is_embedded_mesh = false;
	bool casts_shadows = true;
	Model* model = nullptr;
	renderobj_handle handle = -1;	// handle to render system
};

struct Object_Dict
{
	Dict dict;
	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	const char* name = "";	// referenced from dict
};

using std::vector;

class Model;
class Texture;
class Level
{
public:
	struct Entity_Spawn
	{
		std::string name;
		std::string classname;
		glm::vec3 position;
		glm::vec3 rotation;
		glm::vec3 scale;
		Dict spawnargs;

		int _ed_varying_index_for_statics = -1;
	};

	unique_ptr<Physics_Mesh> collision;	// merged collision of all level_meshes
	vector<Level_Light> lights;
	vector<Static_Mesh_Object> static_mesh_objs;
	Prefab_Model* level_prefab;
	vector<renderobj_handle> prefab_handles;
	vector<Entity_Spawn> espawns;
	vector<Object_Dict> objs;

	Texture* lightmap;

	
	std::string name;
	uint32_t skybox_cubemap;
};

Level* LoadLevelFile(const char* level);
void FreeLevel(Level* level);

#endif // !LEVEL_H
