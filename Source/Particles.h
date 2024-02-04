#pragma once
#include "Texture.h"
#include "glm/glm.hpp"
#include <vector>
#include "Util.h"
#include "Shader.h"
#include "MeshBuilder.h"

class Particle
{
public:
	glm::vec3 origin=glm::vec3(0.f);

	float size_x = 0.5f;
	float size_y = 0.5f;

	glm::vec3 velocity = glm::vec3(0.f);
	float gravity = 0.f;
	float friction = 0.f;

	float rotation = 0.f;
	float rotation_vel = 0.f;

	float life_start = 0.f;
	float life_end = 0.f;

	bool billboard = true;
	//bool billboard_around_axis = true;
	glm::vec3 facing = glm::vec3(0.f);

	bool additive = false;
	Color32 color_start = COLOR_WHITE;
	Color32 color_end = COLOR_WHITE;

	short sprite = -1;
};

class Random;
class Particle_Manager
{
public:
	const static int MAX_PARTICLES = 512;

	void init();
	void shutdown();
	void clear_all();

	void tick(float dt);
	void draw_particles();

	void add_dust_hit(glm::vec3 org);
	void add_footstep(glm::vec3 org, glm::vec3 velocity);
	void add_blood_effect(glm::vec3 org, glm::vec3 normal);

	int particle_count = 0;
	std::vector<Particle> particles;
	Particle* new_particle();
	float get_time();

	Random* rand;

	// i just want this done...
	enum Sprites {
		BLOODSMALL, DIRT1, DIRT2, DIRT3,
		FIRE1, MUZZLE1, MUZZLE2, MUZZLE3,
		SMOKE1,SMOKE2,SMOKE3,SMOKE4,
		TRACE1,TRACE2,TRACE3,
		NUMSPRITES,
	};
	Texture* sprites[NUMSPRITES];

	MeshBuilder verts;
};