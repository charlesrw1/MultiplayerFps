#include "Game/GameAnimEvents.h"
#include "Framework/AddClassToFactory.h"
#define ANIMEVENTIMPL(type_name) static AddClassToFactory<type_name, AnimationEvent> impl##type_name(AnimationEvent::get_factory(), #type_name); \
	 const TypeInfo& type_name::get_typeinfo() const  { \
		static TypeInfo ti = {#type_name, sizeof(type_name)}; \
		return ti; \
	} \
	AnimationEvent* type_name::create_copy() const { \
		return new type_name(*this); \
	}

ANIMEVENTIMPL(SoundAnimEvent);
ANIMEVENTIMPL(FootstepAnimEvent);