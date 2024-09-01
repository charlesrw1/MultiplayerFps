#ifndef MESHBUILDER_H
#define MESHBUILDER_H
#include <vector>
#include "glm/glm.hpp"
#include "Framework/Util.h"


struct MbVertex
{
	MbVertex(glm::vec3 position, Color32 color) : position(position), color(color) {}
	MbVertex() {}
	glm::vec3 position = glm::vec3(0);
	Color32 color = COLOR_WHITE;
	glm::vec2 uv = glm::vec2(0);
};

using glm::vec3;
using glm::vec2;
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
	// line sphere
	void AddSphere(vec3 origin, float radius, int xsegments, int ysegments, Color32 color);

	// GL_TRIANGLES,etc.
	void Draw(uint32_t gl_type);
	// can also use these instead
	static const uint32_t TRIANGLES;
	static const uint32_t LINES;


	const std::vector<MbVertex>& get_v() { return verticies; }
	const std::vector<uint32_t>& get_i() { return indicies; }

	// hack for now
	void get_data_to_render_with(uint32_t& vao, uint32_t& vbo, int& count, int& type);
private:
	uint32_t VBO = 0, VAO = 0, EBO = 0;
	std::vector<MbVertex> verticies;
	std::vector<uint32_t> indicies;
};

#endif // !MESHBUILDER_H
