#pragma once
#include "glm/glm.hpp"
#include "Util.h"

struct Decal_Params
{
	glm::mat4 transform;
	Material* material;
};

class Material;
class DecalPublic
{
public:
	virtual void init() = 0;

	// caller handles lifetime
	virtual handle<Decal_Params> spawn_decal(const Decal_Params& params) = 0;
	virtual void update_decal(handle<Decal_Params> handle, const Decal_Params& params) = 0;
	virtual void remove_decal(handle<Decal_Params>& handle);

	// system handles lifetime
	// if entity and bone are not -1, then decal automatically updates. Is removed once entityhandle_t becomes invalid
	virtual void spawn_oneshot_decal(Material* material, float lifetime = 0.0, int entity = -1, int bone = -1) = 0;
};

// lights
// decals
// particles
// models
// sounds