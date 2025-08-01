#pragma once
#include <glm/glm.hpp>
class MaterialInstance;
struct Render_Decal
{
	Render_Decal() {
		visible = false;
	}
	glm::mat4 transform = glm::mat4(1.0);
	glm::vec2 uv_scale = glm::vec2(1.f);
	const MaterialInstance* material = nullptr;
	int ordering = 0;
	bool visible : 1;
};
