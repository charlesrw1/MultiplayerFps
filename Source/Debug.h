#pragma once

#include "glm/glm.hpp"
#include "Framework/Util.h"

class Debug
{
public:
	static void add_line(glm::vec3 from, glm::vec3 to, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_sphere(glm::vec3 center, float rad, Color32 color, float lifetime, bool fixedupdate = true);
	static void add_box(glm::vec3 center, glm::vec3 size, Color32 color, float lifetime, bool fixedupdate = true);
};

