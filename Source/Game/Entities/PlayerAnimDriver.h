#pragma once


#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Animation/Runtime/Animation.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/Reflection2.h"


class CharacterGraphDriver : public AnimatorInstance
{
public:
	CLASS_BODY(CharacterGraphDriver);

	virtual void on_init() override;
	virtual void on_update(float dt) override;
	virtual void on_post_update() override;

	REF float flMovex=0.f;
	REF float flMovey=0.f;
	REF float flSpeed=0.f;
	REF bool bCrouch=false;
	REF bool bJumping=false;
	REF bool bFalling=false;
	REF bool bRunning=false;
	REF float flAimx=0.f;
	REF float flAimy=0.f;
	REF float flStopPercentage=0.f;
	REF bool bTurnInPlaceLeft=false;
	REF bool bTurnInPlaceRight=false;
	REF bool bLeftFootForwards=false;
	REF bool bRightFootForwards=false;
	REF int iSomeInteger = 0;
	REF glm::vec3 vLeftFootPosition;
	REF glm::quat qLeftFootRotation;
	REF glm::quat qRightHandRotation;

	bool left_foot_is_forward = false;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
