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
class AnimatorObject;
class AnimationEvent : public ClassBase {
public:
	CLASS_BODY(AnimationEvent);
	~AnimationEvent() {}
	virtual void on_event(AnimatorObject* animator) {}
	int get_frame() const { return frame; }
private:
	friend class AnimationEventGetter;
	friend class EditModelAnimations;
	int frame = 0;
};

class AnimDurationEvent : public ClassBase {
public:
	CLASS_BODY(AnimDurationEvent);

	// guaranted to fire on_end after on_start
	virtual void on_start(AnimatorObject* animator) const {}
	virtual void on_end(AnimatorObject* animator) const {}
	int get_end_frame() const { return frame_begin + frame_duration; }

	int frame_begin = 0;
	int frame_duration = 0;
};