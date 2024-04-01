#include "ParticlesLocal.h"

#include "Game_Engine.h"
#include "MathLib.h"
#include "DrawLocal.h"
#include "glad/glad.h"
#include "ParticlesLocal.h"

Particle_Manager particles;
Particle_Manager_Public* iparticle = &particles;

void pool_test()
{
	Pool_Allocator pool(8, 10);
	int* a = (int*)pool.allocate();
	*a = 5;
	int* b = (int*)pool.allocate();
	*b = 6;
	ASSERT(*a == 5);
	ASSERT(*b == 6);

	pool.free(a);
	int* c = (int*)pool.allocate();
	*c = 3;
	a = (int*)pool.allocate();
	*a = 4;
	ASSERT(*b == 6);
	ASSERT(*a == 4);
	ASSERT(*c == 3);
	pool.free(a);
	pool.free(b);
	pool.free(c);
}

void Particle_Manager::init()
{
	pool_test();
}


void Particle_Manager::update_systems(float dt)
{
}

void Particle_Manager::stop_playing_all_effects()
{
}

int Particle_Manager::find_effect(const char* name)
{
	return 0;
}

handle<Emitter_Object> Particle_Manager::create_emitter(const Emitter_Object& obj)
{
	handle<Emitter_Object> handle{ system_instances.make_new() };
	//System_Inst* system = (System_Inst*)particle_and_sys_alloc.allocate();
//	system = new(system)System_Inst();
	//system->obj = obj;
	//system_instances.get(handle.id) = system;
	return {};
}

void Particle_Manager::update_emitter(handle<Emitter_Object> handle, const Emitter_Object& obj)
{
	System_Inst* system = system_instances.get(handle.id);
	system->obj = obj;
}

void Particle_Manager::destroy_emitter(handle<Emitter_Object>& handle)
{
	System_Inst*& system = system_instances.get(handle.id);
	system->~System_Inst();
	
	//particle_and_sys_alloc.free(system);
	system = nullptr;

	system_instances.free(handle.id);
	handle.id = -1;
}

void Particle_Manager::play_effect(const char* name)
{
}