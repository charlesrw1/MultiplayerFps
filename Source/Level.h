#ifndef LEVEL_H
#define LEVEL_H
#include <vector>
#include "Model.h"

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

	CollisionData collision_data;
	RenderData render_data;
};

Level* LoadLevelFile(const char* level);
extern Level* TEMP_LEVEL;

#endif // !LEVEL_H
