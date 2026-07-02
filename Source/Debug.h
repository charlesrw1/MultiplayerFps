#pragma once

#include "glm/glm.hpp"
#include "Framework/Util.h"
#include <string>

class Debug
{
public:
	static void add_line(glm::vec3 from, glm::vec3 to, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_sphere(glm::vec3 center, float rad, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_box(glm::vec3 center, glm::vec3 size, Color32 color, float lifetime, bool fixedupdate = true);
	// Projects `pos` to screen space every frame and prints `text` there in a monospace font,
	// no depth test (always on top), alpha faded out with distance from the camera.
	static void add_text(glm::vec3 pos, std::string text, Color32 color, float lifetime, bool fixedupdate = true);

	static void add_transformed_box(glm::mat4 transform, glm::vec3 size, Color32 color, float lifetime,
									bool fixedupdate = true);
	static void add_circle(glm::vec3 center, glm::vec3 normal, float radius, Color32 color,
						   float lifetime, bool fixedupdate = true, int segments = 32);
	static void add_cone(glm::vec3 apex, glm::vec3 direction, float length, float angle_degrees,
						 Color32 color, float lifetime, bool fixedupdate = true, int segments = 32);
};
