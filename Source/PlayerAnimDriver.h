#pragma once

#include "IGraphDriver.h"

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Animation.h"
#include "AnimationUtil.h"

class CharacterGraphDriver : public IAnimationGraphDriver
{
public:

	virtual void on_init() override;
	virtual void on_update(float dt) override;
	virtual void pre_ik_update(Pose& pose, float dt) override;
	virtual void post_ik_update() override;

	handle<AG_ControlParam> flMovex;
	handle<AG_ControlParam> flMovey;
	handle<AG_ControlParam> flSpeed;
	handle<AG_ControlParam> bCrouch;
	handle<AG_ControlParam> bJumping;
	handle<AG_ControlParam> bFalling;
	handle<AG_ControlParam> bMoving;
	handle<AG_ControlParam> flAimx;
	handle<AG_ControlParam> flAimy;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
