#pragma once
#include "DrawLocal.h"

/*
###########
# OUTLINE #
###########

DATA STRUCTURES:
struct CullObject
	vec4 transformed_bounds
	int model_offset
CullObject object_buffer[]

int8 model_data_buffer[]
format:	
	int num_lods
	lods[]
		int part_ofs, int part_count, float screen_size
	parts[]
		int mat, int mdi_offset, int flags

STEPS:
1. make the object buffer and model data buffer on cpu
2. make fully expanded MDI commands on cpu (sorted)
3. execute culling for main view, shadows, etc
4. now multidraw_buffer can be bound and called
	



*/
struct CullObject {
	glm::vec4 bounds_sphere;
	glm::ivec4 model_ofs;
};
struct CullData {
	vec4 frustum_up;
	vec4 frustum_down;
	vec4 frustum_l;
	vec4 frustum_r;
	int num_objects;
	float inv_two_times_tanfov_2;
	float p00;
	float p11;

	vec4 camera_origin;

	float near;
	int pyramid_width;
	int pyramid_height;
	int padding2;

	mat4 view;
};
class GpuCullingTest {
public:
	static GpuCullingTest* inst;

	GpuCullingTest();
	~GpuCullingTest();

	void build_data();

	void dodraw();
	void debug_overlay();
	void init_depth_pyramid(int w, int h);
	void downsample_depth();

	program_handle cull_compute{};
	program_handle compaction{};
	program_handle debug_overlays{};
	IGraphicsBuffer* object_buffer = nullptr;
	IGraphicsBuffer* model_data_buffer = nullptr;
	IGraphicsBuffer* multidraw_buffer = nullptr;

	IGraphicsBuffer* objindirect = nullptr;
	IGraphicsBuffer* matindirect = nullptr;

	IGraphicsBuffer* cull_data = nullptr;
	CullData cull{};
	std::vector<const MaterialInstance*> cmd_mats;

	std::vector<Multidraw_Batch> batches;

	program_handle build_pyramid{};
	IGraphicsTexture* depth_pyramid = nullptr;
	glm::ivec2 depth_size = {};

	uint32 hiZSampler{};

	glm::mat4 prev_view = glm::mat4(1.0);

};