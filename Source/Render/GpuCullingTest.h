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
	int padding2;
	int padding1;
	vec4 camera_origin;
};
class GpuCullingTest {
public:
	static GpuCullingTest* inst;

	GpuCullingTest();
	~GpuCullingTest();

	void build_data();

	void dodraw();

	program_handle cull_compute{};
	program_handle compaction{};

	IGraphicsBuffer* object_buffer = nullptr;
	IGraphicsBuffer* model_data_buffer = nullptr;
	IGraphicsBuffer* multidraw_buffer = nullptr;

	IGraphicsBuffer* objindirect = nullptr;
	IGraphicsBuffer* matindirect = nullptr;

	IGraphicsBuffer* cull_data = nullptr;
	CullData cull{};
	std::vector<const MaterialInstance*> cmd_mats;

	std::vector<Multidraw_Batch> batches;

};