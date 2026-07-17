#pragma once
// Miscellaneous render helpers: Texture3d, render pass params, shared render targets,
// DecalBatcher, LightListCuller, DebuggingTextureOutput, ThumbnailRenderer.

#include "Render/DrawTypedefs.h"
#include "Render/RenderScene.h"
#include "Render/RenderLevelParams.h" // Render_lists_cpufast, Render_Level_Params
#include "Framework/Config.h"

#include "glm/glm.hpp"
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

class IGraphicsTexture;
class IGraphicsBuffer;
class Texture;
class Model;
class MaterialInstance;

// ---------------------------------------------------------------------------
// ConfigVar externs
// ---------------------------------------------------------------------------

extern ConfigVar draw_collision_tris;
extern ConfigVar draw_sv_colliders;
extern ConfigVar draw_viewmodel;
extern ConfigVar enable_vsync;
extern ConfigVar shadow_quality_setting;
extern ConfigVar enable_bloom;
extern ConfigVar enable_volumetric_fog;
extern ConfigVar enable_ssao;
extern ConfigVar use_halfres_reflections;
extern ConfigVar r_indirect_loop;

// ---------------------------------------------------------------------------
// 3-D texture wrapper + Perlin generator
// ---------------------------------------------------------------------------

struct Texture3d
{
	glm::ivec3 size;
	uint32_t id = 0;
};
Texture3d generate_perlin_3d(glm::ivec3 size, uint32_t seed, int octaves, int frequency, float persistence,
							 float lacunarity);

// ---------------------------------------------------------------------------
// Cubemap constants
// ---------------------------------------------------------------------------

const int MAX_CUBEMAPS = 32;
const int CUBEMAP_WIDTH = 128;

// ---------------------------------------------------------------------------
// Bloom constant
// ---------------------------------------------------------------------------

const uint32_t MAX_BLOOM_MIPS = 6;

// ---------------------------------------------------------------------------
// Thumbnail renderer (editor only)
// ---------------------------------------------------------------------------

#ifdef EDITOR_BUILD
class ThumbnailRenderer
{
public:
	ThumbnailRenderer(int size);
	void render(Model* m, MaterialInstance* override_mat);
	// Renders multiple placed models (e.g. every MeshComponent in a prefab) into one
	// thumbnail, framing the camera around their combined world-space bounds.
	void render_multi(const std::vector<ThumbnailRenderItem>& items);
	void output_to_path(std::string path);

private:
	IGraphicsTexture* color{};
	IGraphicsTexture* depth{};

	Texture* vts_handle = nullptr;
	// Pool of persistent scene proxies used only by render_multi(), grown on demand to cover
	// the largest prefab thumbnailed so far and reused (not freed) by later, smaller ones.
	// Kept entirely separate from `object` below -- render() assumes its slot's transform is
	// always identity, so it must never be reused as one of render_multi()'s placed items.
	std::vector<handle<Render_Object>> multi_objects;
	handle<Render_Object> object; // used by the single-model render()
	Render_Pass pass;
	Render_Lists list;
	int size = 0;

	void draw_and_output(const glm::vec3& center, float radius, bool tight_margin);
};
#endif

// ---------------------------------------------------------------------------
// Decal batching
// ---------------------------------------------------------------------------

class DecalBatcher
{
public:
	DecalBatcher();
	void build_batches();
	void draw_decals();

private:
	struct DecalObj
	{
		int orig_index = 0;
		int program = 0;
		int texture_set = 0;
		int sort_order = 0;

		MaterialInstance* the_material = nullptr;
	};
	struct DecalDraw
	{
		int count = 0;
		MaterialInstance* shared_pipeline_material = nullptr;
		program_handle the_program_to_use = 0;
	};

	std::vector<DecalDraw> draws;
	IGraphicsBuffer* multidraw_commands = nullptr;
	IGraphicsBuffer* indirection_buffer = nullptr;
};

// ---------------------------------------------------------------------------
// Light list culler (tiled)
// ---------------------------------------------------------------------------

class LightListCuller
{
public:
	LightListCuller();
	void cull(const View_Setup& setup);
	void draw_lights();
	const std::vector<int>& get_counts() { return counts; }

private:
	std::vector<int> counts;
	IGraphicsBuffer* tiled_uniforms = nullptr;
	IGraphicsBuffer* light_count_buffer = nullptr;
	IGraphicsBuffer* light_indirection = nullptr;
};

// ---------------------------------------------------------------------------
// Shared render target sanity helpers
// ---------------------------------------------------------------------------

class SharedRenderTargetTexture;
class SharedRenderTargetOwner
{
public:
	IGraphicsTexture*& get_ptr_ref_for_setting() {
		ASSERT(!is_locked);
		return ptr;
	}
	IGraphicsTexture* get_for_reading(SharedRenderTargetTexture* t) {
		ASSERT(t == is_locked);
		return ptr;
	}
	IGraphicsTexture* aquire_lock_to_write(SharedRenderTargetTexture* t) {
		ASSERT(!is_locked);
		is_locked = t;
		return ptr;
	}
	void release_lock(SharedRenderTargetTexture* t) {
		ASSERT(is_locked == t);
		is_locked = nullptr;
	}

private:
	IGraphicsTexture* ptr = nullptr;
	SharedRenderTargetTexture* is_locked = nullptr;
};

class SharedRenderTargetTexture
{
public:
	void init(SharedRenderTargetOwner* p) {
		ASSERT(!parent);
		this->parent = p;
	}
	IGraphicsTexture* get_for_reading() { return parent->get_for_reading(this); }
	IGraphicsTexture* aquire_lock_to_write() { return parent->aquire_lock_to_write(this); }
	void release_lock() { parent->release_lock(this); }

private:
	SharedRenderTargetOwner* parent = nullptr;
};

// ---------------------------------------------------------------------------
// Debugging texture output
// ---------------------------------------------------------------------------

class DebuggingTextureOutput
{
public:
	void draw_out();

	Texture* output_tex = nullptr;
	float alpha = 1.0;
	float mip = 0.0;
	float scale = 1.0;
	bool explicit_texel = false;
};
