#pragma once

#include <cstdint>
#include <vector>
#include "glm/glm.hpp"
#include "Framework/Util.h"


class Animator;
class Material;
class Entity;
class Model;
struct Render_Object
{
	Render_Object() {
		visible = true;
		shadow_caster = true;
		viewmodel_layer = false;
		outline = false;
		color_overlay = false;
		dither = false;
		opposite_dither = false;
	}

	Model* model = nullptr;
	Animator* animator = nullptr;

	Color32 param1 = COLOR_WHITE;
	Color32 param2 = COLOR_WHITE;

	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;
	bool outline : 1;
	bool color_overlay : 1;
	bool dither : 1;
	bool opposite_dither : 1;

	// for debugging only
	Entity* owner = nullptr;


	glm::mat4 transform = glm::mat4(1.f);
};

struct Render_Decal
{
	glm::mat4 transform = glm::mat4(1.0);
	glm::vec3 dimensions = glm::vec3(1.0);
	Material* material=nullptr;
};

struct Render_Light
{
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec3 color;
	float conemin;
	float conemax;
	bool casts_shadow = false;

	int type = 0;

	// use for the main sun
	bool main_light_override = false;
};
