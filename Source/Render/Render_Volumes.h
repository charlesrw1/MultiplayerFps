#pragma once
#include <glm/glm.hpp>
#include "Framework/Util.h"

// Irradiance and reflection probe volumes used for indirect lighting

class Texture;

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

	float fog_cubemap_min_dist = 0.0;
	float fog_cubemap_max_dist = 100.0;
	float fog_cubemap_max_mip = 0.7;
};