#include "ParticleComponent.h"
#include "Render/RenderObj.h"
#include "GameEnginePublic.h"
#include "ParticleMgr.h"

CLASS_IMPL(ParticleComponent);

void ParticleComponent::start()
{
	Particle_Object o{};
	o.meshbuilder = &builder;
	o.material = particle_mat.get();
	o.owner = this;
	o.transform = (inherit_transform) ? get_ws_transform() : glm::mat4(1.f);

	obj = idraw->get_scene()->register_particle_obj(o);
	r.state = wang_hash((uint32_t)get_instance_id());
	ParticleMgr::get().register_this(this);

	is_playing = true;
	start_time = eng->get_game_time();
	last_pos = get_ws_position();
}
void ParticleComponent::end()
{
	idraw->get_scene()->remove_particle_obj(obj);
	ParticleMgr::get().unregister_this(this);
}
static bool is_between(float x, float y, float value)
{
	return value > x && value <= y;
}


glm::vec3 spawn_in_sphere(Random& r)
{
	glm::vec3 v=glm::vec3(0.f);
	int count = 0;
	do
	{
		v = glm::vec3(r.RandF(-1, 1), r.RandF(-1, 1), r.RandF(-1, 1));
		count++;
	} while (glm::dot(v, v) > 1.0&&count<10);
	return v;
}

void ParticleComponent::update()
{
	if (!is_playing)
		return;

	const glm::vec3 mypos = get_ws_position();
	const float dt = eng->get_dt();
	const glm::mat4 transform = get_ws_transform();
	const glm::vec3 est_vel = (mypos - last_pos) / dt;

	auto init_new_particle = [&]()
	{
		// init particle in sphere
		glm::vec3 pos = spawn_in_sphere(r);
		float len = glm::length(pos);
		pos *= bias;
		pos = glm::normalize(pos);
		auto dir = pos;
		pos = pos * glm::mix(spawn_sphere_min, spawn_sphere_max, len);

		float s = r.RandF(speed_min, speed_max);
		glm::vec3 vel = glm::vec3(0.f);
		if(init_particle_vel_sphere)
			vel = dir * s * pow(len, speed_exp);

		float lifetime = r.RandF(life_time_min, life_time_max);

		float size = r.RandF(size_min, size_max);

		float rot = r.RandF(rotation_min, rotation_max);
		float rot_vel = r.RandF(rot_vel_min, rot_vel_max);

		float d = glm::abs(glm::dot(dir, glm::vec3(0, 1, 0)));

		glm::vec3 side = (d < 0.9999) ? glm::normalize(glm::cross(dir, glm::vec3(0, 1, 0))) : glm::vec3(1, 0, 0);
		glm::vec3 up = glm::cross(side, dir);
		vel += (up * radial_vel.x + side * radial_vel.y)* s;

		if (!inherit_transform) {
			pos = transform * glm::vec4(pos,1.0);
			vel = glm::mat3(transform) * vel;
		}
		if (inherit_velocity)
			vel += est_vel;

		ParticleDef pdef;
		pdef.position = pos;
		pdef.velocity = vel;
		pdef.rotation = rot;
		pdef.rotationRate = rot_vel;
		pdef.lifeTime = lifetime;
		pdef.totalLife = lifetime;
		pdef.scale = size;

		active_particles.push_back(pdef);
	};

	float t = eng->get_game_time() - start_time;
	// add new particles
	if (continious_rate > 0.001) {
		float tick = 1 / continious_rate;
		float x = floor(last_t / tick)*tick;
		x += tick;
		int count = 0;
		while (is_between(last_t, t, x)) {
			count++;
			x += tick;
			ASSERT(count < 1000);
		}
		for (int i = 0; i < count; i++)
			init_new_particle();
	}
	if (is_between(last_t, t, burst_time)) {
		int c = r.RandI(burst_min, burst_max);
		for (int i = 0; i < c; i++)
			init_new_particle();
	}

	// update particles
	for (int i = 0; i < active_particles.size(); i++)
	{
		auto& p = active_particles[i];
		if (orbital_strength > 0.001) {
			auto center_pos = (!inherit_transform) ? mypos : glm::vec3(0.f);

			glm::vec3 to = center_pos - p.position;
			p.velocity += to * orbital_strength * dt;
		}
		p.velocity.y -=  gravity * dt;
		p.velocity -=  p.velocity* drag * dt;


		p.position += p.velocity * dt;
		p.lifeTime -= dt;
		p.scale -=  p.scale * size_decay * dt;

		if (p.lifeTime <= 0) {
			if (i < active_particles.size() - 1)
			{
				active_particles[i] = active_particles.back();
				active_particles.pop_back();
				i--;
			}
			else {
				active_particles.pop_back();
				break;
			}
		}
	}
	last_t = t;
	last_pos = mypos;

}


void ParticleComponent::draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front)
{
	builder.Begin();
	for (auto& p : active_particles) {
		int base = builder.GetBaseVertex();
		MbVertex v;
		v.position = p.position;
		v.position += (side + up)* p.scale;
		v.uv = glm::vec2(0, 0);
		builder.AddVertex(v);
		v.position = p.position;
		v.position += (-side + up)*p.scale;
		v.uv = glm::vec2(1, 0);
		builder.AddVertex(v);
		v.position = p.position;
		v.position += (-side - up)*p.scale;
		v.uv = glm::vec2(1, 1);
		builder.AddVertex(v);
		v.position = p.position;
		v.position += (side - up)*p.scale;
		v.uv = glm::vec2(0, 1);
		builder.AddVertex(v);

		builder.AddQuad(base, base + 1, base + 2, base + 3);
	}
	builder.End();
}

void ParticleComponent::on_changed_transform()
{
	Particle_Object o{};
	o.meshbuilder = &builder;
	o.material = particle_mat.get();
	o.owner = this;
	o.transform = (inherit_transform) ? get_ws_transform() : glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(obj, o);
}

void ParticleMgr::draw(const View_Setup& vs)
{
	glm::mat4 invview = glm::inverse(vs.view);
	
	for (auto c : all_components)
		c->draw(invview[1],invview[0],invview[2]);
}