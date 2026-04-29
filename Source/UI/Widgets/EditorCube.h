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

	// mouse_pos is viewport-local (subtract vp_rect.get_pos() from window mouse pos before passing)
	void draw(RenderWindow& window, float dt, glm::ivec2 mouse_pos);

	struct CubeClickResult {
		bool hit = false;
		glm::vec3 eye_dir = {};
	};
	// Returns the eye direction to snap to, or hit=false if mouse isn't over the cube.
	// In ortho mode edges are tested first (45-degree views); faces always tested.
	// mouse_pos must be viewport-local (same coordinate space as ws_position).
	CubeClickResult test_click(glm::ivec2 mouse_pos) const;

	// Eye direction for each face (direction FROM orbit target TO camera)
	static constexpr glm::vec3 face_eye_dirs[6] = {
		{ 0, 0, 1}, { 0, 0,-1},
		{-1, 0, 0}, { 1, 0, 0},
		{ 0, 1, 0}, { 0,-1, 0},
	};

	glm::ivec2 ws_position = {10, 10};
	glm::ivec2 ws_sz = {30, 30};

	std::vector<const Texture*> textures;
	guiCubeTransform rotation;
	bool is_ortho = false;  // set each frame by caller; enables edge clicking and highlights

	glm::quat desired_rotation;
	glm::quat current_rotation;
	float alpha = 0.0;

private:
	struct HoverResult { int face = -1; int edge = -1; };
	HoverResult compute_hover(glm::ivec2 mouse_pos) const;

	glm::vec2 face_screen_centers[6] = {};
	bool      face_front[6]          = {};
	glm::vec2 corner_screen_pts[8]   = {};
	bool      edge_visible[12]       = {};
	bool      draw_valid             = false;
};
