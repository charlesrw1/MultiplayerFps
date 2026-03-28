#pragma once
#include "AllHeader.h"
class OrthoCamera
{
public:
	bool can_take_input() const;
	glm::vec3 position = glm::vec3(0.0);
	float width = 25.0;
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	glm::vec3 side = glm::vec3(0, 0, 1);

	MulticastDelegate<> on_ortho_set;

	float far = 200.0;
	void set_position_and_front(glm::vec3 position, glm::vec3 front);

	void scroll_callback(int amt);
	void update_from_input(float aspectratio);
	glm::mat4 get_view_matrix() const { return glm::lookAt(position, position + front, up); }
	glm::mat4 get_proj_matrix(float aspect_ratio) const;
	// used for ImGuizmo which doesnt like reverse Z
	glm::mat4 get_friendly_proj_matrix(float aspect_ratio) const;
};