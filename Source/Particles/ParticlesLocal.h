#pragma once
#include "Particles/ParticlesPublic.h"
// A piece of logic that operates on particles
class IParticleNode
{
public:
	// used by regular nodes
	virtual void simulate() = 0;
	virtual void spawn() = 0;

private:
	uint32_t rt_offset = 0;
};
