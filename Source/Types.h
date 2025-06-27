#ifndef GAMETYPE_H
#define GAMETYPE_H
#include "Framework/Util.h"
#include <glm/glm.hpp>

class User_Camera
{
public:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	glm::vec3 orbit_target = glm::vec3(0.f);
	bool orbit_mode = false;

	void set_orbit_target(glm::vec3 target, float object_size);
	bool can_take_input() const;
	void scroll_callback(int amt);
	void update_from_input(int width, int height, float aratio, float fov);
	glm::mat4 get_view_matrix() const;
};


#endif // !GAMETYPE_H
