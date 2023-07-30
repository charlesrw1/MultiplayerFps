#ifndef MOVECOMMAND_H
#define MOVECOMMAND_H
#include "glm/glm.hpp"

struct MoveCommand
{
	int tick = 0;
	float forward_move = 0.f;
	float lateral_move = 0.f;
	float up_move = 0.f;
	int button_mask = 0;
	glm::vec3 view_angles = glm::vec3(0.f);
};

#endif // !MOVECOMMAND_H
