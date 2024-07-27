#pragma once
#include <glm/glm.hpp>

// use for directional lights
struct Render_Sun
{
	glm::vec3 color = glm::vec3(2.f);
	glm::vec3 direction = glm::vec3(0, -1, 0);

	bool enabled = true;
	bool cast_shadows = true;

	// various settings
	bool fit_to_scene = true;
	float log_lin_lerp_factor = 0.5;
	float max_shadow_dist = 80.f;
	float epsilon = 0.008f;
	float z_dist_scaling = 1.f;
};
