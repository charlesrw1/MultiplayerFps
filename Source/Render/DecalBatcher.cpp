#include "DrawLocal.h"
#include "Framework/ArenaAllocator.h"
#include "Render/Model.h"
#include "Render/ModelManager.h"
#include <algorithm>

extern ConfigVar r_drawdecals;
extern const GLenum MODEL_INDEX_TYPE_GL;

DecalBatcher::DecalBatcher() {
	CreateBufferArgs args;
	args.flags = GraphicsBufferUseFlags::BUFFER_USE_DYNAMIC;
	indirection_buffer = IGraphicsDevice::inst->create_buffer(args);
	multidraw_commands = IGraphicsDevice::inst->create_buffer(args);
}

void DecalBatcher::build_batches() {
	draws.clear();

	Model* the_model = Model::load("eng/cube.cmdl");

	Memory_Arena& arena = draw.get_arena();
	ArenaScope memScope(arena);
	const int num_decals = draw.scene.decal_list.objects.size();
	std::span<DecalObj> decal_objs = arena.alloc_bottom_span<DecalObj>(num_decals);
	int actual_count = 0;
	for (int i = 0; i < decal_objs.size(); i++) {
		auto& decal_obj = draw.scene.decal_list.objects.at(i).type_.decal;
		MaterialInstance* mat = (MaterialInstance*)decal_obj.material;
		if (!decal_obj.visible || !mat || mat->impl->get_master_impl()->usage != MaterialUsage::Decal)
			continue;
		decal_objs[actual_count].orig_index = i;
		decal_objs[actual_count].the_material = mat;
		program_handle the_shader = matman.get_mat_shader(the_model, mat, 0);
		decal_objs[actual_count].program = the_shader;
		decal_objs[actual_count].texture_set = mat->impl->texture_id_hash;
		actual_count += 1;
	}
	decal_objs = std::span<DecalObj>(decal_objs.data(), actual_count);
	if (decal_objs.empty())
		return;

	std::sort(decal_objs.begin(), decal_objs.end(), [](const DecalObj& a, const DecalObj& b) {
		if (a.program != b.program)
			return a.program < b.program;
		return a.texture_set < b.texture_set;
		});


	auto make_indirection_data = [&]() {
		std::span<int> indirection_data = arena.alloc_bottom_span<int>(actual_count);

		DecalDraw cur_draw;
		DecalObj* current_batching = nullptr;
		for (int i = 0; i < decal_objs.size(); i++) {
			DecalObj* self = &decal_objs[i];
			indirection_data[i] = self->orig_index;
			if (!current_batching || current_batching->program != self->program || current_batching->texture_set != self->texture_set) {
				if (cur_draw.count != 0) {
					draws.push_back(cur_draw);
					cur_draw = DecalDraw();
				}
				cur_draw.count = 1;
				cur_draw.the_program_to_use = self->program;
				cur_draw.shared_pipeline_material = self->the_material;
				current_batching = self;
			}
			else {
				cur_draw.count += 1;
			}
		}
		if (cur_draw.count != 0) {
			draws.push_back(cur_draw);
			cur_draw = DecalDraw();
		}
		indirection_buffer->upload(indirection_data.data(), indirection_data.size_bytes());
	};
	make_indirection_data();


	// multidraw buffer
	auto make_multidraw_buffer = [&]() {
		auto& part = the_model->get_part(0);
		using DEIcmd = gpu::DrawElementsIndirectCommand;

		DEIcmd cmd;
		cmd.baseVertex = part.base_vertex + the_model->get_merged_vertex_ofs();
		cmd.count = part.element_count;
		cmd.firstIndex = part.element_offset + the_model->get_merged_index_ptr();
		cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;
		cmd.primCount = 1;
		cmd.baseInstance = 0;

		std::span<DEIcmd> out_cmds = arena.alloc_bottom_span<DEIcmd>(actual_count);
		for (int i = 0; i < actual_count; i++)
			out_cmds[i] = cmd;

		multidraw_commands->upload(out_cmds.data(), out_cmds.size_bytes());
	};
	make_multidraw_buffer();
}

void DecalBatcher::draw_decals() {
	GPUFUNCTIONSTART;


	if (!r_drawdecals.get_bool())
		return;
	//RenderPassSetup setup("decalgbuffer",fbo.gbuffer,false,false,0,0, view_to_use.width, view_to_use.height);
	//auto scope = device.start_render_pass(setup);

	RenderPassState setup2;
	auto color_targets = {
		ColorTargetInfo(draw.tex.scene_gbuffer0),
		ColorTargetInfo(draw.tex.scene_gbuffer1),
		ColorTargetInfo(draw.tex.scene_gbuffer2),
		ColorTargetInfo(draw.tex.scene_color),
		ColorTargetInfo(draw.tex.editor_id_buffer),
		ColorTargetInfo(draw.tex.scene_motion),
	};
	setup2.color_infos = color_targets;
	setup2.depth_info = draw.tex.scene_depth;
	IGraphicsDevice::inst->set_render_pass(setup2);

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);


	draw.bind_texture_ptr(20/* FIXME, defined to be bound at spot 20, also in MasterDecalShader.txt*/, draw.tex.scene_depth);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, draw.buf.decal_uniforms->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, indirection_buffer->get_internal_handle());
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, multidraw_commands->get_internal_handle());

	vertexarrayhandle vao = g_modelMgr.get_vao_ptr(VaoType::Animated)->get_internal_handle();
	const GLenum index_type = MODEL_INDEX_TYPE_GL;

	int cur_offset = 0;
	for (int i = 0; i < draws.size(); i++) {
		DecalDraw& ddraw = draws[i];
		MaterialInstance* const l = ddraw.shared_pipeline_material;
		const program_handle program = ddraw.the_program_to_use;
		RenderPipelineState state;
		state.depth_testing = true;
		state.depth_writes = false;
		state.program = program;
		state.vao = vao;
		state.blend = l->get_master_material()->blend;
		draw.get_device().set_pipeline(state);
		auto& texs = l->impl->get_textures();
		for (int j = 0; j < texs.size(); j++)
			draw.bind_texture_ptr(j, texs[j]->gpu_ptr);

		draw.shader().set_uint("decal_indirect_offset", cur_offset);

		const int dei_size = sizeof(gpu::DrawElementsIndirectCommand);
		glMultiDrawElementsIndirect(
			GL_TRIANGLES,
			index_type,
			(void*)int64_t(cur_offset * dei_size),
			ddraw.count,
			dei_size
		);

		cur_offset += ddraw.count;
	}
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}
