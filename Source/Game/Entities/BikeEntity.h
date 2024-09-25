#pragma once
#include "Game/Entity.h"
#include "Game/BasePlayer.h"

class CharacterController;
CLASS_H(BikeEntity,Entity)
public:
	BikeEntity();
	void update() override;
	void start() override;

	float turn_strength = 0.0;	// -1,1 r,l
	float forward_strength = 0.0;	// 0,1
	glm::vec3 bike_direction{};
private:
	std::unique_ptr<CharacterController> ccontrol = nullptr;
	float current_turn = 0.0; // -1,1
	float forward_speed = 0.0;
	float current_roll = 0.0;
};