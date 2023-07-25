#ifndef MESHBUILDER_H
#define MESHBUILDER_H
#include <vector>
#include "glm/glm.hpp"
#include "Util.h"
// FIXME:
using glm::vec3;
using glm::vec2;
struct MbVertex
{
	MbVertex(vec3 position, Color32 color) : position(position), color(color) {}
	MbVertex() {}
	vec3 position;
	Color32 color;
	vec2 uv;
};

class MeshBuilder
{
public:
	void Begin(int reserve_verts = 0);
	void End();

	void Push(MbVertex vertex);
	void Push2(MbVertex v1, MbVertex v2);
	void Push3(MbVertex v1, MbVertex v2, MbVertex v3);

	// Useful helpers
	void Push2dQuad(vec2 upper_left, vec2 size, vec2 upper_left_uv, vec2 uv_size, Color32 color);


	void PushLine(vec3 start, vec3 end, Color32 color);
	void Push2dQuad(vec2 upper_left, vec2 size);
	void Push3dQuad(vec3 v1, vec3 v2, vec3 v3, vec3 v4, Color32 color);
	void PushSolidBox(vec3 box_min, vec3 box_max, Color32 color);
	void PushSolidBox(vec3 box_min, vec3 box_max, Color32 color[6]);
	void PushLineBox(vec3 box_min, vec3 box_max, Color32 color);
	void PushOrientedLineBox(vec3 box_min, vec3 box_max, glm::mat4 transform, Color32 color);
	void AddSphere(vec3 origin, float radius, int xsegments, int ysegments, Color32 color);

	// GL_TRIANGLES,etc.
	void Draw(uint32_t gl_type);
private:
	uint32_t VBO = 0, VAO = 0;
	std::vector<MbVertex> verticies;
};
inline void MeshBuilder::Push(MbVertex vert)
{
	verticies.push_back(vert);
}
inline void MeshBuilder::Push2(MbVertex v1, MbVertex v2) {
	Push(v1);
	Push(v2);
}
inline void MeshBuilder::Push3(MbVertex v1, MbVertex v2, MbVertex v3) {
	Push(v1);
	Push(v2);
	Push(v3);
}

#endif // !MESHBUILDER_H
