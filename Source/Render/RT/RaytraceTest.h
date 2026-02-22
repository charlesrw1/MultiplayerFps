#pragma once
#include "Render/DrawTypedefs.h"
#include "../IGraphsDevice.h"

#include "Framework/BVH.h"

struct GPUBVHNode
{
	glm::vec4 min;
	glm::vec4 max;
	int left_node;
	int count = -1;
	int padding1;
	int padding2;
};
struct GPUTriangle
{
	glm::vec4 v0;
	glm::vec4 v1;
	glm::vec4 v2;
};


struct RayBufferStruct {
	glm::vec4 dir_dist;
	glm::vec4 shading;
};

struct DdgiVolumeGpu {
	glm::vec4 origin_priority;
	glm::vec4 density;
	glm::ivec4 size_offset;
	int get_num_probes_total() const {
		return size_offset.x * size_offset.y * size_offset.z;
	}
};
struct DdgiGlobals {
	int atlas_x;
	int atlas_y;
	float normal_bias;
	float view_bias;

	int num_volumes=0;
	float relocate_normal_dist;
	int padding[2];
};
class DdgiTesting
{
public:
	DdgiTesting();
	~DdgiTesting();
	
	// editor side
	void build_world();
	void execute();
	void calculate_lum_for_spec();

	// scene load
	void load_the_gi(
		IGraphicsTexture* irrad,
		IGraphicsTexture* depth,
		std::vector<glm::vec4>& relocate,
		std::vector<DdgiVolumeGpu>& vols
	);

	// main func
	void draw_lighting(IGraphicsTexture* ssao, bool for_cubemap_view);
	// debug funcs
	void render_rt();
	void render_probes();

	glm::ivec3 selected_probe{};

	std::vector<DdgiVolumeGpu> myvolumes;
	std::vector<glm::vec4> temp_probe_relocate_thing;
	DdgiGlobals theglobals;

	IGraphicsBuffer* ddgi_globals = nullptr;
	IGraphicsBuffer* ddgi_volumes = nullptr;
	IGraphicsBuffer* ddgi_probe_relocation_offsets = nullptr;
	IGraphicsBuffer* ddgi_probe_avg_value = nullptr;

	IGraphicsTexture* probe_irradiance = nullptr;
	IGraphicsTexture* probe_depth = nullptr;

private:
	void compute_avg_probe_value();

	// for editor builds
	IGraphicsBuffer* verts = nullptr;
	IGraphicsBuffer* indicies = nullptr;
	IGraphicsBuffer* nodes = nullptr;
	IGraphicsBuffer* references = nullptr;
	IGraphicsBuffer* materials = nullptr;

	void create_textures_raybuffer(int w, int h);

	program_handle raytrace_test{};
	program_handle debug_probes{};
	program_handle trace_shader{};
	program_handle gather_shader{};
	program_handle relocate_shader{};
	program_handle lum_calc{};
	program_handle get_best_cubemap_shader{};
	program_handle shade_fs{};
	program_handle shade_debug_fs{};

	program_handle avg_probe_calc{};

};