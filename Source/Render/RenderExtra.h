#pragma once
#include "Render/DrawPublic.h"
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Shader.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "DrawTypedefs.h"
#include <unordered_map>


class Volumetric_Fog_System
{
public:
	int quality = 2;

	glm::ivec3 voltexturesize;
	float density = 10.0;

	float falloff = 0.5;
	float offset = 0.0;


	float anisotropy = 0.7;
	float spread = 1.0;
	float frustum_end = 50.f;
	int temporal_sequence = 0;

	struct buffers {
		bufferhandle param;
	}buffer;

	struct programs {
		program_handle reproject;
		program_handle lightcalc;
		program_handle raymarch;
	}prog;

	struct textures {
		texhandle volume;
		texhandle last_volume;
	}texture;

	void init();
	void shutdown();
	void compute();
};


// manages an atlas for spotlight shadows
// atlas size=1024x1024
// big shadow = 512x512
// medium = 256x256
// small = 128x128
// small shadows are half that
#include "Framework/Rect2d.h"
class Texture;
class IGraphicsTexture;
class ShadowMapAtlas {
public:
	ShadowMapAtlas();
	int allocate(int8_t size);
	void free(int handle);
	Rect2d get_atlas_rect(int handle);
	IGraphicsTexture* get_atlas_texture();
	glm::ivec2 get_size() {
		return atlas_size;
	}
	bool has_any_free() const {
		for (auto& r : rects) {
			if (!r.used) return true;
		}
		return false;
	}
	int total_rects() {
		return rects.size();
	}

private:
	struct Available {
		Rect2d rect;
		int8_t quality = 0;
		bool used = false;
	};
	std::vector<Available> rects;
	glm::ivec2 atlas_size = { 0,0 };
	IGraphicsTexture* atlas = nullptr;
	Texture* vtsHandle = nullptr;
};
#include "Render_Light.h"
// total abstraction garbage
struct Render_Lists;
class ShadowMapManager {
public:
	ShadowMapManager();
	void update();
	void get_lights_to_render(std::vector<handle<Render_Light>>& vec);
	void do_render(Render_Lists& list, handle<Render_Light> handle, bool any_dynamic_in_frustum);
	void on_remove_light(handle<Render_Light> h);
	ShadowMapAtlas& get_atlas() {
		return atlas;
	}
private:
	ShadowMapAtlas atlas;
	bufferhandle frame_view = 0;
};

// rect packed atlas for light cookies+ies
class LightCookieAtlas {
public:
	static const int ATLAS_WIDTH = 1024;
	static LightCookieAtlas* inst;
	LightCookieAtlas();

	void update();
	// normalized coords
	glm::vec4 get_rect_for_cookie(Texture* t);
	IGraphicsTexture* get_atlas() const { return atlas; }
private:
	std::unordered_map<Texture*, Rect2d> rects;
	IGraphicsTexture* atlas = nullptr;
	int atlasheight = 0;
};

class CascadeShadowMapSystem
{
public:
	void init();
	void update_matricies();
	void render_cascades();

	void make_csm_rendertargets();
	void update_cascade(int idx, const View_Setup& vs, glm::vec3 directional_dir);

	const static int MAXCASCADES = 4;
	const static int CASCADES_USED = 3;	// also change this in SunLightAccumulation.txt

	struct uniform_buffers {
		bufferhandle frame_view[4];
		bufferhandle info;
	}ubo;


	struct textures {
		IGraphicsTexture* shadow_array=nullptr;
		Texture* shadow_vts_handle = nullptr;
	}texture;

	struct params {
		bool cull_front_faces = false;
		bool fit_to_scene = true;
		bool reduce_shimmering = true;
		float log_lin_lerp_factor = 0.5;
		float max_shadow_dist = 80.f;
		float epsilon = 0.008f;
		float poly_units = 4;
		// = 1.1
		float poly_factor = -3.4;
		float z_dist_scaling = 1.f;
		int quality = 2;
	}tweak;

	glm::vec4 split_distances;
	glm::mat4x4 matricies[MAXCASCADES];
	float nearplanes[MAXCASCADES];
	float farplanes[MAXCASCADES];

	// max of x/y
	float max_extents[MAXCASCADES];

	int csm_resolution = 0;
	bool enabled = false;

	bool targets_dirty = false;
};

// uses old fbohandle and texhandle instead of being wrapped in IGraphicsTexture 
// Nvidia wrote the hbao code and im afraid to break it so just leaving it for now
class SSAO_System
{
public:
	void init();
	void reload_shaders();
	void make_render_targets(bool initial, int w, int h);
	void render();
	void update_ubo();

	int width = 0, height = 0;
	const static int RANDOM_ELEMENTS = 16;

	struct framebuffers {
		fbohandle depthlinear = 0;
		fbohandle viewnormal = 0;
		fbohandle hbao2_deinterleave = 0;
		fbohandle hbao2_calc = 0;
		fbohandle finalresolve = 0;
	}fbo;

	struct programs {
		program_handle hbao_calc;
		program_handle linearize_depth;
		program_handle make_viewspace_normals;
		program_handle hbao_blur;
		program_handle hbao_deinterleave;
		program_handle hbao_reinterleave;
	}prog;

	struct textures {
		IGraphicsTexture* result = nullptr;
		IGraphicsTexture* blurred = nullptr;

		texhandle random = 0;
		texhandle viewnormal = 0;
		texhandle depthlinear = 0;
		texhandle deptharray = 0;
		texhandle resultarray = 0;

		Texture* result_vts_handle = nullptr;
		Texture* blur_vts_handle = nullptr;
		Texture* view_normal_vts_handle = nullptr;
		Texture* linear_depth_vts_handle = nullptr;
	}texture;

	struct uniform_buffers {
		bufferhandle data = 0;
	}ubo;

	struct params {
		float radius = 0.3;
		float intensity = 1.5;
		float bias = 0.1;
		float blur_sharpness = 20.0;
	}tweak;

	gpu::HBAOData data = {};
	glm::vec4 random_elements[RANDOM_ELEMENTS];
};

