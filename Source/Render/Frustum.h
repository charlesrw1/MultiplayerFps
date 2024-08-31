#pragma once
struct Frustum
{
	glm::vec4 top_plane;
	glm::vec4 bot_plane;
	glm::vec4 right_plane;
	glm::vec4 left_plane;
};

struct View_Setup;
void build_a_frustum_for_perspective(Frustum& f, const View_Setup& view, glm::vec3* arrow_origin = nullptr);
