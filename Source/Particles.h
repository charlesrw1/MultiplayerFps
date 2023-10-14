#pragma once
#include "Texture.h"
#include "glm/glm.hpp"
#include <vector>


struct ParticleFx
{
	glm::vec3 origin=glm::vec3(0.f);
	glm::vec3 velocity = glm::vec3(0.f);
	glm::vec3 facing = glm::vec3(0.f);
	glm::vec2 dimensions = glm::vec2(0.f);
	float gravity = 0.f;
	float life_start = 0.f;
	float life_end = 0.f;
	bool billboarded = false;
	bool billboard_around_face_axis = true;
	Texture* sprite = nullptr;

	void(*think)(ParticleFx* fx) = nullptr;
};

class ClientGame;
class Client;
class ParticleMgr
{
public:
	const static int MAX_PARTICLES = 512;
	void Init(ClientGame* cgame, Client* client);
	void ClearAll();

	void Update(float deltat);

	void AddTracer(glm::vec3 org, glm::vec3 end);

	void AddSpark(glm::vec3 org, glm::vec3 dir, float spd);
	void MakeSparkEffect(glm::vec3 org, glm::vec3 n, int num);

	void MakeBloodEffect(glm::vec3 org);

	void AddBloodHit();
	void AddGunshotHit();
	void AddMuzzleFlash();
	void AddGrenadeExplosion();

	void AddTrailEffect();

	ParticleFx* AllocNewParticle();

	int num_particles = 0;
	std::vector<ParticleFx> particles;
	
	ClientGame* cgame = nullptr;
	Client* myclient = nullptr;
};