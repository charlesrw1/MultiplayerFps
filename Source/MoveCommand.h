#ifndef MOVECOMMAND_H
#define MOVECOMMAND_H
#include "glm/glm.hpp"

enum MoveCmdButtons
{
	CmdBtn_Sprint = 1,
	CmdBtn_Jump = 2,
	CmdBtn_Duck = 4,
	CmdBtn_PFire = 8,
	CmdBtn_SFire = 16,
	CmdBtn_Misc1 = 32,
	CmdBtn_Misc2 = 64,
	CmdBtn_Reload = 128,
};

struct MoveCommand
{
	int tick = 0;
	float forward_move = 0.f;
	float lateral_move = 0.f;
	float up_move = 0.f;
	int button_mask = 0;
	glm::vec3 view_angles = glm::vec3(0.f);

	static uint8_t quantize(float f) {
		return glm::clamp(int((f + 1.0) * 128.f),0,255);
	}
	static float unquantize(uint8_t c) {
		return (float(c) - 128.f) / 128.f;
	}
};

#endif // !MOVECOMMAND_H
