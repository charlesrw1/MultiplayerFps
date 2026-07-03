#include "ChannelsAndPresets.h"

// Default layer collision matrix. Upper-triangular, row-major, diagonal included.
// Row i (Default..Character) lists whether layer i collides with the columns,
// where columns run in REVERSE enum order (Character first, own-layer last):
//
//	C	P	D	S	De     <- columns: Character, Physics, Dynamic, Static, Default
//	1,	1,	1,	1,	1,     // Default   collides with everything
//	1,	1,	1,	1,		   // Static    collides with everything
//	1,	0,	1,			   // Dynamic   collides with all except Physics
//	0,	1,				   // Physics   collides with Default, Static, Physics only
//	1,					   // Character collides with everything
//
// A 1 means the two layers collide/overlap; a 0 disables their pair entirely.
// See PhysicsManImpl::set_physics_layer_collisions for the exact index mapping.
static const bool DEFAULT_RESPONSE_ARRAY[] = {
//	C	P	D	S	De
	1,	1,	1,	1,	1,		// default
	1,	1,	1,	1,			// static
	1,	0,	1,				// dynamic
	0,	1,					// physics
	1,						// character
};

std::span<const bool> get_default_layer_collision_matrix() {
	return std::span<const bool>(DEFAULT_RESPONSE_ARRAY, std::size(DEFAULT_RESPONSE_ARRAY));
}
