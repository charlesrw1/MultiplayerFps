#include "Framework/MeshBuilder.h"
#include "glad/glad.h"
#include "MeshBuilderImpl.h"

static const int MIN_VERTEX_ARRAY_SIZE = 16;


const uint32_t MeshBuilderDD::TRIANGLES = GL_TRIANGLES;
const uint32_t MeshBuilderDD::LINES = GL_LINES;


void MeshBuilderDD::free()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}


void MeshBuilder::Begin(int reserve_verts)
{
	reserve_verts = glm::max(reserve_verts, MIN_VERTEX_ARRAY_SIZE);
	verticies.reserve(reserve_verts);
	verticies.clear();
	indicies.clear();
	wants_new_upload = true;
}

void MeshBuilderDD::init_from(MeshBuilder& mb)
{
	if (!mb.wants_new_upload)
		return;
	if (VAO == 0) {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &VBO);
		glGenBuffers(1, &EBO);

		glBindVertexArray(VAO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
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
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	}
	mb.wants_new_upload = false;
	glBufferData(GL_ARRAY_BUFFER, mb.verticies.size() * sizeof(MbVertex), mb.verticies.data(), GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, mb.indicies.size() * sizeof(uint32_t), mb.indicies.data(), GL_STREAM_DRAW);
	num_indicies = mb.indicies.size();
	glBindVertexArray(0);
}

void MeshBuilder::End()
{
}

void MeshBuilderDD::draw(uint32_t gl_type)
{
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glDrawElements((GLenum)gl_type, num_indicies, GL_UNSIGNED_INT, (void*)0);
}

void MeshBuilder::PushLine(vec3 start, vec3 end, Color32 color)
{
	int basev = GetBaseVertex();
	MbVertex v;
	v.position = start;
	v.color = color;
	AddVertex(v);
	v.position = end;
	AddVertex(v);
	AddLine(basev, basev + 1);

}
void MeshBuilder::Push2dQuad(vec2 upper_left, vec2 size, vec2 upper_left_uv, vec2 uv_size, Color32 color)
{
	int start = GetBaseVertex();

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
	for (int i = 0; i < 4; i++)
		AddVertex(corners[i]);
	AddTriangle(start + 2, start + 1, start + 0);
	AddTriangle(start + 3, start + 2, start + 0);


	//Push3(corners[0], corners[2], corners[1]);
	//Push3(corners[0], corners[3], corners[2]);
}
void MeshBuilder::Push2dQuad(vec2 upper, vec2 size)
{
	int start = GetBaseVertex();
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
	for (int i = 0; i < 4; i++)
		AddVertex(corners[i]);
	AddTriangle(start + 2, start + 1, start + 0);
	AddTriangle(start + 3, start + 2, start + 0);
	//Push3(corners[0], corners[2], corners[1]);
	//Push3(corners[0], corners[3], corners[2]);
}


void MeshBuilder::PushSolidBox(vec3 min, vec3 max, Color32 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
							vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	int start = GetBaseVertex();
	for (int i = 0; i < 8; i++)
		AddVertex(MbVertex(corners[i], color));
	AddQuad(start + 0, start + 1, start + 2, start + 3);//+y
	AddQuad(start + 4, start + 5, start + 6, start + 7);//-y
	AddQuad(start + 0, start + 1, start + 5, start + 4);//+x
	AddQuad(start + 3, start + 2, start + 6, start + 7);//-x
	AddQuad(start + 3, start + 0, start + 4, start + 7);//+z
	AddQuad(start + 2, start + 1, start + 5, start + 6);//-z
}

void MeshBuilder::PushLineBox(vec3 min, vec3 max, Color32 color)
{
	vec3 corners[8] = { max, vec3(max.x,max.y,min.z),vec3(min.x,max.y,min.z),vec3(min.x,max.y,max.z),	// top CCW
						vec3(max.x,min.y,max.z), vec3(max.x,min.y,min.z),min,vec3(min.x,min.y,max.z) };	// bottom
	int start = GetBaseVertex();
	for (int i = 0; i < 8; i++) {
		AddVertex(MbVertex(corners[i], color));
	}
	for (int i = 0; i < 4; i++) {
		AddLine(start + i, start + (i + 1) % 4);
		AddLine(start + (4+i), start + 4 + (i + 1) % 4);
	}
	// connecting
	for (int i = 0; i < 4; i++) {
		AddLine(start + i, start + i + 4);
	}
}
void MeshBuilder::PushOrientedLineBox(vec3 min, vec3 max, glm::mat4 transform, Color32 color)
{
	PushLineBox(min, max, color);
	int start = verticies.size() - 8;
	for (int i = start; i < verticies.size(); i++)
		verticies[i].position = transform * glm::vec4(verticies[i].position,1.0);
}
void MeshBuilder::AddLineSphere(vec3 origin, float radius, Color32 color)
{
	auto add_half_sphere = [&](glm::vec3 origin, float phi_mult, float radius, int segments) {
		for (int j = 0; j < 4; j++) {
			AddVertex(MbVertex(vec3(0, phi_mult*radius, 0) + origin, color));
			for (int i = 0; i < segments; i++)
			{
				auto phi = phi_mult * PI * double(i + 1) / double(segments * 2);

				auto theta = TWOPI * j / 4;
				auto x = std::sin(phi) * std::cos(theta);
				auto y = phi_mult*std::cos(phi);
				auto z = std::sin(phi) * std::sin(theta);
				AddVertex(MbVertex(vec3(x, y, z)*radius + origin, color));
				AddLine(verticies.size() - 2, verticies.size() - 1);
			}
		}
	};
	auto add_circle = [&](glm::vec3 center, float radius, int segments) {
		AddVertex(MbVertex(glm::vec3(center) + glm::vec3(radius, 0, 0), color));
		for (int i = 0; i < segments; i++) {
			auto theta = TWOPI * ((i + 1) / double(segments));
			auto x = std::cos(theta);
			auto y = 0.f;
			auto z = std::sin(theta);
			AddVertex(MbVertex(vec3(x, 0.f, z) * radius + center, color));
			AddLine(verticies.size() - 2, verticies.size() - 1);
		}
	};
	add_circle(origin, radius, 16);
	add_half_sphere(origin, 1.f, radius, 10);
	add_half_sphere(origin, -1.f, radius, 10);
}
void MeshBuilder::AddLineCapsule(vec3 origin, float radius, float realhalfheight, Color32 color)
{
	float halfheight = realhalfheight - (radius);

	auto add_half_sphere = [&](glm::vec3 origin, float phi_mult, float radius, int segments) {
		for (int j = 0; j < 4; j++) {
			AddVertex(MbVertex(vec3(0, phi_mult*radius, 0) + origin, color));
			for (int i = 0; i < segments; i++)
			{
				auto phi = phi_mult * PI * double(i + 1) / double(segments * 2);

				auto theta = TWOPI * j / 4;
				auto x = std::sin(phi) * std::cos(theta);
				auto y = phi_mult*std::cos(phi);
				auto z = std::sin(phi) * std::sin(theta);
				AddVertex(MbVertex(vec3(x, y, z)*radius + origin, color));
				AddLine(verticies.size() - 2, verticies.size() - 1);
			}
		}
	};

	auto add_circle = [&](glm::vec3 center, float radius, int segments) {
		AddVertex(MbVertex(glm::vec3(center) + glm::vec3(radius, 0, 0), color));
		for (int i = 0; i < segments; i++) {
			auto theta = TWOPI * ((i + 1) / double(segments));
			auto x = std::cos(theta);
			auto y = 0.f;
			auto z = std::sin(theta);
			AddVertex(MbVertex(vec3(x, 0.f, z) * radius + center, color));
			AddLine(verticies.size() - 2, verticies.size() - 1);
		}
	};
	add_circle(origin + glm::vec3(0, halfheight, 0), radius, 16);
	add_circle(origin + glm::vec3(0, -halfheight, 0), radius, 16);

	add_half_sphere(origin + glm::vec3(0, halfheight, 0), 1.f, radius, 10);
	add_half_sphere(origin + glm::vec3(0, -halfheight, 0), -1.f, radius, 10);
	//
	for (int j = 0; j < 4; j++) {
	
		auto theta = TWOPI * j / 4;
		auto x = std::cos(theta);
		auto y = 0.f;
		auto z = std::sin(theta);
		AddVertex(MbVertex(vec3(x, y, z)*radius + origin + glm::vec3(0,halfheight,0), color));
		AddVertex(MbVertex(vec3(x, y, z)*radius + origin - glm::vec3(0,halfheight,0), color));
		AddLine(verticies.size() - 2, verticies.size() - 1);
	}
}
void MeshBuilder::AddSphere(vec3 origin, float radius, int xsegments, int ysegments, Color32 color)
{
	int basev = GetBaseVertex();

	AddVertex(MbVertex(vec3(0,1,0), color));
	for (int i = 0; i < ysegments - 1; i++)
	{
		auto phi = PI * double(i + 1) / double(ysegments);
		for (int j = 0; j < xsegments; j++)
		{
			auto theta = TWOPI * double(j) / double(xsegments);
			auto x = std::sin(phi) * std::cos(theta);
			auto y = std::cos(phi);
			auto z = std::sin(phi) * std::sin(theta);
			AddVertex(MbVertex(vec3(x, y, z), color));
		}
	}
	AddVertex(MbVertex(vec3(0, -1, 0), color));

	for (int i = basev; i < verticies.size(); i++)
		verticies[i].position = verticies[i].position * radius + origin;
	
	// add top / bottom triangles
	int v0 = basev;
	int v1 = verticies.size() - 1;
	for (int i = 0; i < xsegments; ++i)
	{
		auto i0 = i + 1;
		auto i1 = (i + 1) % xsegments + 1;
		AddLine(v0, basev + i0);
		AddLine(basev + i0, basev+i1);
		AddLine(basev+i1,v0);
		i0 = i + xsegments * (ysegments - 2) + 1;
		i1 = (i + 1) % xsegments + xsegments * (ysegments - 2) + 1;
		AddLine(v1, basev + i0);
		AddLine(basev + i0, basev + i1);
		AddLine(basev + i1, v1);
	}

	for (int j = 0; j < ysegments - 2; j++)
	{
		auto j0 = j * xsegments+1;
		auto j1 = (j + 1) * xsegments+1;
		for (int i = 0; i < xsegments; i++)
		{
			auto i0 = j0 + i;
			auto i1 = j0 + (i + 1) % xsegments;
			auto i2 = j1 + (i + 1) % xsegments;
			auto i3 = j1 + i;
			AddLine(basev + i0, basev + i1);
			AddLine(basev + i1, basev + i2);
			AddLine(basev + i2, basev + i3);
			AddLine(basev + i3, basev + i0);
		}
	}


}
