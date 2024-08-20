#pragma once
#include "Framework/Util.h"
#include "Framework/Handle.h"
#include "glm/glm.hpp"
#include "Assets/IAsset.h"
#include "Framework/ReflectionMacros.h"
#include "Framework/ArrayReflection.h"

#include <glm/gtc/quaternion.hpp>

class ParticleEmitter;
CLASS_H(ParticleFXAsset, IAsset)
public:
	ParticleFXAsset();
	~ParticleFXAsset();
	static const PropertyInfoList* get_props();


	void post_load(ClassBase*) {}
	bool load_asset(ClassBase*&) {
		return false;
	}
	void uninstall() {}
	void move_construct(IAsset*) {}
	void sweep_references() const {}
private:
	std::vector<std::unique_ptr<ParticleEmitter>> emitters;
	friend class ParticleSystemLocal;
};

struct ParticleFx
{
	glm::vec3 pos{};
	glm::quat rot{};
	ParticleFXAsset* asset = nullptr;
	bool playing = false;
};

class ParticleSystemPublic
{
public:

	virtual ParticleFXAsset* load_particle_fx(const std::string& file) = 0;

	// ParticleEmitter manipulation
	virtual handle<ParticleFx> create_fx(const ParticleFx& fx) = 0;
	virtual void update_fx(handle<ParticleEmitter> emitter, const ParticleFx& fx) = 0;
	virtual void free_fx(handle<ParticleEmitter>& handle) = 0;

	virtual void simulate_all_systems() = 0;

	virtual void init() = 0;
};

extern ParticleSystemPublic* g_particles;