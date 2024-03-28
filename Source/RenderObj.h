#pragma once

#include <cstdint>
#include <vector>
#include "glm/glm.hpp"

class Mesh;
class Animator;
class Material;
class Entity;
class Model;
struct Render_Object_Proxy
{
	Render_Object_Proxy() {
		visible = true;
		shadow_caster = true;
		viewmodel_layer = false;
		outline = false;
		color_overlay = false;
	}

	Mesh* mesh = nullptr;
	Animator* animator = nullptr;
	std::vector<Material*>* mats = nullptr;
	Color32 param1 = COLOR_WHITE;
	Color32 param2 = COLOR_WHITE;

	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;
	bool outline : 1;
	bool color_overlay : 1;

	// for debugging only
	Entity* owner = nullptr;


	glm::mat4 transform = glm::mat4(1.f);
};

typedef int renderobj_handle;