#pragma once
struct Frustum
{
	glm::vec4 top_plane;
	glm::vec4 bot_plane;
	glm::vec4 right_plane;
	glm::vec4 left_plane;

	glm::vec4 back_plane;
	bool is_ortho = false;
	float ortho_max_extent = 1.0; // for cascades, fixme
	glm::vec3 origin{};
	float fov = 0.0;
};

#include "Render/RenderScene.h"

struct View_Setup;
void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin = nullptr);
Frustum build_frustom_for_ortho(const glm::mat4& ortho_viewproj);
void build_frustum_for_cascade(Frustum& f, int index);
void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin);
Frustum build_frustum_for_light(RL_Internal& light);