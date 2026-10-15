#pragma once

#include "Game/EntityComponent.h"
#include "Render/DynamicModelPtr.h"
#include "Framework/Handle.h"
#include <array>
#include <vector>

class MaterialInstance;
class Texture;
struct Render_Object;
namespace physx { class PxRigidActor; }

// Persistent, CPU-authored heightfield terrain.  Height and splat data live in a compact
// sidecar so editing a terrain never bloats its owning .tmap/.tprefab file.
class TerrainComponent : public Component {
public:
	CLASS_BODY(TerrainComponent, spawnable);
	TerrainComponent();
	~TerrainComponent() override;

	void start() override;
	void update() override;
	void stop() override;
	void on_sync_render_data() override;
	void on_changed_transform() override;
	void serialize(Serializer& s) override;

	REF void set_resolution(int value);
	REF int get_resolution() const { return resolution; }
	REF void set_size(float value);
	REF float get_size() const { return size; }
	REF void set_material_tile_size(float value) { material_tile_size = glm::max(value, 0.01f); rebuild_runtime(); }
	REF float get_material_tile_size() const { return material_tile_size; }
	REF void set_data_path(const std::string& value);
	REF const std::string& get_data_path() const { return data_path; }
	REF void set_material0(MaterialInstance* value) { material0 = value; mark_render_dirty(); }
	REF void set_material1(MaterialInstance* value) { material1 = value; mark_render_dirty(); }
	REF void set_material2(MaterialInstance* value) { material2 = value; mark_render_dirty(); }
	REF void set_material3(MaterialInstance* value) { material3 = value; mark_render_dirty(); }

	bool save_data();
	bool load_data();
	void rebuild_runtime();
	float sample_height_local(float x, float z) const;
	float sample_height_world(float x, float z) const;

	enum class Brush { Raise, Lower, Smooth, Flatten, Noise, ThermalErosion, PaintMaterial };
	void apply_brush(Brush brush, const glm::vec3& world_pos, float radius, float strength,
		float falloff = 1.f, int material_layer = 0);
	// Call once at the end of a brush stroke. Collision cooking is intentionally deferred
	// here; doing it once per mouse sample causes severe editor hitches.
	void finish_edit_stroke();
	std::vector<float> copy_heights() const { return heights; }
	std::vector<uint8_t> copy_weights() const { return weights; }
	void restore_edit_data(std::vector<float> new_heights, std::vector<uint8_t> new_weights);

#ifdef EDITOR_BUILD
	const char* get_editor_outliner_icon() const override { return "eng/editor/mesh_icon.png"; }
#endif

private:
	void ensure_data();
	void build_render_meshes();
	void update_clipmap();
	void ensure_gpu_textures();
	void upload_terrain_textures();
	void update_terrain_material();
	void destroy_render_meshes();
	void rebuild_collision();
	void destroy_collision();
	void mark_render_dirty() { textures_dirty = true; sync_render_data(); }
	int index(int x, int z) const { return z * resolution + x; }
	float height_at(int x, int z) const;

	REF int resolution = 129; // 128 quads per side; keeps dynamic-model indices within uint16.
	REF float size = 100.f;
	REF float material_tile_size = 2.f;
	REF bool collision_enabled = true;
	REF std::string data_path = "terrain/terrain.terrain";
	REF MaterialInstance* material0 = nullptr;
	REF MaterialInstance* material1 = nullptr;
	REF MaterialInstance* material2 = nullptr;
	REF MaterialInstance* material3 = nullptr;

	std::vector<float> heights;
	std::vector<uint8_t> weights; // RGBA splat weights: four paintable material layers.
	// Five nested, snapped grid rings.  Ring zero is the high-detail centre;
	// each successive ring doubles spacing and has a hole for the previous ring.
	static constexpr int CLIPMAP_LEVELS = 5;
	static constexpr int CLIPMAP_GRID = 65;
	std::array<DynamicModelUniquePtr, CLIPMAP_LEVELS> dynamic_models;
	std::array<handle<Render_Object>, CLIPMAP_LEVELS> render_handles;
	std::array<glm::vec2, CLIPMAP_LEVELS> clipmap_centers{};
	Texture* heightmap_texture = nullptr;
	Texture* normalmap_texture = nullptr;
	Texture* splatmap_texture = nullptr;
	// Asset-owned rather than dynamically allocated: render-object removal is
	// deferred, so a transient dynamic material can otherwise be freed while a
	// proxy still references it.
	MaterialInstance* terrain_material = nullptr;
	physx::PxRigidActor* collision_actor = nullptr;
	bool data_dirty = false;
	bool textures_dirty = true;
	bool clipmap_dirty = true;
};
