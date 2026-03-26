#pragma once
#include "UI/BaseGUI.h"
#include <vector>
#include <glm/glm.hpp>
class Texture;
class RenderWindow;
#include "Animation/Runtime/Easing.h"
class guiCubeTransform
{
public:
	glm::mat3 get_matrix(float dt) {
		if (alpha >= 0.f) {
			glm::mat3 m = glm::mat3_cast(get_interpolated());
			alpha += dt * 2.0;
			if (alpha >= 1.0)
				alpha = -1;
			return m;
		}
		return cur_matrix;
	}
	void set_current(glm::mat3 matrix) { cur_matrix = matrix; }
	void begin_interpolate() {
		alpha = 0.0f;
		last_matrix = cur_matrix;
	}

private:
	glm::quat get_interpolated() const {
		float a = evaluate_easing(Easing::CubicEaseInOut, alpha);
		return glm::slerp(glm::quat_cast(last_matrix), glm::quat_cast(cur_matrix), a);
	}

	float alpha = -1;
	glm::mat3 last_matrix;
	glm::mat3 cur_matrix;
};

class guiEditorCube
{
public:
	guiEditorCube();
	void draw(RenderWindow& window, float dt);
	glm::ivec2 ws_position = {10, 10};
	glm::ivec2 ws_sz = {30, 30};

	std::vector<const Texture*> textures;
	guiCubeTransform rotation;
	bool is_ortho = false;

	glm::quat desired_rotation;
	glm::quat current_rotation;
	float alpha = 0.0;
};
