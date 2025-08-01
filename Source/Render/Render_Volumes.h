#pragma once
#include <glm/glm.hpp>
#include "Framework/Util.h"

// Irradiance and reflection probe volumes used for indirect lighting

class Texture;

struct Render_Reflection_Volume
{
	glm::vec3 probe_position = glm::vec3(0.f);
	glm::vec3 boxmin = glm::vec3(0.f);
	glm::vec3 boxmax = glm::vec3(0.f);
	bool wants_update = false;
	Texture* generated_cube = nullptr;
	// manually set by baker, for reasons...
	//glm::vec3 ambientCube[6];
	int probe_ofs = -1;

};

// sky reflection and ambient
// and stuffing the fog settings here too
struct Render_Skylight
{
	bool wants_update = false;
	const Texture* generated_cube = nullptr;

	bool fog_enabled = false;
	bool fog_use_skylight_cubemap = false;
	Color32 fog_color{};
	float height_fog_exp = 1.0;
	float height_fog_start = 0.0;
	float height_fog_density = 1.0;
};