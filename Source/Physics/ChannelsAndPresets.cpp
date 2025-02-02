#include "ChannelsAndPresets.h"


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