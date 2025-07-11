#pragma once
#include <glm/glm.hpp>

// Irradiance and reflection probe volumes used for indirect lighting

class Texture;

struct Render_Reflection_Volume
{
	glm::vec3 probe_position = glm::vec3(0.f);
	glm::vec3 boxmin = glm::vec3(0.f);
	glm::vec3 boxmax = glm::vec3(0.f);
	bool wants_update = false;
	Texture* generated_cube = nullptr;
	glm::vec3 ambientCube[6];
};

// sky reflection and ambient
struct Render_Skylight
{
	bool wants_update = false;
	const Texture* generated_cube = nullptr;
};