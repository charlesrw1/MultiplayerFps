#include "MeshBuilder.h"
#include "glad/glad.h"

static const int MIN_VERTEX_ARRAY_SIZE = 16;

void MeshBuilder::Begin(int reserve_verts)
{
	reserve_verts = glm::max(reserve_verts, MIN_VERTEX_ARRAY_SIZE);
	verticies.reserve(reserve_verts);
	verticies.clear();
}
void MeshBuilder::End()
{
	if (VAO == 0) {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		// POSITION (float*3)
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MbVertex), (void*)0);
		glEnableVertexAttribArray(0);
		// Color (uint8*4)
		glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(MbVertex), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		// UV (float * 2)
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MbVertex), (void*)(3 * sizeof(float) + sizeof(Color32)));
		glEnableVertexAttribArray(2);

	}
	else {
		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
	}
	glBufferData(GL_ARRAY_BUFFER, verticies.size() * sizeof(MbVertex), verticies.data(), GL_STREAM_DRAW);

	glBindVertexArray(0);
}

void MeshBuilder::Draw(uint32_t gl_type)
{
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glDrawArrays((GLenum)gl_type, 0, verticies.size());
}

void MeshBuilder::PushLine(vec3 start, vec3 end, Color32 color)
{
	MbVertex v;
	v.position = start;
	v.color = color;
	Push(v);
	v.position = end;
	Push(v);
}
void MeshBuilder::Push2dQuad(vec2 upper_left, vec2 size, vec2 upper_left_uv, vec2 uv_size, Color32 color)
{
	MbVertex corners[4];
	corners[0].position = vec3(upper_left, 0);
	corners[1].position = vec3(upper_left.x + size.x, upper_left.y, 0);
	corners[2].position = vec3(upper_left.x + size.x, upper_left.y + size.y, 0);
	corners[3].position = vec3(upper_left.x, upper_left.y + size.y, 0);
	corners[0].uv = vec2(upper_left_uv);
	corners[1].uv = vec2(upper_left_uv.x + uv_size.x, upper_left_uv.y);
	corners[2].uv = vec2(upper_left_uv + uv_size);
	corners[3].uv = vec2(upper_left_uv.x, upper_left_uv.y + uv_size.y);
	for (int i = 0; i < 4; i++)
		corners[i].color = color;
	Push3(corners[0], corners[2], corners[1]);
	Push3(corners[0], corners[3], corners[2]);
}
void MeshBuilder::Push2dQuad(vec2 upper, vec2 size)
{
	MbVertex corners[4];
	corners[0].position = vec3(upper, 0);
	corners[1].position = vec3(upper.x + size.x, upper.y, 0);
	corners[2].position = vec3(upper.x + size.x, upper.y + size.y, 0);
	corners[3].position = vec3(upper.x, upper.y + size.y, 0);
	corners[0].uv = vec2(0);
	corners[1].uv = vec2(1, 0);
	corners[2].uv = vec2(1, 1);
	corners[3].uv = vec2(0, 1);
	for (int i = 0; i < 4; i++)
		corners[i].color = COLOR_WHITE;
	Push3(corners[0], corners[2], corners[1]);
	Push3(corners[0], corners[3], corners[2]);
}

void MeshBuilder::Push3dQuad(vec3 v1, vec3 v2, vec3 v3, vec3 v4, Color32 color)
{
	Push3(MbVertex(v1, color), MbVertex(v2, color), MbVertex(v3, color));
	Push3(MbVertex(v1, color), MbVertex(v3, color), MbVertex(v4, color));
}

void MeshBuilder::PushSolidBox(vec3 min, vec3 max, Color32 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
							vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	Push3dQuad(corners[0], corners[1], corners[2], corners[3], color);// top
	Push3dQuad(corners[4], corners[5], corners[6], corners[7], color);// bottom
	Push3dQuad(corners[0], corners[1], corners[5], corners[4], color);//+X
	Push3dQuad(corners[3], corners[2], corners[6], corners[7], color);//-X
	Push3dQuad(corners[3], corners[0], corners[4], corners[7], color);//+Z
	Push3dQuad(corners[2], corners[1], corners[5], corners[6], color);//+Z
}
void MeshBuilder::PushSolidBox(vec3 min, vec3 max, Color32 color[6])
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
							vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	Push3dQuad(corners[0], corners[1], corners[2], corners[3], color[2]);// top
	Push3dQuad(corners[4], corners[5], corners[6], corners[7], color[3]);// bottom
	Push3dQuad(corners[0], corners[1], corners[5], corners[4], color[0]);//+X
	Push3dQuad(corners[3], corners[2], corners[6], corners[7], color[1]);//-X
	Push3dQuad(corners[3], corners[0], corners[4], corners[7], color[4]);//+Z
	Push3dQuad(corners[2], corners[1], corners[5], corners[6], color[5]);//+Z
}
void MeshBuilder::PushLineBox(vec3 min, vec3 max, Color32 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
						vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	for (int i = 0; i < 4; i++) {
		Push2(MbVertex(corners[i], color), MbVertex(corners[(i + 1) % 4], color));
		Push2(MbVertex(corners[4 + i], color), MbVertex(corners[4 + ((i + 1) % 4)], color));
	}
	// connecting
	for (int i = 0; i < 4; i++) {
		Push2(MbVertex(corners[i], color), MbVertex(corners[i + 4], color));
	}
}
void MeshBuilder::PushOrientedLineBox(vec3 min, vec3 max, glm::mat4 transform, Color32 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
						vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	for (int i = 0; i < 8; i++) {
		corners[i] = transform * glm::vec4(corners[i], 1.0);
	}

	for (int i = 0; i < 4; i++) {
		Push2(MbVertex(corners[i], color), MbVertex(corners[(i + 1) % 4], color));
		Push2(MbVertex(corners[4 + i], color), MbVertex(corners[4 + ((i + 1) % 4)], color));
	}
	// connecting
	for (int i = 0; i < 4; i++) {
		Push2(MbVertex(corners[i], color), MbVertex(corners[i + 4], color));
	}
}