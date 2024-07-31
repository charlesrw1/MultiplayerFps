#pragma once
#include "Framework/Util.h"
struct RenderFog
{
	float fog_density = 1.0;
	float height = 0.0;
	float fog_height_falloff = 1.0;
	Color32 inscattering_color{};

	bool use_scene_directional_light = true;
	float directional_exponent = 1.0;
	Color32 directional_color{};
};