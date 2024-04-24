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

	handle<Parameter> flMovex;
	handle<Parameter> flMovey;
	handle<Parameter> flSpeed;
	handle<Parameter> bCrouch;
	handle<Parameter> bJumping;
	handle<Parameter> bFalling;
	handle<Parameter> bMoving;
	handle<Parameter> flAimx;
	handle<Parameter> flAimy;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
