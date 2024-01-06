#ifndef LEVEL_H
#define LEVEL_H
#include <vector>
#include "Model.h"
#include "BVH.h"
class Level
{
public:
	struct CollisionTri
	{
		int indicies[3];
		glm::vec3 face_normal;
		float plane_offset = 0.f;
		short surf_flags = 0;
		short surf_type = 0;
	};
	struct StaticInstance
	{
		int model_index;
		glm::mat4 transform;
	};
	struct RenderData
	{
		std::vector<StaticInstance> instances;
		std::vector<Model*> embedded_meshes;
	};
	struct CollisionData
	{
		std::vector<CollisionTri> collision_tris;
		std::vector<glm::vec3> vertex_list;
	};
	struct PlayerSpawn
	{
		glm::vec3 position;
		float angle = 0.f;
		short team = 0;
		short mode = 0;
	};
	struct Trigger
	{
		glm::mat4x3 inv_transform;	// aabb is then 1x1x1 cube
		glm::vec3 size = glm::vec3(1);
		short shape_type = 0;
		short type = 0;
		short param1 = 0;
	};
	BVH static_geo_bvh;
	CollisionData collision_data;
	RenderData render_data;
	std::vector<PlayerSpawn> spawns;
	std::vector<Trigger> triggers;
	std::string name;

	int ref_count = 0;	// to manage case of server/client accessing same level
};

Level* LoadLevelFile(const char* level);
void FreeLevel(const Level* level);

#endif // !LEVEL_H
