#pragma once
#include "Framework/Util.h"
#include "Framework/Handle.h"
#include "glm/glm.hpp"

class ParticleEmitter;
class ParticleManPublic
{
public:
	// ParticleEmitter manipulation
	virtual handle<ParticleEmitter> create_fx(const char* name) = 0;
	void set_fx_position(handle<ParticleEmitter> emitter, glm::vec3 position) {
		set_fx_control_point(emitter, 0, position);
	}
	void set_fx_target(handle<ParticleEmitter> emitter, glm::vec3 target) {
		set_fx_control_point(emitter, 1, target);
	}
	virtual void set_fx_velocity(handle<ParticleEmitter> emitter, glm::vec3 velocity) = 0;
	virtual void set_fx_rotation(handle<ParticleEmitter> emitter, glm::quat rotation) = 0;
	virtual void set_fx_playback(handle<ParticleEmitter> emitter, bool playing) = 0;
	virtual void free_fx(handle<ParticleEmitter>& handle) = 0;

	virtual void simulate_all_systems() = 0;
	virtual void render_systems() = 0;
private:
	virtual void set_fx_control_point(handle<ParticleEmitter> emitter, int index, glm::vec3 position) = 0;
};

extern ParticleManPublic* g_particles;