#pragma once

#include <cstdint>
#include <vector>
#include "glm/glm.hpp"
#include "Framework/Util.h"


class AnimatorInstance;
class MaterialInstance;
class Entity;
class Model;
class EntityComponent;
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
	AnimatorInstance* animator = nullptr;
	MaterialInstance* mat_override = nullptr;
	Color32 param1 = COLOR_WHITE;
	Color32 param2 = COLOR_WHITE;

	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;
	bool outline : 1;
	bool color_overlay : 1;
	bool dither : 1;
	bool opposite_dither : 1;

	// for debugging only (also editor uses this for picking)
	const EntityComponent* owner = nullptr;

	glm::mat4 transform = glm::mat4(1.f);
};
