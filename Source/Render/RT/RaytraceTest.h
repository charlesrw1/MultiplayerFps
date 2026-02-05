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
	void render();

	IGraphicsBuffer* verts = nullptr;
	IGraphicsBuffer* indicies = nullptr;
	IGraphicsBuffer* nodes = nullptr;
	IGraphicsBuffer* references = nullptr;
	IGraphicsBuffer* materials = nullptr;


	IGraphicsBuffer* ray_buffer = nullptr;

	IGraphicsTexture* probe_irradiance = nullptr;
	IGraphicsTexture* probe_depth = nullptr;

	program_handle raytrace_test{};

	program_handle trace_shader{};
	program_handle gather_shader{};
};