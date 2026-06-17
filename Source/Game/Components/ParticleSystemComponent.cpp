#include "ParticleSystemComponent.h"
#include "Render/RenderObj.h"
#include "GameEnginePublic.h"
#include "ParticleMgr.h"
#include "BillboardComponent.h"
#include "MeshbuilderComponent.h"
#include "Game/Entity.h"
#include "Game/Particles/ParticleAsset.h"
#include "AssetTools/AssetTemplates.h"
#include "Assets/AssetDatabase.h"
#include <glm/gtc/noise.hpp>

void ParticleSystemComponent::start()
{
	if (eng->is_editor_level()) {
		auto billboard = get_owner()->create_component<BillboardComponent>();
		billboard->set_texture(Texture::load("eng/icon/_nearest/particle.png"));
		billboard->dont_serialize_or_edit = true;

		if (!particle_asset.get() || !particle_asset->is_valid_to_use()) {
			auto result = AssetTemplates::create_empty_particle("", "default_particle");
			std::string path = result.value_or("default_particle.particle");
			particle_asset = g_assets.find<ParticleAsset>(path);
		}

		editor_shape_gizmo = get_owner()->create_component<MeshBuilderComponent>();
		editor_shape_gizmo->dont_serialize_or_edit = true;
		editor_shape_gizmo->use_transform = true;
		editor_shape_gizmo->depth_tested = true;
	}

	rng.state = wang_hash((uint32_t)get_instance_id());
	ParticleMgr::inst->register_this(this);

	if (play_on_awake && particle_asset.get() && particle_asset->is_valid_to_use())
		play();
}

void ParticleSystemComponent::stop()
{
	for (auto& [mat, batch] : batch_states)
		idraw->get_scene()->remove_particle_obj(batch.render_handle);
	batch_states.clear();
	ParticleMgr::inst->unregister_this(this);
	subsystem_states.clear();

	if (editor_shape_gizmo) {
		editor_shape_gizmo->destroy();
		editor_shape_gizmo = nullptr;
	}
}

void ParticleSystemComponent::on_changed_transform()
{
	for (auto& [mat, batch] : batch_states) {
		if (batch.render_handle.is_valid()) {
			Particle_Object o{};
			o.meshbuilder = &batch.builder;
			o.material = mat;
			o.owner = this;
			o.transform = glm::mat4(1.f);
			idraw->get_scene()->update_particle_obj(batch.render_handle, o);
		}
	}
}

void ParticleSystemComponent::on_sync_render_data()
{
	for (auto& [mat, batch] : batch_states) {
		if (!batch.render_handle.is_valid())
			batch.render_handle = idraw->get_scene()->register_particle_obj();
		Particle_Object o{};
		o.meshbuilder = &batch.builder;
		o.material = mat;
		o.owner = this;
		o.transform = glm::mat4(1.f);
		idraw->get_scene()->update_particle_obj(batch.render_handle, o);
	}
}

void ParticleSystemComponent::play()
{
	auto* asset = particle_asset.get();
	if (!asset || !asset->is_valid_to_use())
		return;

	playing = true;
	total_elapsed = 0.f;
	init_subsystem_states();
}

void ParticleSystemComponent::stop_emitting()
{
	for (auto& ss : subsystem_states)
		ss.is_emitting = false;
}

void ParticleSystemComponent::clear()
{
	for (auto& ss : subsystem_states)
		ss.particles.clear();
	playing = false;
	total_elapsed = 0.f;
}

bool ParticleSystemComponent::is_playing() const
{
	return playing;
}

int ParticleSystemComponent::get_alive_count() const
{
	int count = 0;
	for (auto& ss : subsystem_states)
		count += (int)ss.particles.size();
	return count;
}

void ParticleSystemComponent::init_subsystem_states()
{
	auto* asset = particle_asset.get();
	ASSERT(asset);
	subsystem_states.resize(asset->subsystems.size());
	for (int i = 0; i < (int)asset->subsystems.size(); i++) {
		auto& state = subsystem_states[i];
		state.is_emitting = true;
		state.elapsed_time = 0.f;
		state.emission_accumulator = 0.f;
		state.next_burst_index = 0;
		state.particles.clear();
		state.rng.state = wang_hash(rng.Rand() + i);
	}
}

void ParticleSystemComponent::spawn_from_shape(const ShapeModule& shape, glm::vec3& out_pos, glm::vec3& out_vel, Random& r)
{
	switch (shape.shape) {
	case ParticleShapeType::Sphere: {
		glm::vec3 dir;
		int count = 0;
		do {
			dir = glm::vec3(r.RandF(-1, 1), r.RandF(-1, 1), r.RandF(-1, 1));
			count++;
		} while (glm::dot(dir, dir) > 1.f && count < 10);
		if (glm::length(dir) > 0.001f)
			dir = glm::normalize(dir);
		float dist = glm::mix(1.f - shape.radius_thickness, 1.f, r.RandF()) * shape.radius;
		out_pos = dir * dist + shape.position_offset;
		out_vel = dir;
		break;
	}
	case ParticleShapeType::Hemisphere: {
		glm::vec3 dir;
		int count = 0;
		do {
			dir = glm::vec3(r.RandF(-1, 1), r.RandF(0, 1), r.RandF(-1, 1));
			count++;
		} while (glm::dot(dir, dir) > 1.f && count < 10);
		if (glm::length(dir) > 0.001f)
			dir = glm::normalize(dir);
		float dist = glm::mix(1.f - shape.radius_thickness, 1.f, r.RandF()) * shape.radius;
		out_pos = dir * dist + shape.position_offset;
		out_vel = dir;
		break;
	}
	case ParticleShapeType::Cone: {
		float angle_rad = glm::radians(shape.angle);
		float theta = r.RandF() * 2.f * glm::pi<float>();
		float phi = r.RandF() * angle_rad;
		glm::vec3 dir(glm::sin(phi) * glm::cos(theta), glm::cos(phi), glm::sin(phi) * glm::sin(theta));
		float dist = glm::mix(1.f - shape.radius_thickness, 1.f, r.RandF()) * shape.radius;
		out_pos = glm::vec3(dir.x, 0.f, dir.z) * dist + shape.position_offset;
		out_vel = dir;
		break;
	}
	case ParticleShapeType::Box: {
		out_pos = glm::vec3(r.RandF(-0.5f, 0.5f), r.RandF(-0.5f, 0.5f), r.RandF(-0.5f, 0.5f))
			* shape.scale_offset + shape.position_offset;
		out_vel = glm::vec3(0, 1, 0);
		break;
	}
	case ParticleShapeType::Circle: {
		float theta = r.RandF() * 2.f * glm::pi<float>() * (shape.arc / 360.f);
		float dist = glm::mix(1.f - shape.radius_thickness, 1.f, r.RandF()) * shape.radius;
		out_pos = glm::vec3(glm::cos(theta), 0.f, glm::sin(theta)) * dist + shape.position_offset;
		out_vel = glm::normalize(glm::vec3(out_pos.x, 0.f, out_pos.z) + glm::vec3(0, 0.01f, 0));
		break;
	}
	}
}

void ParticleSystemComponent::emit_particles(int ss_idx, int count)
{
	auto* asset = particle_asset.get();
	ASSERT(asset && ss_idx < (int)asset->subsystems.size());
	auto& subsys = asset->subsystems[ss_idx];
	auto& state = subsystem_states[ss_idx];
	auto& r = state.rng;

	int max_p = subsys.main.max_particles;
	for (int i = 0; i < count && (int)state.particles.size() < max_p; i++) {
		ParticleSystemDef p;
		p.random_seed = r.RandF();

		float lifetime = subsys.main.start_lifetime.evaluate(0.f, p.random_seed);
		float speed = subsys.main.start_speed.evaluate(0.f, p.random_seed);
		float size = subsys.main.start_size.evaluate(0.f, p.random_seed);
		float rot = subsys.main.start_rotation.evaluate(0.f, p.random_seed);

		glm::vec3 pos(0.f), vel(0.f);
		if (subsys.shape.enabled)
			spawn_from_shape(subsys.shape, pos, vel, r);

		vel *= speed;

		if (subsys.main.simulation_space == SimulationSpace::World) {
			glm::mat4 transform = get_ws_transform();
			pos = glm::vec3(transform * glm::vec4(pos, 1.f));
			vel = glm::mat3(transform) * vel;
		}

		Color32 start_col = subsys.main.start_color.evaluate(p.random_seed);

		p.position = pos;
		p.velocity = vel;
		p.lifeTime = lifetime;
		p.totalLife = lifetime;
		p.scale = size;
		p.start_size = size;
		p.rotation = rot;
		p.start_color = start_col;
		p.colorA = start_col;

		state.particles.push_back(p);
	}
}

void ParticleSystemComponent::update()
{
	if (!playing)
		return;
	auto* asset = particle_asset.get();
	if (!asset || !asset->is_valid_to_use())
		return;

	const float dt = eng->get_dt() * playback_speed;
	total_elapsed += dt;

	for (int ss_idx = 0; ss_idx < (int)asset->subsystems.size(); ss_idx++) {
		if (ss_idx >= (int)subsystem_states.size())
			break;

		auto& subsys = asset->subsystems[ss_idx];
		auto& state = subsystem_states[ss_idx];
		auto& main = subsys.main;

		state.elapsed_time += dt;

		// handle looping
		if (main.looping && state.elapsed_time > main.duration) {
			state.elapsed_time = fmod(state.elapsed_time, main.duration);
			state.next_burst_index = 0;
		}
		else if (!main.looping && state.elapsed_time > main.duration) {
			state.is_emitting = false;
		}

		// emission
		if (state.is_emitting && subsys.emission.enabled) {
			float normalized_t = main.duration > 0.f ? state.elapsed_time / main.duration : 0.f;
			float rate = subsys.emission.rate_over_time.evaluate(normalized_t, state.rng.RandF());
			state.emission_accumulator += rate * dt;
			int to_emit = (int)state.emission_accumulator;
			state.emission_accumulator -= to_emit;
			if (to_emit > 0)
				emit_particles(ss_idx, to_emit);

			// bursts
			for (int bi = state.next_burst_index; bi < (int)subsys.emission.bursts.size(); bi++) {
				auto& burst = subsys.emission.bursts[bi];
				if (state.elapsed_time >= burst.time) {
					int burst_count = (int)burst.count.evaluate(0.f, state.rng.RandF());
					emit_particles(ss_idx, burst_count);
					state.next_burst_index = bi + 1;
				}
				else break;
			}
		}

		// per-particle update
		auto& particles = state.particles;
		for (int i = 0; i < (int)particles.size(); i++) {
			auto& p = particles[i];
			float normalized_life = (p.totalLife > 0.f) ? (1.f - p.lifeTime / p.totalLife) : 1.f;

			// velocity over lifetime
			if (subsys.velocity_over_lifetime.enabled) {
				auto& vol = subsys.velocity_over_lifetime;
				float vx = vol.x.evaluate(normalized_life, p.random_seed);
				float vy = vol.y.evaluate(normalized_life, p.random_seed);
				float vz = vol.z.evaluate(normalized_life, p.random_seed);
				p.velocity += glm::vec3(vx, vy, vz) * dt;
			}

			// gravity
			float grav = main.gravity_modifier.evaluate(normalized_life, p.random_seed);
			p.velocity.y -= grav * 9.81f * dt;

			// noise
			if (subsys.noise.enabled) {
				auto& nm = subsys.noise;
				float strength = nm.strength.evaluate(normalized_life, p.random_seed);
				float scroll = nm.scroll_speed.evaluate(normalized_life, p.random_seed);
				float freq = nm.frequency;
				float time_offset = state.elapsed_time * scroll;

				glm::vec3 noise_pos = p.position * freq;
				glm::vec3 displacement(0.f);
				float amplitude = 1.f;
				for (int oct = 0; oct < nm.octaves; oct++) {
					float nx = glm::perlin(glm::vec3(noise_pos.y + time_offset, noise_pos.z, (float)oct * 31.7f));
					float ny = glm::perlin(glm::vec3(noise_pos.z + time_offset, noise_pos.x, (float)oct * 31.7f + 17.3f));
					float nz = glm::perlin(glm::vec3(noise_pos.x + time_offset, noise_pos.y, (float)oct * 31.7f + 43.1f));
					displacement += glm::vec3(nx, ny, nz) * amplitude;
					noise_pos *= 2.f;
					amplitude *= (nm.damping ? 0.5f : 1.f);
				}
				p.position += displacement * strength * dt;
			}

			// integrate position
			p.position += p.velocity * dt;

			// size over lifetime
			if (subsys.size_over_lifetime.enabled) {
				float s = subsys.size_over_lifetime.size.evaluate(normalized_life, p.random_seed);
				p.scale = p.start_size * s;
			}

			// color over lifetime
			if (subsys.color_over_lifetime.enabled) {
				Color32 col = subsys.color_over_lifetime.color.evaluate(normalized_life);
				p.colorA = Color32(
					(uint8_t)((col.r * p.start_color.r) / 255),
					(uint8_t)((col.g * p.start_color.g) / 255),
					(uint8_t)((col.b * p.start_color.b) / 255),
					(uint8_t)((col.a * p.start_color.a) / 255));
			}

			// rotation over lifetime
			if (subsys.rotation_over_lifetime.enabled) {
				float angular_vel = subsys.rotation_over_lifetime.angular_velocity.evaluate(normalized_life, p.random_seed);
				p.rotation += angular_vel * dt;
			}

			// texture sheet
			if (subsys.texture_sheet.enabled) {
				auto& ts = subsys.texture_sheet;
				int total_frames = ts.tiles_x * ts.tiles_y;
				float frame_f = ts.frame_over_time.evaluate(normalized_life, p.random_seed);
				p.tex_frame = glm::clamp((int)(frame_f * total_frames), 0, total_frames - 1);
			}

			// kill dead particles
			p.lifeTime -= dt;
			if (p.lifeTime <= 0.f) {
				if (i < (int)particles.size() - 1)
					particles[i] = particles.back();
				particles.pop_back();
				i--;
			}
		}
	}

	// check if everything is done
	if (!subsystem_states.empty()) {
		bool all_done = true;
		for (auto& ss : subsystem_states) {
			if (ss.is_emitting || !ss.particles.empty()) {
				all_done = false;
				break;
			}
		}
		if (all_done)
			playing = false;
	}
}

void ParticleSystemComponent::draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front)
{
	auto* asset = particle_asset.get();
	if (!asset || !asset->is_valid_to_use())
		return;

	// clear all batch builders
	for (auto& [mat, batch] : batch_states)
		batch.builder.Begin();

	// group particles by material and build quads
	for (int ss_idx = 0; ss_idx < (int)asset->subsystems.size(); ss_idx++) {
		if (ss_idx >= (int)subsystem_states.size())
			break;
		auto& subsys = asset->subsystems[ss_idx];
		if (!subsys.renderer.enabled)
			continue;

		MaterialInstance* mat = subsys.renderer.material.get();
		if (!mat)
			continue;

		auto& batch = batch_states[mat];

		glm::mat4 local_to_world = (subsys.main.simulation_space == SimulationSpace::Local)
			? get_ws_transform() : glm::mat4(1.f);

		for (auto& p : subsystem_states[ss_idx].particles) {
			glm::vec3 world_pos = (subsys.main.simulation_space == SimulationSpace::Local)
				? glm::vec3(local_to_world * glm::vec4(p.position, 1.f))
				: p.position;

			float s = p.scale;
			float rot = p.rotation;
			float cos_r = glm::cos(rot);
			float sin_r = glm::sin(rot);
			glm::vec3 r_side = (side * cos_r + up * sin_r) * s;
			glm::vec3 r_up = (-side * sin_r + up * cos_r) * s;

			int base = batch.builder.GetBaseVertex();
			MbVertex v;
			v.color = p.colorA;

			float u0 = 0.f, v0_coord = 0.f, u1 = 1.f, v1_coord = 1.f;
			if (subsys.texture_sheet.enabled && subsys.texture_sheet.tiles_x > 1) {
				int tx = subsys.texture_sheet.tiles_x;
				int ty = subsys.texture_sheet.tiles_y;
				int col = p.tex_frame % tx;
				int row = p.tex_frame / tx;
				float tw = 1.f / tx;
				float th = 1.f / ty;
				u0 = col * tw;
				v0_coord = row * th;
				u1 = u0 + tw;
				v1_coord = v0_coord + th;
			}

			v.position = world_pos + r_side + r_up;
			v.uv = {u0, v0_coord};
			batch.builder.AddVertex(v);
			v.position = world_pos - r_side + r_up;
			v.uv = {u1, v0_coord};
			batch.builder.AddVertex(v);
			v.position = world_pos - r_side - r_up;
			v.uv = {u1, v1_coord};
			batch.builder.AddVertex(v);
			v.position = world_pos + r_side - r_up;
			v.uv = {u0, v1_coord};
			batch.builder.AddVertex(v);

			batch.builder.AddQuad(base, base + 1, base + 2, base + 3);
		}
	}

	// end all batch builders
	for (auto& [mat, batch] : batch_states)
		batch.builder.End();
}

#ifdef EDITOR_BUILD

static void add_line_circle_3d(MeshBuilder& mb, glm::vec3 center, glm::vec3 axis_up,
	float radius, int segments, Color32 color, float arc_degrees = 360.f)
{
	glm::vec3 right, forward;
	if (glm::abs(glm::dot(axis_up, glm::vec3(0, 1, 0))) < 0.99f)
		right = glm::normalize(glm::cross(axis_up, glm::vec3(0, 1, 0)));
	else
		right = glm::normalize(glm::cross(axis_up, glm::vec3(0, 0, 1)));
	forward = glm::cross(right, axis_up);

	float arc_rad = glm::radians(arc_degrees);
	glm::vec3 prev = center + right * radius;
	for (int i = 1; i <= segments; i++) {
		float theta = arc_rad * (float)i / segments;
		glm::vec3 pt = center + (right * glm::cos(theta) + forward * glm::sin(theta)) * radius;
		mb.PushLine(prev, pt, color);
		prev = pt;
	}
}

static void add_line_cone(MeshBuilder& mb, glm::vec3 origin, float radius, float angle_deg,
	float height, int segments, Color32 color)
{
	float angle_rad = glm::radians(angle_deg);
	float top_radius = radius + height * glm::tan(angle_rad);

	add_line_circle_3d(mb, origin, glm::vec3(0, 1, 0), radius, segments, color);
	glm::vec3 top = origin + glm::vec3(0, height, 0);
	add_line_circle_3d(mb, top, glm::vec3(0, 1, 0), top_radius, segments, color);

	for (int i = 0; i < 4; i++) {
		float theta = glm::two_pi<float>() * i / 4.f;
		float cos_t = glm::cos(theta);
		float sin_t = glm::sin(theta);
		glm::vec3 bottom_pt = origin + glm::vec3(cos_t * radius, 0, sin_t * radius);
		glm::vec3 top_pt = top + glm::vec3(cos_t * top_radius, 0, sin_t * top_radius);
		mb.PushLine(bottom_pt, top_pt, color);
	}
}

void ParticleSystemComponent::update_shape_gizmo(int subsystem_index)
{
	if (!editor_shape_gizmo)
		return;
	auto* asset = particle_asset.get();
	if (!asset || !asset->is_valid_to_use() || subsystem_index < 0
		|| subsystem_index >= (int)asset->subsystems.size()) {
		editor_shape_gizmo->mb.Begin();
		editor_shape_gizmo->mb.End();
		editor_shape_gizmo->sync_render_data();
		return;
	}

	auto& ss = asset->subsystems[subsystem_index];
	if (!ss.shape.enabled) {
		editor_shape_gizmo->mb.Begin();
		editor_shape_gizmo->mb.End();
		editor_shape_gizmo->sync_render_data();
		return;
	}

	auto& shape = ss.shape;
	glm::vec3 center = shape.position_offset;

	const Color32 gizmo_color = Color32(0, 200, 100, 200);
	const int segments = 32;

	auto& mb = editor_shape_gizmo->mb;
	mb.Begin();

	switch (shape.shape) {
	case ParticleShapeType::Sphere:
		mb.AddLineSphere(center, shape.radius, gizmo_color);
		if (shape.radius_thickness < 1.f)
			mb.AddLineSphere(center, shape.radius * (1.f - shape.radius_thickness), Color32(0, 150, 75, 120));
		break;

	case ParticleShapeType::Hemisphere:
		add_line_circle_3d(mb, center, glm::vec3(0, 1, 0), shape.radius, segments, gizmo_color);
		for (int j = 0; j < 4; j++) {
			float theta = glm::two_pi<float>() * j / 4.f;
			glm::vec3 prev_pt = center;
			for (int i = 0; i <= 16; i++) {
				float phi = glm::half_pi<float>() * (float)i / 16.f;
				float x = glm::sin(phi) * glm::cos(theta) * shape.radius;
				float y = glm::cos(phi) * shape.radius;
				float z = glm::sin(phi) * glm::sin(theta) * shape.radius;
				glm::vec3 pt = center + glm::vec3(x, y, z);
				if (i > 0) mb.PushLine(prev_pt, pt, gizmo_color);
				prev_pt = pt;
			}
		}
		break;

	case ParticleShapeType::Cone:
		add_line_cone(mb, center, shape.radius, shape.angle, shape.radius * 2.f, segments, gizmo_color);
		break;

	case ParticleShapeType::Box: {
		glm::vec3 half = shape.scale_offset * 0.5f;
		mb.PushOrientedLineBox(-half, half, glm::translate(glm::mat4(1.f), center), gizmo_color);
		break;
	}
	case ParticleShapeType::Circle:
		add_line_circle_3d(mb, center, glm::vec3(0, 1, 0), shape.radius, segments, gizmo_color, shape.arc);
		if (shape.arc < 360.f) {
			mb.PushLine(center, center + glm::vec3(shape.radius, 0, 0), gizmo_color);
			float arc_rad = glm::radians(shape.arc);
			glm::vec3 end_pt = center + glm::vec3(glm::cos(arc_rad), 0, glm::sin(arc_rad)) * shape.radius;
			mb.PushLine(center, end_pt, gizmo_color);
		}
		if (shape.radius_thickness < 1.f) {
			float inner = shape.radius * (1.f - shape.radius_thickness);
			add_line_circle_3d(mb, center, glm::vec3(0, 1, 0), inner, segments,
				Color32(0, 150, 75, 120), shape.arc);
		}
		break;
	}

	mb.End();
	editor_shape_gizmo->sync_render_data();
}

#endif
