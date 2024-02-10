#include "Particles.h"
#include "Client.h"
#include "Game_Engine.h"
#include "MathLib.h"
#include "Draw.h"
#include "glad/glad.h"

void Particle_Manager::init() {
	particles.resize(MAX_PARTICLES);
	particle_count = 0;

	sprites[BLOODSMALL] = FindOrLoadTexture("fx/bloodsmall.png");
	sprites[DIRT1] = FindOrLoadTexture("fx/dirt_01.png");
	sprites[SMOKE1] = FindOrLoadTexture("fx/smoke_01.png");

	rand = new Random(2345808);
}

void Particle_Manager::shutdown()
{
	delete rand;
}

void Particle_Manager::clear_all()
{
	particle_count = 0;
}

float Particle_Manager::get_time()
{
	return engine.time;
}

void Particle_Manager::tick(float deltat)
{
	for (int i = 0; i < particle_count; i++) {
		Particle& p = particles[i];
		if (p.life_end <= get_time()) {
			p = particles[particle_count - 1];
			particle_count--;
			i--;
			continue;
		}
		// fixme
		p.velocity.x *= 1 - (p.friction * deltat);
		p.velocity.z *= 1 - (p.friction * deltat);

		p.velocity.y += p.gravity * deltat;
		p.origin += p.velocity * (float)deltat;
		p.rotation += p.rotation_vel * deltat;
	}
}

void Particle_Manager::draw_particles()
{
	int sprite = -2;
	bool additive = false;

	draw.set_shader(draw.shade[draw.S_PARTICLE_BASIC]);
	draw.shader().set_mat4("ViewProj", draw.vs.viewproj);
	draw.shader().set_mat4("Model", glm::mat4(1));
	draw.shader().set_vec4("tint", glm::vec4(1.f));

	glm::vec3 side = cross(draw.vs.front, vec3(0.f, 1.f, 0.f));
	glm::vec3 up = cross(side, draw.vs.front);

	glEnable(GL_BLEND);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	verts.Begin();
	for (int i = 0; i < particle_count; i++)
	{
		Particle& p = particles[i];
		if (p.sprite != sprite || p.additive != additive) {
			if (verts.GetBaseVertex() > 0) {
				verts.End();
				verts.Draw(GL_TRIANGLES);
			}
			verts.Begin();

			if (p.additive != additive) {
				if (p.additive)
					glBlendFunc(GL_ONE, GL_ONE);
				else
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				additive = p.additive;
			}
			if (sprite != p.sprite) {
				sprite = p.sprite;
				draw.bind_texture(0, (p.sprite < 0) ? draw.white_texture : sprites[p.sprite]->gl_id);
			}
		}

		Color32 color = p.color_start;

		int base = verts.GetBaseVertex();
		glm::vec2 uvbase = glm::vec2(0);
		glm::vec2 uvsize = glm::vec2(1);

		


		MbVertex v[4];
		v[0].position = p.origin - p.size_x * side + p.size_y*up;
		v[3].position = p.origin + p.size_x * side + p.size_y * up;
		v[2].position = p.origin + p.size_x * side - p.size_y * up;
		v[1].position = p.origin - p.size_x * side - p.size_y * up;
		v[0].uv = uvbase;
		v[3].uv = glm::vec2(uvbase.x+uvsize.x, uvbase.y);
		v[2].uv = uvbase + uvsize;
		v[1].uv = glm::vec2(uvbase.x,uvbase.y+uvsize.y);
		for (int j = 0; j < 4; j++) v[j].color = color;
		for (int j = 0; j < 4; j++)verts.AddVertex(v[j]);
		verts.AddQuad(base, base + 1, base + 2, base + 3);
	}

	if (verts.GetBaseVertex() > 0) {
		verts.End();
		verts.Draw(GL_TRIANGLES);
	}

	glDisable(GL_BLEND);
	glDepthMask(GL_TRUE);
}

void Particle_Manager::add_dust_hit(glm::vec3 org)
{
	float life = 1.5f;
	Particle* p = new_particle();
	*p = Particle();
	p->origin = org;
	p->life_start = get_time();
	p->life_end = get_time() + life;
	p->size_x = 1.0;
	p->size_y = 0.75;


	p->sprite = SMOKE1;
	p->color_start = Color32{ 190, 145,70,255 };
	p->color_end = Color32{ 89,69,51,128 };

	p->rotation_vel = 0.7f;
	p->rotation = rand->RandF(0, TWOPI);
	p->gravity = 0.02f;	// positive gravity
}

Particle* Particle_Manager::new_particle()
{
	static Particle useless;
	if (particle_count >= MAX_PARTICLES) {
		sys_print("particles full\n");
		return &useless;
	}
	return &particles[particle_count++];
}

void Particle_Manager::add_blood_effect(glm::vec3 org, glm::vec3 normal)
{
	static Config_Var* blood_grav = cfg.get_var("particle/grav", "-10.0");

	glm::vec3 up = cross(normal, (normal.y>0.999)?vec3(1,0,0): vec3(0, 1, 0));
	glm::vec3 side = cross(normal, up);

	for (int i = 0; i < 12; i++) {

		float angle = rand->RandF(0, TWOPI);
		float r = rand->RandF(0, 1.f);
		glm::vec2 disk = { sqrt(r) * cos(angle),sqrt(r) * sin(angle) };

		vec3 dir = glm::normalize(normal + disk.x * up + disk.y * side);
		Particle* p = new_particle();
		p->sprite = BLOODSMALL;
		p->color_start = { 143,5,5,255 };
		p->gravity = blood_grav->real;
		p->life_start = get_time();
		p->life_end = get_time() + 1.5;
		p->friction = 0.5;
		p->origin = org + vec3(rand->RandF(-0.1, 0.1));
		float speed = rand->RandF(3, 5);
		p->velocity = dir * speed;

		float size = rand->RandF(0.075, 0.2);
		p->size_x = size;
		p->size_y = size;
	}
}

