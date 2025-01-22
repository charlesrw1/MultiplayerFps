#include "ChannelsAndPresets.h"

ENUM_START(PL)
	STRINGIFY_EUNM(PL::Default,			0),
	STRINGIFY_EUNM(PL::StaticObject,	1),
	STRINGIFY_EUNM(PL::DynamicObject,	2),
	STRINGIFY_EUNM(PL::PhysicsObject,	3),
	STRINGIFY_EUNM(PL::Character,		4),
	STRINGIFY_EUNM(PL::Visiblity,		5),
ENUM_IMPL(PL);

#define GET_MASK(x) (1ul << uint32_t(PL::x))

uint32_t get_collision_mask_for_physics_layer(PhysicsLayer layer)
{
	switch (layer)
	{
	case PL::PhysicsObject:
		return GET_MASK(PhysicsObject) | GET_MASK(Default) | GET_MASK(StaticObject);
	default:
		return UINT32_MAX;
	}
}