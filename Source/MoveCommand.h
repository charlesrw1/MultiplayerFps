#ifndef MOVECOMMAND_H
#define MOVECOMMAND_H
#include "glm/glm.hpp"

enum Game_Command_Buttons
{
	BUTTON_JUMP = (1),
	BUTTON_DUCK = (1<<2),
	BUTTON_FIRE1 = (1<<3),
	BUTTON_FIRE2 = (1<<4),
	BUTTON_RELOAD = (1<<5),
	BUTTON_USE = (1<<6),

	BUTTON_ITEM1 = (1 << 8),
	BUTTON_ITEM2 = (1 << 9),
	BUTTON_ITEM3 = (1 << 10),
	BUTTON_ITEM4 = (1 << 11),
	BUTTON_ITEM5 = (1 << 12),
	BUTTON_ITEM_NEXT = (1 << 12),
	BUTTON_ITEM_PREV = (1 << 13),
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
