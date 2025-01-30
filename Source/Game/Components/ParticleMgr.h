#pragma once
#include "Framework/Hashset.h"
struct View_Setup;
class ParticleComponent;
class ParticleMgr
{
public:
	ParticleMgr() : all_components(4) {}

	static ParticleMgr& get() {
		static ParticleMgr inst;
		return inst;
	}

	void draw(const View_Setup& setup);
	void register_this(ParticleComponent* c) {
		all_components.insert(c);
	}
	void unregister_this(ParticleComponent* c) {
		all_components.remove(c);
	}
private:
	hash_set<ParticleComponent> all_components;
};