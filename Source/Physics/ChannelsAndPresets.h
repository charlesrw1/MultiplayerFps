#pragma once

#include "Framework/EnumDefReflection.h"

// physics layers
NEWENUM(PL, uint8_t)
{
	Default,
	StaticObject,	// static world objects (like static meshes)
	DynamicObject,	// world objects being moved by code
	PhysicsObject,	// world objects beign simulated by the physics engine
	Character,		// any player or ai objects
	Visiblity,		// any ray casts of the world
	
	UserStart,	// can use any integer values starting here for whatever. cant use it in the editor, however. max of 32
};
using PhysicsLayer = PL;

uint32_t get_collision_mask_for_physics_layer(PhysicsLayer layer);