#pragma once
#include "Game/EntityComponent.h"
#include "Render/MaterialPublic.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Framework/MathLib.h"
#include "Framework/Hashset.h"
#include "Framework/ReflectionMacros.h"
#include "Game/SerializePtrHelpers.h"

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

class ParticleComponent : public Component {
public:
	CLASS_BODY(ParticleComponent);

	ParticleComponent() : r(0){
		set_call_init_in_editor(true);
		set_ticking(true);
	}
	void start() final;
	void update() final;
	void end() final;
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