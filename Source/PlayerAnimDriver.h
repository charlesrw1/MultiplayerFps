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


	glm::vec3 worldpos;
	glm::vec3 worldrot;

	glm::vec3 desireddir;
	glm::vec3 facedir;
	glm::vec3 velocity;
	glm::vec2 groundvelocity;
	glm::vec2 relmovedir;	//forwards/back etc.
	glm::vec2 relaccel;

	bool reset_accel = true;
	glm::quat player_rot_from_accel = glm::quat(1, 0, 0, 1);

	bool crouched;
	bool falling;
	bool injump;
	bool ismoving;

	bool use_rhik;
	bool use_lhik;
	glm::vec3 rhandtarget;
	glm::vec3 lhandtarget;
	bool use_headlook;
	glm::vec3 headlooktarget;

	glm::vec3 meshoffset;
};
