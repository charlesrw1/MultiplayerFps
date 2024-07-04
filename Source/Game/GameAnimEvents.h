#pragma once
#include "Animation/Event.h"

class SoundAnimEvent : public AnimationEvent
{
public:
	CLASS_HEADER();

	virtual void on_event(Animator* a) override {
		sys_print("--- sound! %d\n", some_number);
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(SoundAnimEvent)
			REG_INT(some_number, PROP_DEFAULT, "")
		END_PROPS(SoundAnimEvent)
	}

	int some_number = 0;
};

class FootstepAnimEvent : public AnimationEvent
{
public:
	CLASS_HEADER();

	virtual void on_event(Animator* a) override {
		sys_print("--- footstep %d\n");
	}
	static const PropertyInfoList* get_props() {
		return nullptr;
	}
};