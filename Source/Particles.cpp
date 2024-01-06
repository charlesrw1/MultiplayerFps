#include "Particles.h"
#include "Client.h"

void ParticleMgr::Init(ClientGame* cgame, Client* client) {
	particles.resize(MAX_PARTICLES);
	num_particles = 0;
	this->cgame = cgame;
	myclient = client;
}
void ParticleMgr::ClearAll()
{
	num_particles = 0;
}

void ParticleMgr::Update(float deltat) {
#if 0
	for (int i = 0; i < num_particles; i++) {
		ParticleFx& p = particles[i];
		if (p.life_end < client.time) {
			if (i != num_particles - 1) {
				p = particles.at(num_particles - 1);
				i--;// update the particle we just moved next iteration
			}
			num_particles--;
		}
		else if (p.life_start >= client.time) {
			p.velocity.y += p.gravity * deltat;
			p.origin += p.velocity * deltat;
			if(p.think)
				p.think(&p);
		}
	}
#endif
}

const static float tracer_speed = 10.f;
const static float tracer_life = 0.5f;
const static float tracer_width = 0.2f;
const static float tracer_length = 1.f;

void ParticleMgr::AddTracer(glm::vec3 org, glm::vec3 end) {
	ParticleFx* p = AllocNewParticle();
	glm::vec3 dir = glm::normalize(end - org);
	p->origin = org;
	p->velocity = dir * 10.f;
	//p->life_start = client.time;
	//p->life_end = client.time + tracer_life;
	p->gravity = 0.0;
	p->think = nullptr;
	p->billboarded = false;
	p->billboard_around_face_axis = true;
	p->dimensions.x = tracer_width;
	p->dimensions.y = tracer_length;
	p->sprite = nullptr;	// default to white for now
}

inline ParticleFx* ParticleMgr::AllocNewParticle() {
	if (num_particles >= particles.size())
		return nullptr;
	return &particles[num_particles++];
}
