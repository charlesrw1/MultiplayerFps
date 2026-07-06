#pragma once
#include "Game/EntityComponent.h"
#include "Game/Particles/ParticleAsset.h"
#include "Framework/MeshBuilder.h"
#include "Render/DrawPublic.h"
#include "Framework/MathLib.h"
#include "Framework/Hashset.h"
#include <unordered_map>

struct ParticleSystemDef
{
	glm::vec3 position{};
	glm::vec3 velocity{};
	float lifeTime = 0.f;
	float totalLife = 0.f;
	float scale = 1.f;
	float rotation = 0.f;
	float rotationRate = 0.f;
	Color32 colorA = COLOR_WHITE;
	float random_seed = 0.f;
	float start_size = 1.f;
	Color32 start_color = COLOR_WHITE;
	int tex_frame = 0;
	glm::vec3 inherited_velocity_contribution{};
};

struct TrailPoint
{
	glm::vec3 position{};
	float time_created = 0.f;
	float width = 1.f;
	Color32 color = COLOR_WHITE;
};

struct TrailData
{
	std::vector<TrailPoint> points;
	bool has_trail = false;
};

struct SubSystemState
{
	bool is_emitting = false;
	float elapsed_time = 0.f;
	float emission_accumulator = 0.f;
	float distance_emission_accumulator = 0.f;
	glm::vec3 last_emitter_position{};
	bool has_last_emitter_position = false;
	float emitter_speed = 0.f;
	glm::vec3 emitter_velocity{};
	int next_burst_index = 0;
	std::vector<ParticleSystemDef> particles;
	std::vector<TrailData> trail_data;
	Random rng{0};
};

struct BatchState
{
	MeshBuilder builder;
	handle<Particle_Object> render_handle;
};

class ParticleSystemComponent : public Component
{
public:
	CLASS_BODY(ParticleSystemComponent);

	ParticleSystemComponent() : rng(0) {
		set_call_init_in_editor(true);
		set_ticking(true);
	}

	void start() final;
	void update() final;
	void stop() final;
	void on_changed_transform() final;
	void on_sync_render_data() final;

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const final { return "eng/editor/particle.png"; }
	std::unique_ptr<IComponentEditorUi> create_editor_ui() final;
	void update_shape_gizmo(int subsystem_index);
	class MeshBuilderComponent* editor_shape_gizmo = nullptr;
	bool gizmo_drawn_this_frame = false;
#endif
	REF void set_parameters(ParticleAsset* asset, bool play_on_awake, bool destroy_owner_when_finished) {
		particle_asset = asset;
		this->play_on_awake = play_on_awake;
		this->destroy_owner_when_finished = destroy_owner_when_finished;
	}

	REF AssetPtr<ParticleAsset> particle_asset;
	REF bool play_on_awake = true;
	REF float playback_speed = 1.f;

	// if set, owning entity is destroyed the moment this system finishes playing (no looping subsystems, no particles alive)
	bool destroy_owner_when_finished = false;

	REF void play();
	REF void stop_emitting();
	REF void clear();
	REF bool is_playing() const;
	int get_alive_count() const;

	void draw(const glm::vec3& side, const glm::vec3& up, const glm::vec3& front);

	std::vector<SubSystemState> subsystem_states;

private:
	void init_subsystem_states();
	void emit_particles(int ss_idx, int count);
	void spawn_from_shape(const ShapeModule& shape, glm::vec3& out_pos, glm::vec3& out_vel, Random& rng);

	std::unordered_map<MaterialInstance*, BatchState> batch_states;
	Random rng;
	bool playing = false;
	float total_elapsed = 0.f;
};
