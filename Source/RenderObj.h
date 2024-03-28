#pragma once

#include <cstdint>
#include <vector>
#include "glm/glm.hpp"

class Mesh;
class Animator;
class Material;
class Entity;
struct Render_Object_Proxy
{
	Render_Object_Proxy() {
		visible = true;
		shadow_caster = true;
		viewmodel_layer = false;
	}

	Mesh* mesh = nullptr;
	Animator* animator = nullptr;
	std::vector<Material*>* mats = nullptr;
	uint32_t param1 = 0;
	uint32_t param2 = 0;

	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;

	// for debugging only
	Entity* owner = nullptr;


	glm::mat4 transform = glm::mat4(1.f);
};

typedef int renderobj_handle;