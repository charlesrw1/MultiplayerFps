#include "DrawLocal.h"
#include "Framework/Util.h"
#include "Render/Texture.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include "Assets/AssetDatabase.h"
#include "Render/ModelManager.h"
#include "tracy/public/tracy/Tracy.hpp"
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"

// -----------------------------------------------------------------------
// BuildSceneData_CpuFast – batch rebuilding, GPU upload, and draw dispatch
// -----------------------------------------------------------------------

void BuildSceneData_CpuFast::rebuild_batches() {
	ASSERT(!out_cmds.empty() || true); // invariant: arrays are consistently sized

	auto make_batches = [&](std::vector<Multidraw_Batch>& batches, const bool is_depth_pass) {
		batches.clear();

		if (out_cmds.empty())
			return;

		Multidraw_Batch batch;
		batch.first = 0;
		batch.count = 1;

		const Model* batch_model = cmd_to_extra.at(0).model;
		auto batch_sort_key = cmd_to_extra.at(0).key;

		for (int i = 1; i < (int)out_cmds.size(); i++) {

			const Model* this_model = cmd_to_extra.at(i).model;
			auto this_sort_key = cmd_to_extra.at(i).key;

			bool batch_this = false;

			bool same_layer = batch_sort_key.layer == this_sort_key.layer;
			bool same_vao = batch_sort_key.vao == this_sort_key.vao;
			bool same_material = batch_sort_key.texture == this_sort_key.texture;
			bool same_shader = batch_sort_key.shader == this_sort_key.shader;
			bool same_other_state =
				batch_sort_key.blending == this_sort_key.blending && batch_sort_key.backface == this_sort_key.backface;

			if (!is_depth_pass) {
				if (same_vao && same_material && same_other_state && same_shader && same_layer)
					batch_this = true;
				else
					batch_this = false;

			} else { // pass==DEPTH
				// can batch across texture changes as long as its not alpha tested
				if (same_shader && same_vao && same_other_state)
					batch_this = true;
				else
					batch_this = false;
			}

			if (batch_this) {
				batch.count += 1;
			} else {
				batches.push_back(batch);
				batch.count = 1;
				batch.first = i;

				batch_model = this_model;
				batch_sort_key = this_sort_key;
			}
		}

		batches.push_back(batch);
	};

	make_batches(gbuffer_pass.batches, false);
	make_batches(shadow_pass.batches, true);
}

void BuildSceneData_CpuFast::upload_gpu_cmds(int sum_count) {
	ASSERT(sum_count >= 0);

	const int command_bytes_size = (int)out_cmds.size() * sizeof(gpu::DrawElementsIndirectCommand);
	gpu.cmd_list->upload(nullptr, command_bytes_size * 2);
	gpu.cmd_list->sub_upload(out_cmds.data(), command_bytes_size, 0);

	gpu.glinst_to_inst->upload(nullptr, sum_count * sizeof(int) * 2); // *2 because materials stored with instances
}

void setup_batch2(const MaterialInstance* mat, const int offset, bool is_depth, bool depth_less_than_op,
				  bool force_backface, Model* m, bool overdraw_vis) {
	ASSERT(mat != nullptr);
	ASSERT(m != nullptr);

	auto flags = (is_depth) ? MSF_DEPTH_ONLY : 0;
	flags |= MSF_MATERIAL_IN_INSTANCE;

	if (is_depth)
		flags |= MSF_NO_TAA;
	if (r_debug_mode.get_integer() != 0)
		flags |= MSF_DEBUG;
	flags |= MSF_EDITOR_ID;
	if (m->has_bones())
		flags |= MSF_ANIMATED;

	const program_handle program = matman.get_mat_shader(nullptr, mat, flags);
	auto master = mat->get_master_material();
	BlendState blend = master->blend;
	const bool show_backface = master->backface;

	VaoType type = VaoType::Lightmapped;
	if (m->has_bones())
		type = VaoType::Animated;
	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(type);

	bool depth_tests = true;
	if (overdraw_vis) {
		blend = BlendState::ADD;
		depth_tests = false;
	}

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface && !force_backface;
	state.blend = blend;
	state.depth_testing = depth_tests;
	state.depth_writes = !master->is_translucent();
	state.depth_less_than = depth_less_than_op;

	draw.get_device().set_pipeline(state);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < (int)textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = 0;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}

void BuildSceneData_CpuFast::do_draw_shared(int flags, float poly_factor) {
	ASSERT(gpu.num_cullobjs >= 0);

	if (gpu.num_cullobjs <= 0)
		return;

	if (flags & IS_SHADOW) {
		gfx().set_polygon_offset(true, poly_factor, 4 /* this does nothing?*/);
	}

	auto do_draw_internal = [&](std::vector<Multidraw_Batch>& batches, const bool is_depth) {
		bool force_backface = false;
		bool want_less_than = false;

		if (is_depth) {
			force_backface = true;
			want_less_than = bool(flags & DEPTH_LESSTHAN);
		}

		IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();
		auto& scene = draw.scene;
		gfx().bind_storage_buffer_base(2, scene.gpu_instance_buffer);
		gfx().bind_storage_buffer_base(3, scene.gpu_skinned_mats_buffer);
		gfx().bind_storage_buffer_base(4, material_buffer);
		gfx().bind_storage_buffer_base(5, gpu.glinst_to_inst);

		const int command_size = (int)out_cmds.size() * sizeof(gpu::DrawElementsIndirectCommand);
		gfx().bind_parameter_buffer(gpu.gbuffer_count);
		gfx().bind_indirect_buffer(gpu.cmd_list);

		const int offset_buffer_start = command_size;
		int offset = 0;
		const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
		for (int i = 0; i < (int)batches.size(); i++) {
			const int count = batches.at(i).count;
			const int mat_ofs = batches.at(i).first;
			const int incr = count;
			if (count != 0) {

				setup_batch2(cmd_to_extra.at(mat_ofs).material, offset, is_depth, want_less_than, force_backface,
							 cmd_to_extra.at(mat_ofs).model, flags & OVERDRAWVIS);

				const int indirect_byte_offset = offset_buffer_start + offset * DEIcmdSz;
				gfx().multi_draw_elements_indirect_count(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					indirect_byte_offset, i * (int)sizeof(uint32), count,
					sizeof(gpu::DrawElementsIndirectCommand));

				draw.stats.total_draw_calls += 1;
			}
			offset += incr;
		}
		gfx().bind_indirect_buffer(nullptr);
	};

	if (flags & IS_SHADOW)
		do_draw_internal(shadow_pass.batches, true);
	else
		do_draw_internal(gbuffer_pass.batches, false);

	gfx().set_polygon_offset(false, 0, 0);
}

void BuildSceneData_CpuFast::do_shadow_draw(float factor, bool less_than) {
	ASSERT(gpu.num_cullobjs >= 0);

	int flags = IS_SHADOW;
	if (less_than)
		flags |= DEPTH_LESSTHAN;
	do_draw_shared(flags, factor);
}

GpuCullInput BuildSceneData_CpuFast::get_cull_input() const {
	ASSERT(gpu.cmd_list != nullptr);

	GpuCullInput input;
	input.batches_buf = gpu.gbuffer_batches;
	input.cmd_buf = gpu.cmd_list;
	input.count_buf = gpu.gbuffer_count;
	input.draw_to_batch = gpu.gbuffer_draw_to_batch;
	input.glinst_to_inst = gpu.glinst_to_inst;
	input.mod_data = gpu.mod_data_gpu;
	input.num_batches = gbuffer_pass.batches.size();
	input.num_cmds = out_cmds.size();
	input.obj_data_buf = gpu.cullobj_buf;
	input.num_objs = gpu.num_cullobjs;
	return input;
}

GpuCullInput BuildSceneData_CpuFast::get_cull_input_shadow() const {
	ASSERT(gpu.shadow_batches != nullptr);

	GpuCullInput input = get_cull_input();
	input.batches_buf = gpu.shadow_batches;
	input.draw_to_batch = gpu.shadow_draw_to_batch;
	input.num_batches = shadow_pass.batches.size();

	return input;
}

void BuildSceneData_CpuFast::do_gbuffer_draw(bool overdraw_visualization_2nd_pass) {
	ASSERT(gpu.num_cullobjs >= 0);

	int flags = 0;
	if (overdraw_visualization_2nd_pass)
		flags |= OVERDRAWVIS;
	do_draw_shared(flags, 0);
}

void BuildSceneData_CpuFast::rebuild_mod_data() {
	ZoneScopedN("BuildSceneData_CpuFast::rebuild_mod_data");
	ASSERT(BuildSceneData_CpuFast::inst != nullptr);

	ArenaScope scope(draw.mem_arena);

	out_cmds.clear();
	cmd_to_mod_data_ptr.clear();
	cmd_to_extra.clear();

	gbuffer_pass.batches.clear();
	shadow_pass.batches.clear();

	auto make_key = [&](MaterialInstance* this_mat, Model* this_model) -> draw_call_key {
		draw_call_key k{};
		if (!this_mat)
			return k;
		auto parent = this_mat->impl->get_master_impl();
		if (!parent)
			return k;

		k.backface = parent->backface;
		k.blending = (uint64_t)parent->blend;
		k.mesh = this_model->get_uid();
		k.texture = this_mat->impl->get_texture_id_hash();
		int flags = 0;
		if (this_model->has_bones())
			flags |= MSF_ANIMATED;
		k.shader = matman.get_mat_shader(nullptr, this_mat, flags);
		return k;
	};

	arena_vec<int> mod_data_gpu_buf(scope);
	mod_data_gpu_buf.reserve(10'000);

	for (auto& [key, md] : mod_data) {
		auto m = key.m;

		const int bufstart = (int)mod_data_gpu_buf.size();
		md.gpu_buf_ofs = bufstart;

		mod_data_gpu_buf.push_back(m->get_num_lods());
		for (int lodi = 0; lodi < m->get_num_lods(); lodi++) {
			auto& lod = m->get_lod(lodi);
			mod_data_gpu_buf.push_back(lod.part_ofs);
			mod_data_gpu_buf.push_back(lod.part_count);
			const float f = lod.end_percentage;
			mod_data_gpu_buf.push_back(*((int*)&f));
		}

		const int num_parts = key.m->get_num_parts();
		md.part_to_draw_cmd.clear();
		for (int parti = 0; parti < num_parts; parti++) {
			auto& part = key.m->get_part(parti);
			MaterialInstance* mati = (MaterialInstance*)key.m->get_material_for_part(part);
			if (key.has_textures)
				mati = key.has_textures;
			if (!mati || !mati->impl)
				mati = matman.get_fallback();

			gpu::DrawElementsIndirectCommand cmd{};
			cmd.baseVertex = part.base_vertex + m->get_merged_vertex_ofs();
			cmd.count = part.element_count;
			cmd.firstIndex = part.element_offset + m->get_merged_index_ptr();
			cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

			// Important! Set primCount to 0 because visible instances will increment this
			cmd.primCount = 0;
			cmd.baseInstance = 0;
			out_cmds.push_back(cmd);
			cmd_to_mod_data_ptr.push_back(md.ptr_ofs);
			cmd_to_extra.push_back({m, mati, parti, make_key(mati, m)});

			const int cmd_index = (int)out_cmds.size() - 1;
			const int data = (mati->impl->gpu_buffer_offset);

			md.part_to_draw_cmd.push_back(cmd_index);
			md.part_to_draw_cmd.push_back(data);

			mod_data_gpu_buf.push_back(cmd_index);
			mod_data_gpu_buf.push_back(data);
		}
	}

	// sort the commands
	struct IntAndKey
	{
		int i = 0;
		draw_call_key key{};
		int submesh_idx = 0;
	};

	std::vector<IntAndKey> sorted;

	for (int i = 0; i < (int)out_cmds.size(); i++) {
		sorted.push_back({i, cmd_to_extra[i].key, cmd_to_extra[i].submesh});
	}
	const auto& merge_functor = [](const IntAndKey& a, const IntAndKey& b) {
		if (a.key.as_uint64() < b.key.as_uint64())
			return true;
		else if (a.key.as_uint64() == b.key.as_uint64())
			return a.submesh_idx < b.submesh_idx;
		else
			return false;
	};

	std::sort(sorted.begin(), sorted.end(), merge_functor);

	const arena_vec<gpu::DrawElementsIndirectCommand> copied_cmds(out_cmds.begin(), out_cmds.end(), scope);
	for (int i = 0; i < (int)sorted.size(); i++)
		out_cmds[i] = copied_cmds[sorted[i].i];
	const arena_vec<CmdExtraData> copied_extra(cmd_to_extra.begin(), cmd_to_extra.end(), scope);
	for (int i = 0; i < (int)sorted.size(); i++)
		cmd_to_extra[i] = copied_extra[sorted[i].i];
	const arena_vec<int16_t> copied_ptr_i(cmd_to_mod_data_ptr.begin(), cmd_to_mod_data_ptr.end(), scope);
	for (int i = 0; i < (int)sorted.size(); i++)
		cmd_to_mod_data_ptr[i] = copied_ptr_i[sorted[i].i];

	arena_vec<int> inv_sorted((int)sorted.size(), scope);
	for (int i = 0; i < (int)sorted.size(); i++) {
		inv_sorted[sorted[i].i] = i;
	}

	// must adjust model index
	for (auto& [key, md] : mod_data) {
		const int num_parts = (int)md.part_to_draw_cmd.size() / 2;
		for (int parti = 0; parti < num_parts; parti++) {

			{
				const int index = parti * 2;
				const int cmd_ofs_prev = md.part_to_draw_cmd.at(index);
				const int remapped = inv_sorted.at(cmd_ofs_prev);
				md.part_to_draw_cmd.at(index) = remapped;
			}
			{
				const int index = md.gpu_buf_ofs + 1 + 3 * md.m->get_num_lods() + parti * 2;
				const int cmd_ofs_prev = mod_data_gpu_buf.at(index);
				const int remapped = inv_sorted.at(cmd_ofs_prev);
				mod_data_gpu_buf.at(index) = remapped;
			}
		}
	}

	rebuild_batches();

	// ##############
	// # GPU UPLOAD #
	// ##############
	gpu.mod_data_gpu->upload(mod_data_gpu_buf.data(), (int)mod_data_gpu_buf.size() * sizeof(int));
	gpu.gbuffer_batches->upload(gbuffer_pass.batches.data(), (int)gbuffer_pass.batches.size() * sizeof(Multidraw_Batch));
	gpu.shadow_batches->upload(shadow_pass.batches.data(), (int)shadow_pass.batches.size() * sizeof(Multidraw_Batch));
	gpu.gbuffer_count->upload(nullptr, sizeof(int) * (int)gbuffer_pass.batches.size());
	gpu.shadows_count->upload(nullptr, sizeof(int) * (int)shadow_pass.batches.size());

	// cmd upload is done with instances
	auto& gb = gbuffer_pass.batches;
	auto& sb = shadow_pass.batches;
	std::span<int> draw_to_batch = draw.mem_arena.alloc_bottom_span<int>((int)out_cmds.size());
	auto set_and_upload = [&](IGraphicsBuffer* buf, const std::vector<Multidraw_Batch>& mbv) {
		ASSERT(buf != nullptr);
		for (int i = 0; i < (int)mbv.size(); i++) {
			auto& b = mbv.at(i);
			for (int c = 0; c < b.count; c++) {
				ASSERT(b.first + c < (int)draw_to_batch.size());
				draw_to_batch[b.first + c] = i;
			}
		}
		buf->upload(draw_to_batch.data(), draw_to_batch.size_bytes());
	};
	set_and_upload(gpu.gbuffer_draw_to_batch, gb);
	set_and_upload(gpu.shadow_draw_to_batch, sb);
}
