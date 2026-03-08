#pragma once
#include "DrawLocal.h"
#include "Render/Frustum.h"
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

#########
# STEPS #
#########

1. pass 1 
	frustum+occlusion cull objects with LAST frame HI-Z
	ouput MDI commands
	output a bitarray per object if it was drawn
2. draw this MDI buffer
3. build Hi-Z for this frame
4. pass 2
	occlusion cull objects that werent drawn (from bitarray)
	output MDI 
4. draw 2nd MDI buffer



*/
struct CullObject {
	glm::vec4 bounds_sphere;
	glm::ivec4 model_ofs;
	// x component is model
	// y component is object index
	// z component is material ofs
	// w component is flags
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
	int cpu_obj_offset;

	mat4 view;

	float cascade_extent;
	int padding[3];
};

struct GpuCullInput {
	IGraphicsBuffer* mod_data{};	// model data buffer. contains lods, parts, cmd indidices, material offsets
	IGraphicsBuffer* obj_data_buf{};	// CullObject[]
	IGraphicsBuffer* count_buf{};		// count buffer to use with drawelementsindirectcount
	IGraphicsBuffer* batches_buf{};	// array of multidraw_batches
	IGraphicsBuffer* glinst_to_inst{};	// int[], used for indirecting to object transform etc data
	IGraphicsBuffer* cmd_buf{};			// drawelementsindirectcommands[] 
	IGraphicsBuffer* draw_to_batch{};	// int[] mapping from cmd_buf to batches_buf

	int num_batches = 0;
	int num_cmds = 0;
	int num_objs = 0;
};
struct ShadowCullInput {
	ShadowCullInput() = default;
	ShadowCullInput(Frustum f, bool cascade, float fov, glm::vec4 backplane, glm::vec3 origin)
		: f(f), cascade(cascade), fov(fov), backplane(backplane), origin(origin) {}
	Frustum f;
	bool cascade = true;
	float fov;
	glm::vec4 backplane;
	glm::vec3 origin;
};
class GpuCullingTest {
public:
	static GpuCullingTest* inst;

	GpuCullingTest();
	~GpuCullingTest();


	// builds mod info etc
	// culls against prev frustum
	void build_data(const GpuCullInput& input);

	// copies culled data to render lists
	void copy_cpu(Render_Lists_Gpu_Culled& list);
	
	// build hi-z and do 2nd cull pass
	void build_data_2(const GpuCullInput& input);

	// # copy_cpu() again for cpu objects

	// call this twice
	void dodraw();


	void debug_overlay();
	void init_depth_pyramid(int w, int h);
	void downsample_depth();


	void compact_draws(
		const GpuCullInput& input
	);

	program_handle cull_compute{};
	program_handle cull_compute_cascade{};

	program_handle compaction{};
	program_handle debug_overlays{};


	IGraphicsBuffer* matindirect = nullptr;

	// used for both cpu and gpu objs. big buffers
	IGraphicsBuffer* vis_bitarray = nullptr;
	IGraphicsBuffer* cull_data = nullptr;


	CullData cull{};
	//std::vector<const MaterialInstance*> cmd_mats;



	program_handle build_pyramid{};
	IGraphicsTexture* depth_pyramid = nullptr;
	glm::ivec2 depth_size = {};
	glm::ivec2 actual_depth_size = {};


	uint32 hiZSampler{};

	glm::mat4 prev_view = glm::mat4(1.0);

	program_handle zero_instances_mdi{};
	program_handle cpu_vis_array_to_mdi{};

	void zero_instances_in_this(bufferhandle mdi_buf, int count);

	void do_shadow_cull(const GpuCullInput& input, Frustum f);
private:
	enum class Phase {
		Pass1,
		Pass2
	};
	void do_cull_for_scene(const GpuCullInput& input, Phase pass);
	void do_cull(const GpuCullInput& input, Phase pass, bool is_for_shadow, Frustum in_frustum);
};