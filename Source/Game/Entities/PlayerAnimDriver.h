#pragma once


#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Animation/Runtime/Animation.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/Reflection2.h"

NEWCLASS(CharacterGraphDriver, AnimatorInstance)
public:

	virtual void on_init() override;
	virtual void on_update(float dt) override;
	virtual void on_post_update() override;

	REFLECT();
	float flMovex=0.f;
	REFLECT();
	float flMovey=0.f;
	REFLECT();
	float flSpeed=0.f;
	REFLECT();
	bool bCrouch=false;
	REFLECT();
	bool bJumping=false;
	REFLECT();
	bool bFalling=false;
	REFLECT();
	bool bRunning=false;
	REFLECT();
	float flAimx=0.f;
	REFLECT();
	float flAimy=0.f;
	REFLECT();
	float flStopPercentage=0.f;
	REFLECT();
	bool bTurnInPlaceLeft=false;
	REFLECT();
	bool bTurnInPlaceRight=false;
	REFLECT();
	bool bLeftFootForwards=false;
	REFLECT();
	bool bRightFootForwards=false;
	REFLECT();
	int iSomeInteger = 0;
	REFLECT();
	glm::vec3 vLeftFootPosition;
	REFLECT();
	glm::quat qLeftFootRotation;
	REFLECT();
	glm::quat qRightHandRotation;

	bool left_foot_is_forward = false;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
