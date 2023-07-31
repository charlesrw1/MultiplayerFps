#ifndef MOVECOMMAND_H
#define MOVECOMMAND_H
#include "glm/glm.hpp"

enum MoveCmdButtons
{
	CmdBtn_Sprint = 1,
	CmdBtn_Jump = 2,
	CmdBtn_Duck = 4,
	CmdBtn_Misc1 = 8,
	CmdBtn_Misc2 = 16,
};

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
