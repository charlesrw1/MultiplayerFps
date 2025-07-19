#include "ParticleComponent.h"
#include "Render/RenderObj.h"
#include "GameEnginePublic.h"
#include "ParticleMgr.h"

// comment 6

ParticleMgr* ParticleMgr::inst = nullptr;

void ParticleComponent::on_sync_render_data()
{
	if(!obj.is_valid())
		obj = idraw->get_scene()->register_particle_obj();
	Particle_Object o{};
	o.meshbuilder = &builder;
	o.material = particle_mat.get();
	o.owner = this;
	o.transform = (inherit_transform) ? get_ws_transform() : glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(obj, o);
}
void ParticleComponent::start()
{
	
	r.state = wang_hash((uint32_t)get_instance_id());
	ParticleMgr::inst->register_this(this);

	is_playing = true;
	start_time = eng->get_game_time();
	last_pos = get_ws_position();
	active_particles.clear();

	sync_render_data();
}
void ParticleComponent::stop()
{
	idraw->get_scene()->remove_particle_obj(obj);
	ParticleMgr::inst->unregister_this(this);
	active_particles.clear();
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


void ParticleMgr::draw(const View_Setup& vs)
{
	glm::mat4 invview = glm::inverse(vs.view);
	
	for (auto c : all_components)
		c->draw(invview[1],invview[0],invview[2]);
	for (auto t : all_trails)
		t->draw(invview[1], invview[0], invview[2]);
	for (auto t : all_beams)
		t->draw(invview[1], invview[0], invview[2]);
}

void TrailComponent::start()
{
	set_ticking(false);
	ParticleMgr::inst->register_this(this);
	sync_render_data();
}

void TrailComponent::stop()
{
	ParticleMgr::inst->unregister_this(this);
	idraw->get_scene()->remove_particle_obj(obj);
}

void TrailComponent::on_changed_transform()
{
	sync_render_data();
}

void TrailComponent::on_sync_render_data()
{
	if (!obj.is_valid())
		obj = idraw->get_scene()->register_particle_obj();
	Particle_Object o{};
	o.meshbuilder = &builder;
	o.material = material;
	o.owner = this;

	o.transform = (inherit_transform) ? get_ws_transform() : glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(obj, o);
}

void TrailComponent::draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front)
{
	// get position
	const glm::mat4& transform = get_ws_transform();
	const float t = eng->get_game_time();
	const float dt = eng->get_dt();
	if (t > last_sample_time + history_dt) {
		PosEntry entry{ transform[3],transform[2] };
		// just do the stupid thing, fixme ringbuffer
		if (pos_history.size() >= max_history) {
			pos_history.erase(pos_history.begin());
		}
		pos_history.push_back(entry);
		last_sample_time = t;
	}

	builder.Begin();
	for (int i = 0; i < (int)pos_history.size(); i++) {
		auto& entry = pos_history[i];
		if (i+1 == (int)pos_history.size()) {
			entry.pos = transform[3];
		}


		const glm::vec3& upVec = (use_camera_up) ? up : entry.up;
		float len_alpha = 0.0;
		{
			int sz = pos_history.size();
			int len_minus_one = std::max(sz - 1, 1);
			len_alpha = float(i) / len_minus_one;
		}
		float widthToUse = width * len_alpha;


		int base = builder.GetBaseVertex();
		MbVertex v;
		v.position = entry.pos+ upVec * widthToUse;
		v.uv = glm::vec2(0, len_alpha);
		builder.AddVertex(v);
		v.position = entry.pos - upVec * widthToUse;
		v.uv = glm::vec2(1, len_alpha);
		builder.AddVertex(v);

		// if last entry, skip. (quad was added in prev iteration)
		if (i + 1 != (int)pos_history.size()) {
			builder.AddQuad(base, base + 2, base + 3, base + 1);
		}
	}
	builder.End();
}

void BeamComponent::start()
{
	set_ticking(false);
	ParticleMgr::inst->register_this(this);
	sync_render_data();
}

void BeamComponent::stop()
{
	ParticleMgr::inst->unregister_this(this);
	idraw->get_scene()->remove_particle_obj(obj);
}

void BeamComponent::on_changed_transform()
{
	sync_render_data();
}

void BeamComponent::on_sync_render_data()
{
	if (!obj.is_valid())
		obj = idraw->get_scene()->register_particle_obj();
	Particle_Object o{};
	o.meshbuilder = &builder;
	o.material = material;
	o.owner = this;
	o.transform = glm::mat4(1.f);
	idraw->get_scene()->update_particle_obj(obj, o);
}

ConfigVar rope_adjust("rope_adjust", "1", CVAR_FLOAT, "");
ConfigVar rope_grav("rope_grav", "1", CVAR_FLOAT|CVAR_UNBOUNDED, "");
ConfigVar remove_rope_vel("remove_rope_vel", "0.99", CVAR_FLOAT, "");

void BeamComponent::draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front)
{
	if (!is_visible) {
		// fixme
		builder.Begin();
		builder.End();
		return;
	}

	if (sample_points_sz < 2)
		return;
	opt<glm::vec3> target_pos = get_target_pos();
	if (!target_pos.has_value())
		return;
	glm::vec3 my_pos = get_owner()->get_ws_position();
	float dt = eng->get_dt();
	points.resize(sample_points_sz);
	const float rope_length = glm::length(my_pos - target_pos.value());
	const float node_rest_length = (rope_length / std::max(int(points.size()) - 1, 1))* rope_adjust.get_float();

	points.at(0).velocity = (my_pos-points.at(0).pos) / dt;
	points.at(0).pos = my_pos;
	const int last_point = (int)points.size() - 1;
	points.at(last_point).velocity = (*target_pos - points.at(last_point).pos) / dt;
	points.at(last_point).pos = *target_pos;
	const int steps = 1;
	dt /= steps;
	for (int step = 0; step < steps; step++) {
		for (int i = 0; i + 1 < points.size(); i++) {
			auto& point = points[i];
			auto& point2 = points[i + 1];
			glm::vec3 delta = point.pos - point2.pos;
			float dist = glm::length(delta);
			glm::vec3 dir = delta;
			if (dist >= 0.001)
				dir /= dist;


			float stiff_mult = 1.0;
			float damp_mult = 1.0;

			glm::vec3 force = -stiff * (dist - node_rest_length) * dir * stiff_mult;
			glm::vec3 vel_diff = point.velocity - point2.velocity;
			force -= dir * damp * dot(vel_diff, dir) * damp_mult;

			const float rope_gravity_val = 9.0 * rope_grav.get_float();
			if (i != 0) {
				point.velocity += force * dt;

				if (apply_gravity)
					point.velocity.y -= rope_gravity_val * dt;

				point.velocity *= remove_rope_vel.get_float();
			}
			if (i + 1 != int(points.size())) {
				point2.velocity -= force * dt;
				point2.velocity *= remove_rope_vel.get_float();

			}
		}
		for (int i = 1; i + 1 < points.size(); i++) {
			points[i].pos += points[i].velocity * dt;
		}
	}

	builder.Begin();
	for (int i = 0; i < (int)points.size(); i++) {
		auto& entry = points[i];

		const glm::vec3& upVec = up;
		float len_alpha = 0.0;
		{
			int sz = points.size();
			int len_minus_one = std::max(sz - 1, 1);
			len_alpha = float(i) / len_minus_one;
		}
		float widthToUse = width;// *len_alpha;


		int base = builder.GetBaseVertex();
		MbVertex v;
		v.position = entry.pos + upVec * widthToUse;
		v.uv = glm::vec2(0, len_alpha);
		builder.AddVertex(v);
		v.position = entry.pos - upVec * widthToUse;
		v.uv = glm::vec2(1, len_alpha);
		builder.AddVertex(v);

		// if last entry, skip. (quad was added in prev iteration)
		if (i + 1 != (int)points.size()) {
			builder.AddQuad(base, base + 2, base + 3, base + 1);
		}
	}
	builder.End();
}

void BeamComponent::reset()
{
	if (sample_points_sz < 2)
		return;
	opt<glm::vec3> target_pos = get_target_pos();
	if (!target_pos.has_value())
		return;
	glm::vec3 my_pos = get_owner()->get_ws_position();
	float dt = eng->get_dt();
	points.resize(sample_points_sz);
	const float rope_length = glm::length(my_pos - target_pos.value());
	const float node_rest_length = (rope_length / std::max(int(points.size()) - 1, 1)) * rope_adjust.get_float();

	points.at(0).velocity = (my_pos - points.at(0).pos) / dt;
	points.at(0).pos = my_pos;
	const int last_point = (int)points.size() - 1;
	points.at(last_point).velocity = (*target_pos - points.at(last_point).pos) / dt;
	points.at(last_point).pos = *target_pos;

	for (int i = 1; i+1 < points.size(); i++) {
		float alpha = float(i) / float(sample_points_sz - 1);
		points[i].pos = glm::mix(my_pos, *target_pos, alpha);
		points[i].velocity = glm::vec3(0.f);
	}
}
