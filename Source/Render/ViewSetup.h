#pragma once

struct View_Setup
{
	View_Setup() {}
	View_Setup(glm::vec3 origin, glm::vec3 front, float fov, float near, float far, int width, int height);
	View_Setup(glm::mat4 viewMat, float fov, float near, float far, int width, int height);

	// dont use this, just for some things that dont play nice with infinite Z
	glm::mat4 make_opengl_perspective_with_near_far() const;

	bool is_ortho = false;
	glm::vec3 origin;
	glm::vec3 front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};