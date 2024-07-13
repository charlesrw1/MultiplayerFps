#pragma once


#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "Animation/Runtime/Animation.h"
#include "Framework/ReflectionRegisterDefines.h"

CLASS_H(CharacterGraphDriver, AnimatorInstance)


	virtual void on_init() override;
	virtual void on_update(float dt) override;
	virtual void on_post_update() override;


	static const PropertyInfoList* get_props() {
		START_PROPS(CharacterGraphDriver)
			REG_FLOAT(flMovex,PROP_DEFAULT,""),
			REG_FLOAT(flMovey, PROP_DEFAULT, ""),
			REG_FLOAT(flSpeed, PROP_DEFAULT, ""),
			REG_FLOAT(flAimx, PROP_DEFAULT, ""),
			REG_FLOAT(flAimy, PROP_DEFAULT, ""),
			REG_FLOAT(flStopPercentage, PROP_DEFAULT, ""),

			REG_BOOL(bCrouch, PROP_DEFAULT,""),
			REG_BOOL(bJumping, PROP_DEFAULT, ""),
			REG_BOOL(bFalling, PROP_DEFAULT, ""),

			REG_BOOL(bRunning, PROP_DEFAULT, ""),
			REG_BOOL(bTurnInPlaceLeft, PROP_DEFAULT, ""),
			REG_BOOL(bTurnInPlaceRight, PROP_DEFAULT, ""),
			REG_BOOL(bLeftFootForwards, PROP_DEFAULT, ""),
			REG_BOOL(bRightFootForwards, PROP_DEFAULT, ""),

			REG_VEC3(vLeftFootPosition,PROP_DEFAULT),
			REG_QUAT(qLeftFootRotation, PROP_DEFAULT),
			REG_INT(iSomeInteger, PROP_DEFAULT, ""),
			REG_QUAT(qRightHandRotation, PROP_DEFAULT)
		END_PROPS(CharacterGraphDriver)
	}

	float flMovex=0.f;
	float flMovey=0.f;
	float flSpeed=0.f;
	bool bCrouch=false;
	bool bJumping=false;
	bool bFalling=false;
	bool bRunning=false;
	float flAimx=0.f;
	float flAimy=0.f;
	float flStopPercentage=0.f;
	bool bTurnInPlaceLeft=false;
	bool bTurnInPlaceRight=false;
	bool bLeftFootForwards=false;
	bool bRightFootForwards=false;
	int iSomeInteger = 0;

	glm::vec3 vLeftFootPosition;
	glm::quat qLeftFootRotation;

	glm::quat qRightHandRotation;

	bool left_foot_is_forward = false;

	bool crouched = false;
	bool falling=false;
	bool injump=false;
	bool ismoving=false;

	glm::vec3 meshoffset = glm::vec3(0.f);
};
