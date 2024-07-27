#pragma once
#include <glm/glm.hpp>

// Irradiance and reflection probe volumes used for indirect lighting

class Texture;
class IrradianceVolume;

// 3, 3d float rgb textures
struct Render_Irradiance_Volume
{
	glm::mat4 transform = glm::mat4(1);
	float resolution_xz = 1.0;
	float resolution_y = 1.0;
	int priority = 0;

	IrradianceVolume* irradiance = nullptr;
};

struct Render_Reflection_Volume
{
	glm::vec3 probe_position;
	glm::mat4 transform = glm::mat4(1.f);

	Texture* generated_cube = nullptr;
};

// sky reflection and ambient
struct Render_Skylight
{
	Texture* generated_cube = nullptr;
};

struct LevelIrradianceFieldAsset;
// Singleton asset
struct Render_Irradiance_Field_System
{
	LevelIrradianceFieldAsset* asset = nullptr;
	bool enable_dynamic_updates = false;
};