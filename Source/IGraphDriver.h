#pragma once

class Pose;
class Animator;
class IAnimationGraphDriver
{
public:
	Animator* owner = nullptr;
	virtual void on_init() = 0;
	virtual void on_update(float dt) = 0;
	virtual void pre_ik_update(Pose& pose, float dt) = 0;
	virtual void post_ik_update() = 0;
};