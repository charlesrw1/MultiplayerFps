#pragma once

#include <string>
#include "Framework/StringName.h"
#include "Framework/TypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/Util.h"
#include "Framework/ReflectionRegisterDefines.h"
#include "Framework/ClassBase.h"
class Animator;

CLASS_H(AnimationEvent, ClassBase)

	virtual Color32 get_editor_color() { return COLOR_BLUE; }

	// if true, then event has a duration from [frame,frame+frame_duration]
	// on_event is called on enter and on_end is guaranteed to call on exit
	virtual bool is_duration_event() { return false; }
	virtual void on_event(Animator* animator) = 0;
	virtual void on_end(Animator* animator) {}

	uint16_t get_frame() const { return frame; }
	uint16_t get_duration() const { return frame_duration; }

private:
	static const PropertyInfoList* get_props() {
		START_PROPS(AnimationEvent)
			REG_INT(frame,PROP_SERIALIZE,""),
			REG_INT(frame_duration, PROP_SERIALIZE,"")
		END_PROPS(AnimationEvent)
	}
	friend class AnimationEventGetter;

	uint16_t frame = 0;
	uint16_t frame_duration = 0;
};