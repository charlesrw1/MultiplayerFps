#pragma once
#include <glm/glm.hpp>

// spot and point lights
class Texture;
struct Render_Light
{
	glm::vec3 position = glm::vec3(0, 0, 0);

	glm::vec3 normal = glm::vec3(1, 0, 0);	// for spot lights

	glm::vec3 color = glm::vec3(1.f);

	float radius = 20.0;	// radius in meters of light
	float conemin = 45.f;
	float conemax = 45.f;
	float shadow_bias = 0.01;

	bool casts_shadow = false;
	bool is_spotlight = false;

	// light cookie texture
	// can use to project a shape
	// should be square
	Texture* projected_texture = nullptr;
};
