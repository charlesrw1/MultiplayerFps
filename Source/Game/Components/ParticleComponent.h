#pragma once
#include "Game/EntityComponent.h"
#include "Render/MaterialPublic.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Framework/MathLib.h"
#include "Framework/Hashset.h"
#include "Framework/ReflectionMacros.h"
#include "Game/SerializePtrHelpers.h"

// fixme: these arent optimized really

struct ParticleDef
{
	glm::vec3 position{};
	glm::vec3 velocity{};
	float lifeTime = 0.0;
	float totalLife = 0.0;
	float scale{};
	float rotation{};
	float rotationRate{};
	Color32 colorA{};
	uint8_t lightIndex{};
};

class ParticleComponent : public Component {
public:
	CLASS_BODY(ParticleComponent);

	ParticleComponent() : r(0){
		set_call_init_in_editor(true);
		set_ticking(true);
	}
	void start() final;
	void update() final;
	void stop() final;
	void on_changed_transform() final {
		sync_render_data();
	}
	void on_sync_render_data() final;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final {
		return "eng/editor/particle.png";
	}
#endif

	REF float continious_rate = 0.f;	// particles/s
	REF float burst_time = 0.f;
	REF short burst_min = 0;
	REF short burst_max = 0;
	REF float gravity = 0.f;
	REF float drag = 0.f;
	REF float life_time_min = .5f;
	REF float life_time_max = 1.f;
	REF float size_min = 1.f;
	REF float size_max = 1.f;
	REF float size_decay = 0.f;
	REF Color32 start_color = COLOR_WHITE;
	REF Color32 end_color = COLOR_WHITE;
	REF float alpha_start = 1.0;
	REF float alpha_end = 1.0;
	REF float intensity = 1.0;
	REF float intensity_decay = 0.;
	REF float rotation_min = 0.f;
	REF float rotation_max = 0.f;
	REF float rot_vel_min = 0.f;
	REF float rot_vel_max = 0.f;
	REF float spawn_sphere_min = 0.f;
	REF float spawn_sphere_max = 0.5f;
	REF glm::vec3 bias=glm::vec3(1,1,1);
	REF bool init_particle_vel_sphere = true;
	REF float speed_min = 0.f;
	REF float speed_max = 1.f;
	REF float speed_exp = 1.f;
	REF glm::vec3 radial_vel = glm::vec3(0, 0, 0);
	REF float orbital_strength = 0.0;
	REF bool inherit_transform = true;
	REF bool inherit_velocity = false;
	REF AssetPtr<MaterialInstance> particle_mat;


	void restart_effect();
	bool is_effect_done() const;
	float get_effect_time_period() const;	// -1 for infinite

	void draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front);
private:
	bool is_playing = false;
	float start_time = 0.0;
	float last_t = 0.0;
	std::vector<ParticleDef> active_particles;
	handle<Particle_Object> obj;
	MeshBuilder builder;
	Random r;

	glm::vec3 last_pos = glm::vec3(0.f);
};

class TrailComponent : public Component {
public:
	CLASS_BODY(TrailComponent);
	TrailComponent() {
		set_call_init_in_editor(true);
	}
	void start() final;
	void stop() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;
	void draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front);

	REF void set_width(float w, bool use_camera_up) {
		this->width = w;
		this->use_camera_up = use_camera_up;
	}
	REF void set_history(int max, float dt) {
		this->history_dt = dt;
		this->max_history = max;
	}
	REF void set_material(MaterialInstance* m) {
		this->material = m;
	}

	REF MaterialInstance* material = nullptr;
	bool inherit_transform = false;
	REF int max_history = 20;
	REF float history_dt = 0.1;	// every X seconds, take a sample
	REF float width = 1.0;
	REF bool use_camera_up = true;

	float last_sample_time = 0.0;

	handle<Particle_Object> obj;
	MeshBuilder builder;
	struct PosEntry {
		glm::vec3 pos{};
		glm::vec3 up{};
	};
	std::vector<PosEntry> pos_history;
};
#include "Game/EntityPtr.h"
#include "Animation/Editor/Optional.h"
#include "Game/Entity.h"
// almost like a trail
class BeamComponent : public Component {
public:
	CLASS_BODY(BeamComponent);
	BeamComponent() {
		set_call_init_in_editor(true);
	}
	void start() final;
	void stop() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;
	void draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front);

	REF void set_vars(bool gravity, bool use_target, float stiff, float damp, float width, MaterialInstance* material, int sample_points) {
		this->apply_gravity = gravity;
		this->use_target_entity = use_target;
		this->stiff = stiff;
		this->damp = damp;
		this->width = width;
		this->material = material;
		this->sample_points_sz = sample_points;
	}
	REF void set_target_pos(glm::vec3 p) {
		this->world_pos_target = p;
	}
	REF void reset();
	REF void set_visible(bool b) {
		is_visible = b;
		sync_render_data();
	}
	REF bool is_visible = false;
	REF bool apply_gravity = false;
	REF bool use_target_entity = false;
	REF EntityPtr target;
	REF glm::vec3 world_pos_target = glm::vec3(0.f);
	REF float stiff = 1.0;
	REF float damp = 1.0;
	REF float width = 1.0;
	REF float triangle_alpha_fade = 1.0;
	opt<glm::vec3> get_target_pos() const {
		if (use_target_entity) {
			Entity* e = target.get();
			if (!e) {
				return std::nullopt;
			}
			return e->get_ws_position();
		}
		return world_pos_target;
	}

	REF MaterialInstance* material = nullptr;
	REF int sample_points_sz = 20;

	MeshBuilder builder;
	struct Positions {
		glm::vec3 pos{};
		glm::vec3 velocity{};
	};
	handle<Particle_Object> obj;
	std::vector<Positions> points;
};