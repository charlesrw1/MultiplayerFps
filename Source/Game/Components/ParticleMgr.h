#pragma once
#include "Framework/Hashset.h"
struct View_Setup;
class ParticleComponent;
class TrailComponent;
class BeamComponent;
class ParticleMgr
{
public:
	ParticleMgr() : all_components(4) {}
	static ParticleMgr* inst;

	void draw(const View_Setup& setup);
	void register_this(ParticleComponent* c) {
		all_components.insert(c);
	}
	void unregister_this(ParticleComponent* c) {
		all_components.remove(c);
	}
	void register_this(TrailComponent* t) {
		all_trails.insert(t);
	}
	void unregister_this(TrailComponent* t) {
		all_trails.remove(t);
	}
	void register_this(BeamComponent* t) {
		all_beams.insert(t);
	}
	void unregister_this(BeamComponent* t) {
		all_beams.remove(t);
	}
private:
	hash_set<ParticleComponent> all_components;
	hash_set<TrailComponent> all_trails;
	hash_set<BeamComponent> all_beams;


};