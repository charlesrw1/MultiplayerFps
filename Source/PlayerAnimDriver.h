#pragma once

#include "Animation/IGraphDriver.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Animation/Runtime/Animation.h"

class CharacterGraphDriver : public IAnimationGraphDriver
{
public:

	virtual void on_init() override;
	virtual void on_update(float dt) override;
	virtual void pre_ik_update(Pose& pose, float dt) override;
	virtual void post_ik_update() override;

	int8_t flMovex=-1;
	int8_t flMovey=-1;
	int8_t flSpeed=-1;
	int8_t bCrouch=-1;
	int8_t bJumping=-1;
	int8_t bFalling=-1;
	int8_t bRunning=-1;
	int8_t flAimx=-1;
	int8_t flAimy=-1;
	int8_t flStopPercentage=-1;
	int8_t bTurnInPlaceLeft=-1;
	int8_t bTurnInPlaceRight=-1;
	int8_t bLeftFootForwards=-1;
	int8_t bRightFootForwards=-1;

	bool left_foot_is_forward = false;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
