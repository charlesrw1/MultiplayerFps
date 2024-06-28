#pragma once

#include <string>
#include "Framework/StringName.h"
#include "Framework/TypeInfo.h"
#include "Framework/ReflectionProp.h"
#include "Framework/Factory.h"
#include "Framework/Util.h"
#include "Framework/ReflectionRegisterDefines.h"
class Animator;

#define ANIMEVENT_HEADER(type_name)  virtual const TypeInfo& get_typeinfo() const override; \
	virtual AnimationEvent* create_copy() const;
class AnimationEvent
{
public:
	static Factory<std::string, AnimationEvent>& get_factory();

	virtual Color32 get_editor_color() { return COLOR_BLUE; }
	virtual const TypeInfo& get_typeinfo() const = 0;
	// override to add any serialized properties
	virtual PropertyInfoList* get_props() = 0;

	// if true, then event has a duration from [frame,frame+frame_duration]
	// on_event is called on enter and on_end is guaranteed to call on exit
	virtual bool is_duration_event() { return false; }
	virtual void on_event(Animator* animator) = 0;
	virtual void on_end(Animator* animator) {}

	uint16_t get_frame() const { return frame; }
	uint16_t get_duration() const { return frame_duration; }

	virtual AnimationEvent* create_copy() const = 0;

private:
	static PropertyInfoList* get_event_props() {
		START_PROPS(AnimationEvent)
			REG_INT(frame,PROP_SERIALIZE,""),
			REG_INT(frame_duration, PROP_SERIALIZE,"")
		END_PROPS(AnimationEvent)
	}
	friend class AnimationEventGetter;

	uint16_t frame = 0;
	uint16_t frame_duration = 0;
};

struct AnimationEventGetter
{
	static void get(std::vector<PropertyListInstancePair>& props, AnimationEvent* node) {
		props.push_back({ node->get_event_props(), node });
		props.push_back({ node->get_props(), node });
	}
};
