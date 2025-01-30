#ifndef GAMETYPE_H
#define GAMETYPE_H
#include "Framework/Util.h"
#include <glm/glm.hpp>

class User_Camera
{
public:
	glm::vec3 orbit_target = glm::vec3(0.f);
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	bool orbit_mode = false;

	void scroll_callback(int amt);
	void update_from_input(const bool keys[], int mouse_dx, int mouse_dy, glm::mat4 invproj);
	glm::mat4 get_view_matrix() const;
};


#endif // !GAMETYPE_H
