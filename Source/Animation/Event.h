#pragma once

#include <string>
#include "Framework/StringName.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/Util.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ClassBase.h"
class Animator;

CLASS_H(AnimationEvent, ClassBase)
public:
	~AnimationEvent() {}

	virtual Color32 get_editor_color() { return COLOR_BLUE; }

	// if true, then event has a duration from [frame,frame+frame_duration]
	// on_event is called on enter and on_end is guaranteed to call on exit
	virtual bool is_duration_event() { return false; }
	virtual void on_event(Animator* animator) = 0;
	virtual void on_end(Animator* animator) {}

	int get_frame() const { return frame; }
	int get_duration() const { return frame_duration; }

	static const PropertyInfoList* get_props() {
		START_PROPS(AnimationEvent)
			REG_INT(frame,PROP_SERIALIZE,""),
			REG_INT(frame_duration, PROP_SERIALIZE,""),
			REG_INT(editor_layer, PROP_SERIALIZE, ""),
		END_PROPS(AnimationEvent)
	}
private:
	friend class AnimationEventGetter;
	friend class EditModelAnimations;
	int frame = 0;
	int frame_duration = 0;
	uint16_t editor_layer = 0;	// hacky, stores what layer it is in the editor for persistance
};