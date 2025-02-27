#pragma once
#include "Render/DrawPublic.h"
#include "Framework/Config.h"
#include "glm/glm.hpp"
#include "Framework/Util.h"
#include "Shader.h"
#include "../Shaders/SharedGpuTypes.txt"
#include "DrawTypedefs.h"



class Volumetric_Fog_System
{
public:
	int quality = 2;

	glm::ivec3 voltexturesize;
	float density = 10.0;
	float anisotropy = 0.7;
	float spread = 1.0;
	float frustum_end = 50.f;
	int temporal_sequence = 0;

	struct buffers {
		bufferhandle light;
		bufferhandle param;
	}buffer;

	struct programs {
		Shader reproject;
		Shader lightcalc;
		Shader raymarch;
	}prog;

	struct textures {
		texhandle volume;
		texhandle last_volume;
	}texture;

	void init();
	void shutdown();
	void compute();
};

// shadowmap manager
// manages: 
// allocation of shadowmap
// when to update
// strategies:
// dont update if light is out of view frustum
// remove from allocation if further than max shadow distance
// if static, only update once

class SpotlightShadowManager
{
public:
	const int MAX_SHADOWS = 8;

	void init();
	void update();
	void remove_light(handle<Render_Light> handle) {
		for (int i = 0; i < slots_used.size(); i++) {
			if (slots_used[i].id == handle.id) {
				slots_used[i].id = -1;
				num_used--;
				return;
			}
		}
	}
	void make_render_targets();

	int find_index(handle<Render_Light> handle) {
		for (int i = 0; i < slots_used.size(); i++) {
			if (slots_used[i].id == handle.id) {
				return i;
			}
		}
		return -1;
	}

	struct textures {
		texhandle shadow_array{};
		Texture* shadow_vts_handle = nullptr;
	}tex;

	int num_used = 0;
	std::vector<handle<Render_Light>> slots_used;
};

class CascadeShadowMapSystem
{
public:
	void init();
	void update();
	void make_csm_rendertargets();
	void update_cascade(int idx, const View_Setup& vs, glm::vec3 directional_dir);

	const static int MAXCASCADES = 4;

	struct uniform_buffers {
		bufferhandle frame_view[4];
		bufferhandle info;
	}ubo;

	struct framebuffers {
		fbohandle shadow;
	}fbo;

	struct textures {
		texhandle shadow_array;

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
		float poly_factor = 1.1;
		float z_dist_scaling = 1.f;
		int quality = 2;
	}tweak;

	glm::vec4 split_distances;
	glm::mat4x4 matricies[MAXCASCADES];
	float nearplanes[MAXCASCADES];
	float farplanes[MAXCASCADES];
	int csm_resolution = 0;
	bool enabled = false;

	bool targets_dirty = false;
};

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
		texhandle random = 0;
		texhandle result = 0;
		texhandle blur = 0;
		texhandle viewnormal = 0;
		texhandle depthlinear = 0;
		texhandle deptharray = 0;
		texhandle resultarray = 0;
		texhandle depthview[RANDOM_ELEMENTS];

		Texture* result_vts_handle = nullptr;
		Texture* blur_vts_handle = nullptr;
		Texture* view_normal_vts_handle = nullptr;
		Texture* linear_depth_vts_handle = nullptr;
	}texture;

	struct uniform_buffers {
		bufferhandle data = 0;
	}ubo;

	struct params {
		float radius = 0.4;
		float intensity = 2.5;
		float bias = 0.1;
		float blur_sharpness = 3.0;
	}tweak;

	gpu::HBAOData data = {};
	glm::vec4 random_elements[RANDOM_ELEMENTS];
};

