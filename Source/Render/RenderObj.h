#pragma once

#include <cstdint>
#include <vector>
#include "glm/glm.hpp"
#include "Framework/Util.h"


class AnimatorInstance;
class MaterialInstance;
class Entity;
class Model;
class Component;
struct Render_Object {
	Render_Object() {
		visible = true;
		shadow_caster = true;
		viewmodel_layer = false;
		outline = false;
		color_overlay = false;
		dither = false;
		opposite_dither = false;
		is_skybox = false;
		lightmapped = false;
	}

	Model* model = nullptr;
	MaterialInstance* mat_override = nullptr;
	int animator_bone_ofs = -1;
	bool visible : 1;
	bool shadow_caster : 1;
	bool viewmodel_layer : 1;
	bool outline : 1;
	bool color_overlay : 1;
	bool dither : 1;
	bool opposite_dither : 1;
	bool is_skybox : 1;	// if true, then this is included in the global skylight reflection probe
	bool lightmapped : 1;
	// for debugging only (also editor uses this for picking)
	const Component* owner = nullptr;
	glm::mat4 transform = glm::mat4(1.f);
	glm::vec4 lightmap_coord = glm::vec4(0.f);
};

class MeshBuilder;
struct MeshBuilder_Object {
	MeshBuilder_Object() {
		visible = false;
		depth_tested = true;
		use_background_color = false;
	}
	const Component* owner = nullptr;
	MeshBuilder* meshbuilder = nullptr;
	MaterialInstance* material = nullptr;
	Color32 background_color = COLOR_BLACK;
	bool visible : 1;
	bool depth_tested : 1;
	bool use_background_color : 1;
	glm::mat4 transform = glm::mat4(1.f);
};

struct Particle_Object {
	Particle_Object() {

	}
	const Component* owner = nullptr;
	MeshBuilder* meshbuilder = nullptr;
	MaterialInstance* material = nullptr;
	glm::mat4 transform = glm::mat4(1.f);
};
class Texture;
struct Lightmap_Object {
	const Texture* lightmap_texture = nullptr;
};