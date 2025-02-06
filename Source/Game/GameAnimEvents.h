#pragma once
#include "Animation/Event.h"

CLASS_H(SoundAnimEvent, AnimationEvent)
public:
	
	SoundAnimEvent() {
		
	}
	bool is_duration_event() const {
		return true;
	}
	virtual void on_event(Animator* a) override {
		sys_print(Debug,"--- sound! %d\n", some_number);
	}
	static const PropertyInfoList* get_props() {
		START_PROPS(SoundAnimEvent)
			REG_INT(some_number, PROP_DEFAULT, "")
		END_PROPS(SoundAnimEvent)
	}

	int some_number = 0;
};

CLASS_H(FootstepAnimEvent, AnimationEvent)
	
	virtual void on_event(Animator* a) override {
		sys_print(Debug,"--- footstep %d\n");
	}
	static const PropertyInfoList* get_props() {
		return nullptr;
	}
};