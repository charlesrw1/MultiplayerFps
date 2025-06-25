#pragma once

#include <string>
#include "Framework/StringName.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/Util.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ClassBase.h"
#include "Framework/Reflection2.h"
class Animator;

class AnimationEvent : public ClassBase {
public:
	CLASS_BODY(AnimationEvent);
	~AnimationEvent() {}

	virtual Color32 get_editor_color() { return COLOR_BLUE; }

	// if true, then event has a duration from [frame,frame+frame_duration]
	// on_event is called on enter and on_end is guaranteed to call on exit
	virtual bool is_duration_event() const { return false; }
	virtual void on_event(Animator* animator) = 0;
	virtual void on_end(Animator* animator) {}

	int get_frame() const { return frame; }
	int get_duration() const { return frame_duration; }

private:
	friend class AnimationEventGetter;
	friend class EditModelAnimations;
	REF int frame = 0;
	REF int frame_duration = 0;
	REF int16_t editor_layer = 0;	// hacky, stores what layer it is in the editor for persistance
};