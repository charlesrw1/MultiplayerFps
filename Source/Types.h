#ifndef GAMETYPE_H
#define GAMETYPE_H
#include <SDL2/SDL.h>
#include "Net.h"
#include "Util.h"

const int DEFAULT_WIDTH = 1200;
const int DEFAULT_HEIGHT = 800;
const int MAX_GAME_ENTS = 256;


class FlyCamera
{
public:
	glm::vec3 position = glm::vec3(0);
	glm::vec3 front = glm::vec3(1, 0, 0);
	glm::vec3 up = glm::vec3(0, 1, 0);
	float move_speed = 0.1f;
	float yaw = 0, pitch = 0;

	void UpdateFromInput(const bool keys[], int mouse_dx, int mouse_dy, int scroll);
	void UpdateVectors();
	glm::mat4 GetViewMatrix() const;
};

struct View_Setup
{
	glm::vec3 origin, front;
	glm::mat4 view, proj, viewproj;
	float fov, near, far;
	int width, height;
};




#endif // !GAMETYPE_H
