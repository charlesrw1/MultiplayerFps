#pragma once
#include "Texture.h"
#include "glm/glm.hpp"
#include <vector>
#include "Framework/Util.h"
#include "Shader.h"
#include "Framework/MeshBuilder.h"

#include "Framework/MathLib.h"

#include "Framework/FreeList.h"
#include "ParticlesPublic.h"

#include <unordered_map>


struct Trail_Data
{
	glm::vec3 positions[8];
};

struct System_Inst;
class Particle
{
public:
	handle<System_Inst> owner;
	glm::vec3 origin=glm::vec3(0.f);	// localspace
	glm::vec3 velocity = glm::vec3(0.f);
	float life_left = 0.f;
	uint8_t size_start = 0;
	uint8_t size_end = 0;
	uint8_t rotation_start = 0;
	uint8_t rotation_end = 0;
	uint8_t effect_idx = 0;
};
enum class effect_type
{
	PARTICLE,	// indivual particles
	TRAIL,		// each particle gets its own trail
	RIBBON,		// particles are linked together with a ribbon/trail
};

enum class emit_type
{
	CONTINUOUS,
	BURST,
};

// uniformly distributed or constant
template<typename T>
struct Value_Range
{
	Value_Range(T val) : min(val), max(val) {}
	Value_Range(T min, T max) : min(min), max(max) {}

	void get(Random& r) {
		return min + r.RandF(0.0, 1.0) * (max - min);
	}
	T min = T();
	T max = T();
};

struct Effect_Spawn_Def
{
	Effect_Spawn_Def() :
		velocity(glm::vec3(0.f)),
		lifetime(1.f),
		rotation(0.f),
		angular_velocity(0.f),
		gravity(1.f),
		elasticity(0.f),
		color_start(glm::vec4(1.f)),
		color_end(glm::vec4(1.f)),
		size_start(glm::vec2(1.f)),
		size_end(glm::vec2(1.f))
	{
	
	}

	effect_type type = effect_type::PARTICLE;
	emit_type emit = emit_type::CONTINUOUS;

	Value_Range<glm::vec3> velocity;
	Value_Range<float> lifetime;
	Value_Range<float> rotation;
	Value_Range<float> angular_velocity;
	Value_Range<float> gravity;
	Value_Range<float> elasticity;
	Value_Range<glm::vec4> color_start;
	Value_Range<glm::vec4> color_end;
	Value_Range<glm::vec2> size_start;
	Value_Range<glm::vec2> size_end;
	Material* mat = nullptr;
};

struct Particle_System_Def
{
	static const int MAX_EFFECT_DEF = 16;
	int num_effect_defs = 0;
	Effect_Spawn_Def defs[MAX_EFFECT_DEF];
};


struct System_Inst
{
	System_Inst() {
		memset(particles, 0, sizeof particles);
	}
	~System_Inst() {}
	Emitter_Object obj;
	glm::mat4 transform = glm::mat4(1.f);

	int particle_count = 0;
	Particle** particles[4];	// 4*128 particles
};

class Random;
class Particle_Manager : public Particle_Manager_Public
{
public:
	// Inherited via Particle_Manager_Public
	virtual void init() override;
	virtual void update_systems(float dt) override;
	virtual void stop_playing_all_effects() override;
	virtual int find_effect(const char* name) override;
	virtual handle<Emitter_Object> create_emitter(const Emitter_Object& obj) override;
	virtual void update_emitter(handle<Emitter_Object> handle, const Emitter_Object& obj) override;
	virtual void destroy_emitter(handle<Emitter_Object>& handle) override;
	virtual void play_effect(const char* name) override;

//	Pool_Allocator particle_and_sys_alloc;
//	Pool_Allocator trail_alloc;
	Free_List<System_Inst*> system_instances;
	std::unordered_map<std::string, Particle_System_Def> systems;
};