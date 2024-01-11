#ifndef MOVECOMMAND_H
#define MOVECOMMAND_H
#include "glm/glm.hpp"

enum Game_Command_Buttons
{
	BUTTON_SPRINT = 1,
	BUTTON_JUMP = 2,
	BUTTON_DUCK = 4,
	BUTTON_FIRE1 = 8,
	BUTTON_FIRE2 = 16,
	BUTTON_MISC1 = 32,
	BUTTON_MISC2 = 64,
	BUTTON_RELOAD = 128,
};

struct Move_Command
{
	int tick = 0;
	float forward_move = 0.f;
	float lateral_move = 0.f;
	float up_move = 0.f;
	int button_mask = 0;
	glm::vec3 view_angles = glm::vec3(0.f);

	bool first_sim = true;	// not replicated, used in player_updates

	static uint8_t quantize(float f) {
		return glm::clamp(int((f + 1.0) * 128.f),0,255);
	}
	static float unquantize(uint8_t c) {
		return (float(c) - 128.f) / 128.f;
	}
};

#endif // !MOVECOMMAND_H
