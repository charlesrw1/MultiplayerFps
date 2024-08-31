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
		is_skybox = false;
	}

	Model* model = nullptr;
	AnimatorInstance* animator = nullptr;
	MaterialInstance* mat_override = nullptr;

	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;
	bool outline : 1;
	bool color_overlay : 1;
	bool dither : 1;
	bool opposite_dither : 1;
	bool is_skybox : 1;	// if true, then this is included in the global skylight reflection probe

	// for debugging only (also editor uses this for picking)
	const EntityComponent* owner = nullptr;

	glm::mat4 transform = glm::mat4(1.f);
};

class MeshBuilder;
struct MeshBuilder_Object
{
	MeshBuilder_Object() {
		visible = false;
		depth_tested = true;
	}

	const EntityComponent* owner = nullptr;
	MeshBuilder* meshbuilder = nullptr;
	bool visible : 1;
	bool depth_tested : 1;
	glm::mat4 transform = glm::mat4(1.f);
};