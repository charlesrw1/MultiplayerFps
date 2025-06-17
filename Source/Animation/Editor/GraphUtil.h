#pragma once
#include "Framework/Util.h"
#include <glm/glm.hpp>
#include "imgui.h"
#include "Base_node.h"

class GraphUtil
{
public:
	static Color32 to_color32(glm::vec4 v) {
		Color32 c;
		c.r = glm::clamp(v.r * 255.f, 0.f, 255.f);
		c.g = glm::clamp(v.g * 255.f, 0.f, 255.f);
		c.b = glm::clamp(v.b * 255.f, 0.f, 255.f);
		c.a = glm::clamp(v.a * 255.f, 0.f, 255.f);
		return c;
	}
	static glm::vec4 to_vec4(Color32 color) {
		return glm::vec4(color.r, color.g, color.b, color.a) / 255.f;
	}

	static Color32 mix_with(Color32 c, Color32 mix, float fac) {
		return to_color32(glm::mix(to_vec4(c), to_vec4(mix), fac));
	}

	static Color32 add_brightness(Color32 c, int brightness) {
		int r = c.r + brightness;
		int g = c.g + brightness;
		int b = c.b + brightness;
		if (r < 0) r = 0;
		if (r > 255) r = 255;
		if (g < 0)g = 0;
		if (g > 255)g = 255;
		if (b < 0)b = 0;
		if (b > 255)b = 255;
		c.r = r;
		c.g = g;
		c.b = b;
		return c;
	}

	static uint32_t color32_to_int(Color32 c) {
		return c.to_uint();
	}

	static ImVec2 to_imgui(glm::vec2 v) {
		return ImVec2(v.x, v.y);
	}
	static glm::vec2 to_glm(ImVec2 v) {
		return glm::vec2(v.x, v.y);
	}

	struct PinShapeColor {
		Color32 color;
		enum Type {
			Circle,
			Square,
			Triangle
		}type;
	};

	static PinShapeColor get_pin_for_value_type(const GraphPinType::Enum& type);

};