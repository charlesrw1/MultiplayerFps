#pragma once
#include "Game/EntityComponent.h"
#include "Render/MaterialPublic.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Framework/MathLib.h"
#include "Framework/Hashset.h"

// todo: make this an asset instead

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

CLASS_H(ParticleComponent, EntityComponent)
public:
	ParticleComponent() : r(0){
		set_call_init_in_editor(true);
		set_ticking(true);
	}
	void start() override;
	void update() override;
	void end() override;
	void on_changed_transform() override;

	float continious_rate = 0.f;	// particles/s
	float burst_time = 0.f;
	short burst_min = 0;
	short burst_max = 0;

	float gravity = 0.f;
	float drag = 0.f;

	float life_time_min = .5f;
	float life_time_max = 1.f;
	float size_min = 1.f;
	float size_max = 1.f;
	float size_decay = 0.f;
	Color32 start_color = COLOR_WHITE;
	Color32 end_color = COLOR_WHITE;
	float alpha_start = 1.0;
	float alpha_end = 1.0;
	float intensity = 1.0;
	float intensity_decay = 0.;

	float rotation_min = 0.f;
	float rotation_max = 0.f;
	float rot_vel_min = 0.f;
	float rot_vel_max = 0.f;

	float spawn_sphere_min = 0.f;
	float spawn_sphere_max = 0.5f;
	glm::vec3 bias=glm::vec3(1,1,1);
	bool init_particle_vel_sphere = true;
	float speed_min = 0.f;
	float speed_max = 1.f;
	float speed_exp = 1.f;
	glm::vec3 radial_vel = glm::vec3(0, 0, 0);
	float orbital_strength = 0.0;

	bool inherit_transform = true;
	bool inherit_velocity = false;

	AssetPtr<MaterialInstance> particle_mat;

	static const PropertyInfoList* get_props() {
		START_PROPS(ParticleComponent)
			REG_FLOAT(continious_rate, PROP_DEFAULT, "0.0"),
			REG_FLOAT(burst_time,PROP_DEFAULT, "0.0"),
			REG_INT(burst_min, PROP_DEFAULT, "0"),
			REG_INT(burst_max, PROP_DEFAULT, "0"),
			REG_FLOAT(gravity, PROP_DEFAULT, "0"),
			REG_FLOAT(drag, PROP_DEFAULT, "0"),
			REG_FLOAT(life_time_min,PROP_DEFAULT,"0.5"),
			REG_FLOAT(life_time_max,PROP_DEFAULT,"0.5"),
			REG_FLOAT(size_min,PROP_DEFAULT,"0.5"),
			REG_FLOAT(size_max,PROP_DEFAULT,"0.5"),
			REG_FLOAT(size_decay,PROP_DEFAULT,"0.5"),

			REG_INT_W_CUSTOM(start_color, PROP_DEFAULT, "", "ColorUint"),
			REG_INT_W_CUSTOM(end_color, PROP_DEFAULT, "", "ColorUint"),
			REG_FLOAT(alpha_start,PROP_DEFAULT,"1"),
			REG_FLOAT(alpha_end,PROP_DEFAULT,"1"),
			REG_FLOAT(intensity_decay,PROP_DEFAULT,"0.5"),

			REG_FLOAT(rotation_min,PROP_DEFAULT,"1"),
			REG_FLOAT(rotation_max,PROP_DEFAULT,"1"),
			REG_FLOAT(rot_vel_min,PROP_DEFAULT,"1"),
			REG_FLOAT(rot_vel_max,PROP_DEFAULT,"1"),
			REG_VEC3(radial_vel, PROP_DEFAULT),
			REG_FLOAT(orbital_strength, PROP_DEFAULT,"0"),
			REG_BOOL(inherit_velocity,PROP_DEFAULT, "0"),

			REG_FLOAT(spawn_sphere_min,PROP_DEFAULT,"1"),
			REG_FLOAT(spawn_sphere_max,PROP_DEFAULT,"1"),
			REG_VEC3(bias,PROP_DEFAULT),
			REG_FLOAT(speed_min,PROP_DEFAULT,"0"),
			REG_FLOAT(speed_max,PROP_DEFAULT,"0"),
			REG_FLOAT(speed_exp,PROP_DEFAULT,"0"),
			REG_BOOL(init_particle_vel_sphere, PROP_DEFAULT,"1"),

			REG_BOOL(inherit_transform, PROP_DEFAULT, "1"),

			REG_ASSET_PTR(particle_mat, PROP_DEFAULT)
		END_PROPS(ParticleComponent)
	}

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

class ParticleMgr
{
public:
	ParticleMgr() : all_components(4) {}

	static ParticleMgr& get() {
		static ParticleMgr inst;
		return inst;
	}

	void draw(const View_Setup& setup);
	void register_this(ParticleComponent* c) {
		all_components.insert(c);
	}
	void unregister_this(ParticleComponent* c) {
		all_components.remove(c);
	}
private:
	hash_set<ParticleComponent> all_components;
};