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
class DdgiTesting
{
public:
	DdgiTesting();
	~DdgiTesting();
	void build_world();
	void execute();
	void render_rt();
	void render_probes();
	void draw_lighting(IGraphicsTexture* ssao, bool for_cubemap_view);
	glm::ivec3 selected_probe{};

	IGraphicsBuffer* verts = nullptr;
	IGraphicsBuffer* indicies = nullptr;
	IGraphicsBuffer* nodes = nullptr;
	IGraphicsBuffer* references = nullptr;
	IGraphicsBuffer* materials = nullptr;

	IGraphicsBuffer* probe_to_best_cubemap = nullptr;

	IGraphicsBuffer* ray_buffer = nullptr;

	IGraphicsTexture* probe_irradiance = nullptr;
	IGraphicsTexture* probe_depth = nullptr;
	IGraphicsBuffer* buf = nullptr;
	program_handle raytrace_test{};
	program_handle debug_probes{};
	program_handle trace_shader{};
	program_handle gather_shader{};

	IGraphicsBuffer* indirection{};


	program_handle get_best_cubemap_shader{};


	program_handle shade_fs{};
	program_handle shade_debug_fs{};
};