#pragma once

#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

struct pemitter_handle {
	int id = -1;
};

struct Emitter_Object
{
	glm::vec3 position;
	glm::quat rotation;
	int effectid = 0;

};


class Particle_Manager_Public
{
public:
	virtual void init() = 0;

	virtual void update_systems(float dt) = 0;

	virtual void stop_playing_all_effects() = 0;

	virtual int find_effect(const char* name) = 0;

	virtual handle<Emitter_Object> create_emitter(const Emitter_Object& obj) = 0;
	virtual void update_emitter(handle<Emitter_Object> handle, const Emitter_Object& obj) = 0;
	virtual void destroy_emitter(handle<Emitter_Object>& handle) = 0;

	virtual void play_effect(const char* name) = 0;

};

extern Particle_Manager_Public* iparticle;