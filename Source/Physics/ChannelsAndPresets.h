#pragma once

#include "Framework/EnumDefReflection.h"
#include <span>

// physics layers
NEWENUM(PL, uint8_t){
	Default,
	StaticObject,  // static world objects (like static meshes)
	DynamicObject, // world objects being moved by code
	PhysicsObject, // world objects beign simulated by the physics engine
	Character,	   // any player or ai objects
	Visiblity,	   // any ray casts of the world

	UserStart, // can use any integer values starting here for whatever. cant use it in the editor, however. max of 32
};
using PhysicsLayer = PL;

// The engine's built-in default collision matrix, fed to
// PhysicsManager::set_physics_layer_collisions at startup.
std::span<const bool> get_default_layer_collision_matrix();