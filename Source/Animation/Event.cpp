#include "Event.h"

Factory<std::string, AnimationEvent>& AnimationEvent::get_factory()
{
	static Factory<std::string, AnimationEvent> inst;
	return inst;
}