#pragma once
#include "Animation/Event.h"

class SoundAnimEvent : public AnimationEvent
{
public:
	ANIMEVENT_HEADER(SoundAnimEvent);

	virtual void on_event(Animator* a) override {
		sys_print("--- sound! %d\n", some_number);
	}
	virtual PropertyInfoList* get_props() override {
		START_PROPS(SoundAnimEvent)
			REG_INT(some_number, PROP_DEFAULT, "")
		END_PROPS(SoundAnimEvent)
	}

	int some_number = 0;
};

class FootstepAnimEvent : public AnimationEvent
{
public:
	ANIMEVENT_HEADER(FootstepAnimEvent);

	virtual void on_event(Animator* a) override {
		sys_print("--- footstep %d\n");
	}
	virtual PropertyInfoList* get_props() override {
		return nullptr;
	}
};