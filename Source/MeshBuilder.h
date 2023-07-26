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
	void Free();
	void Begin(int reserve_verts = 0);
	void End();

	int GetBaseVertex() const { return verticies.size(); }
	void AddVertex(MbVertex vertex) {
		verticies.push_back(vertex);
	}
	void AddTriangle(int v1, int v2, int v3) {
		indicies.push_back(v1);
		indicies.push_back(v2);
		indicies.push_back(v3);
	}
	void AddLine(int v1, int v2) {
		indicies.push_back(v1);
		indicies.push_back(v2);
	}
	void AddQuad(int v1, int v2, int v3, int v4) {
		AddTriangle(v1, v2, v3);
		AddTriangle(v1, v3, v4);
	}

	// Useful helpers
	void Push2dQuad(vec2 upper_left, vec2 size, vec2 upper_left_uv, vec2 uv_size, Color32 color);
	void PushLine(vec3 start, vec3 end, Color32 color);
	void Push2dQuad(vec2 upper_left, vec2 size);
	void PushSolidBox(vec3 box_min, vec3 box_max, Color32 color);
	void PushLineBox(vec3 box_min, vec3 box_max, Color32 color);
	void PushOrientedLineBox(vec3 box_min, vec3 box_max, glm::mat4 transform, Color32 color);

	void AddSphere(vec3 origin, float radius, int xsegments, int ysegments, Color32 color);
	// Must be vertical
	void AddCapsule(vec3 base, vec3 tip, float radius);
	void AddHemisphere(vec3 origin, vec3 orientation, float radius, int xsegments, int ysegments, Color32 color);

	// GL_TRIANGLES,etc.
	void Draw(uint32_t gl_type);
private:
	uint32_t VBO = 0, VAO = 0, EBO = 0;
	std::vector<MbVertex> verticies;
	std::vector<uint16_t> indicies;
};

#endif // !MESHBUILDER_H
