#pragma once
// CPU-fast batching, GPU culling inputs, and the BuildSceneData_CpuFast manager.

#include "Render/DrawTypedefs.h"
#include "Render/MaterialLocal.h"
#include "Render/RenderLevelParams.h" // Render_lists_cpufast, Render_Level_Params

// shared GPU types
#include "../Shaders/SharedGpuTypes.txt"

#include <vector>
#include <unordered_map>
#include <span>
#include <cstdint>

class Model;
class MaterialInstance;
class MasterMaterialImpl;
class IGraphicsBuffer;
struct Frustum;

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

inline size_t hash_combine(size_t a, size_t b) {
	return a ^ (b + 0x9e3779b9 + (a << 6) + (a >> 2));
}

struct ModelAndMatTextureSet
{
	Model* m{};
	// for mat overrides...
	MasterMaterialImpl* parent{};
	MaterialInstance* has_textures{};
	uint32_t texture_hash{};

	bool operator==(const ModelAndMatTextureSet& other) const {
		return m == other.m && parent == other.parent && texture_hash == other.texture_hash;
	}
};
struct ModelAndMatTextureSetHasher
{
	size_t operator()(const ModelAndMatTextureSet& k) const noexcept {
		size_t h1 = std::hash<Model*>{}(k.m);
		size_t h2 = std::hash<MasterMaterialImpl*>{}(k.parent);
		size_t h3 = k.texture_hash;
		return hash_combine(hash_combine(h2, h3), h1);
	}
};

struct ModelAndMatTData
{
	Model* m{};
	std::vector<int> part_to_draw_cmd; // total num_parts (inc lods etc)
	int instance_count = 0;
	int instance_alloced = 0;
	int16_t ptr_ofs = 0;
	int gpu_buf_ofs = 0;
};

struct MaterialAndShader_CpuFast
{
	MaterialInstance* mat = nullptr;
	draw_call_key key{};
};
struct Render_Pass_CpuFast
{
public:
	std::vector<Multidraw_Batch> batches;
};
struct Render_List_CpuFast
{
	std::vector<int> glinstances;
	std::vector<gpu::DrawElementsIndirectCommand> out_cmds;
	std::vector<MaterialAndShader_CpuFast> batch_to_material;
};

struct GpuCullInput
{
	IGraphicsBuffer* mod_data{};	   // model data buffer. contains lods, parts, cmd indidices, material offsets
	IGraphicsBuffer* obj_data_buf{};   // CullObject[]
	IGraphicsBuffer* count_buf{};	   // count buffer to use with drawelementsindirectcount
	IGraphicsBuffer* batches_buf{};	   // array of multidraw_batches
	IGraphicsBuffer* glinst_to_inst{}; // int[], used for indirecting to object transform etc data
	IGraphicsBuffer* cmd_buf{};		   // drawelementsindirectcommands[]
	IGraphicsBuffer* draw_to_batch{};  // int[] mapping from cmd_buf to batches_buf

	int num_batches = 0;
	int num_cmds = 0;
	int num_objs = 0;
};

// ---------------------------------------------------------------------------
// BuildSceneData_CpuFast — builds and maintains GPU-ready scene draw data
// ---------------------------------------------------------------------------

class BuildSceneData_CpuFast
{
public:
	static BuildSceneData_CpuFast* inst;

	BuildSceneData_CpuFast();

	// now
	void build_scene_data(bool cubemap_view, bool skybox_only);

	// e2e func, fixme
	void cull_and_draw_shadow_cascade(int idx);
	void cull_and_draw_shadow_spot(const Frustum& f);

	void make_shadow_object_data_threadsafe(std::span<uint8_t> vis, std::span<int> glinst,
											std::span<gpu::DrawElementsIndirectCommand> outcmds,
											std::span<int> mdcounts) const;

	void on_fastpath_material_removed(MaterialInstance* mat);
	void on_model_removed(Model* m);
	void rebuild_models() {
		sys_print(Warning, "force rebuild models flag set\n");
		force_rebuild = true;
	}

	int16_t get_index(Model* m, MaterialInstance* mat) {
		if (!m)
			return -1;
		ModelAndMatTextureSet search;
		search.m = m;
		if (mat && mat->impl) {
			search.parent = mat->impl->get_master_impl();
			auto parent_texhash = search.parent->self->impl->get_texture_id_hash();
			auto myhash = mat->impl->get_texture_id_hash();
			if (parent_texhash == myhash) {
				search.texture_hash = parent_texhash;
				search.has_textures = search.parent->self;
			} else {
				search.texture_hash = myhash;
				search.has_textures = mat;
			}
			search.has_textures->impl->used_in_fastpath_cache = true;
		}
		auto find = mod_data.find(search);
		if (find != mod_data.end()) {
			return find->second.ptr_ofs;
		}
		ModelAndMatTData data;
		data.ptr_ofs = (int)mod_data_ptrs.size();
		data.m = m;

		mod_data[search] = data;
		mod_data_ptrs.push_back(&mod_data[search]);

		return data.ptr_ofs;
	}

	inline bool is_modptr_index_in_fast_path(int16_t fast_index) const {
		if (fast_index < 0)
			return false;
		return mod_data_ptrs[fast_index]->instance_alloced > 0;
	}

	GpuCullInput get_cull_input() const;
	GpuCullInput get_cull_input_shadow() const;

	void do_gbuffer_draw(bool overdraw_visualization_2nd_pass);
	void do_shadow_draw(float polyfac, bool lessthan);

	int get_num_commands() const { return out_cmds.size(); }
	int get_num_instances() const { return gbuffer_list.glinstances.size(); }
	int get_num_shadow_batches() const { return shadow_pass.batches.size(); }
	int get_num_depth_batches() { return shadow_pass.batches.size(); }
	int get_num_opaque_batches() { return gbuffer_pass.batches.size(); }
	int get_num_cached_cmds() { return out_cmds.size(); }
	int get_num_cached_mod_mats() { return mod_data_ptrs.size(); }

private:
	bool force_rebuild = false;
	enum DoDrawFlags
	{
		IS_SHADOW = 1,
		DEPTH_LESSTHAN = 2,
		OVERDRAWVIS = 4,
	};

	void do_draw_shared(int flags, float polyfac);
	void rebuild_mod_data();
	void rebuild_batches();
	void upload_gpu_cmds(int sum_count);

	// sorted specially for shadows
	Render_Pass_CpuFast shadow_pass;
	// sorted for opaques
	Render_Pass_CpuFast gbuffer_pass;

	Render_List_CpuFast gbuffer_list;
	// then have a list per cascades and spotlight and such
	Render_List_CpuFast shared_shadow_list;

	// mod data, updated when needed. must then update out_cmds
	std::unordered_map<ModelAndMatTextureSet, ModelAndMatTData, ModelAndMatTextureSetHasher> mod_data;
	std::vector<ModelAndMatTData*> mod_data_ptrs;

	struct
	{
		IGraphicsBuffer* mod_data_gpu = nullptr;
		IGraphicsBuffer* shadow_batches = nullptr;
		IGraphicsBuffer* gbuffer_batches = nullptr;
		IGraphicsBuffer* gbuffer_count = nullptr;
		IGraphicsBuffer* shadows_count = nullptr;
		IGraphicsBuffer* gbuffer_draw_to_batch = nullptr;
		IGraphicsBuffer* shadow_draw_to_batch = nullptr;
		IGraphicsBuffer* glinst_to_inst = nullptr;
		IGraphicsBuffer* cmd_list = nullptr;

		IGraphicsBuffer* cullobj_buf = nullptr;
		int num_cullobjs = 0;
	} gpu;

	// sorted draw cmds, indexed into by M&MTS
	std::vector<gpu::DrawElementsIndirectCommand> out_cmds;
	std::vector<int16_t> cmd_to_mod_data_ptr;
	struct CmdExtraData
	{
		Model* model{};
		MaterialInstance* material{};
		int submesh{};
		draw_call_key key{};
	};
	std::vector<CmdExtraData> cmd_to_extra;
};
