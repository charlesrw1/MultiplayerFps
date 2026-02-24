#include "GpuCullingTest.h"

GpuCullingTest::GpuCullingTest()
{
	model_data_buffer = IGraphicsDevice::inst->create_buffer({});
	object_buffer = IGraphicsDevice::inst->create_buffer({});
	multidraw_buffer = IGraphicsDevice::inst->create_buffer({});
	objindirect = IGraphicsDevice::inst->create_buffer({});
	matindirect = IGraphicsDevice::inst->create_buffer({});
	cull_data = IGraphicsDevice::inst->create_buffer({});
	cull_compute = draw.get_prog_man().create_compute("CullCompute.txt");


}

GpuCullingTest::~GpuCullingTest()
{
}
GpuCullingTest* GpuCullingTest::inst = nullptr;
#include "ModelManager.h"
#include "Frustum.h"
#include <algorithm>
ConfigVar skip_obj_upload("skip_obj_upload", "0", CVAR_BOOL, "");
void GpuCullingTest::build_data()
{
	// fill model data buffer
	// fill mdi buffer (+ sort)
	
	GPUSCOPESTART(gpu_cull);
	std::vector<gpu::DrawElementsIndirectCommand> commands;
	std::vector<int> material_indirection;
	std::vector<const Model*> cmd_to_model;


	cmd_mats.clear();

	auto make_key = [&](int i) -> draw_call_key {
		draw_call_key k{};
		const MaterialInstance* this_mat = cmd_mats[i];
		if (!this_mat)
			return k;
		auto parent = this_mat->impl->get_master_impl();
		if (!parent)
			return k;

		k.backface = parent->backface;
		k.blending = (uint64_t)parent->blend;
		k.mesh = cmd_to_model[i]->get_uid();
		k.texture = cmd_mats[i]->impl->texture_id_hash;
		k.shader = matman.get_mat_shader(
			nullptr, cmd_mats[i],
			0
		);
		return k;
	};

	std::unordered_map<Model*, int> mod_to_ofs;
	static std::unordered_map<const Model*, int> models_used;
	{
		GPUSCOPESTART(mod_data);

		std::vector<int> mod_data_buf;
		const hash_set<Model>& all_models = g_modelMgr.get_all_models();
		for (auto m : all_models) {
			const int start = mod_data_buf.size();
			mod_to_ofs[(Model*)m] = mod_data_buf.size();
			const int num_lods = m->get_num_lods();
			mod_data_buf.push_back(num_lods);
			for (int i = 0; i < num_lods; i++) {
				mod_data_buf.push_back(m->get_lod(i).part_ofs);
				mod_data_buf.push_back(m->get_lod(i).part_count);
				const float f = m->get_lod(i).end_percentage;
				mod_data_buf.push_back(*((int*)&f));
			}
			const int num_parts = m->get_num_parts();
			for (int i = 0; i < num_parts; i++) {
				const int mat = m->get_part(i).material_idx;
				const MaterialInstance* matp = m->get_material(mat);

				auto& part = m->get_part(i);
				gpu::DrawElementsIndirectCommand cmd{};
				cmd.baseVertex = part.base_vertex + m->get_merged_vertex_ofs();
				cmd.count = part.element_count;
				cmd.firstIndex = part.element_offset + m->get_merged_index_ptr();
				cmd.firstIndex /= MODEL_BUFFER_INDEX_TYPE_SIZE;

				// Important! Set primCount to 0 because visible instances will increment this
				cmd.primCount = 0;// meshb.count;
				cmd.baseInstance = 0;

				mod_data_buf.push_back(commands.size());

				material_indirection.push_back(matp->impl->gpu_buffer_offset);
				commands.push_back(cmd);
				cmd_to_model.push_back(m);
				cmd_mats.push_back(matp);
			}
		}

		// sort the commands around
		{
			std::vector<int> sorted;
			
			for (int i = 0; i < cmd_mats.size(); i++)
				sorted.push_back(i);
			std::sort(sorted.begin(), sorted.end(),
				[&](int a, int b) -> bool {
			
					return make_key(a).as_uint64() < make_key(b).as_uint64();
			
				});

			auto copiedmats = cmd_mats;
			for (int i = 0; i < sorted.size(); i++)
				cmd_mats[i] = copiedmats[sorted[i]];
			auto copiedmods = cmd_to_model;
			for (int i = 0; i < sorted.size(); i++)
				cmd_to_model[i] = copiedmods[sorted[i]];
			auto copiedmi = material_indirection;
			for (int i = 0; i < sorted.size(); i++)
				material_indirection[i] = copiedmi[sorted[i]];
			auto copcommands = commands;
			for (int i = 0; i < sorted.size(); i++)
				commands[i] = copcommands[sorted[i]];

			std::vector<int> inv_sorted(sorted.size());
			for (int i = 0; i < sorted.size(); i++) {
				inv_sorted[sorted[i]] = i;
			}

			// must adjust model index
			for (auto& [mod, ofs] : mod_to_ofs) {
				const int num_lods = mod_data_buf.at(ofs);
				for (int lod = 0; lod < num_lods; lod++) {
					const int part_ofs = mod_data_buf.at(ofs + 1 + lod * 3);
					const int part_count = mod_data_buf.at(ofs + 1 + lod * 3+1);
					for (int part = 0; part < part_count; part++) {
						const int index = ofs + 1 + 3 * num_lods + part_ofs + part;
						const int command_prev_index = mod_data_buf.at(index);
						const int remapped = inv_sorted.at(command_prev_index);
						mod_data_buf.at(index) = remapped;
					}
				}
			}
		}
		model_data_buffer->upload(mod_data_buf.data(), mod_data_buf.size() * sizeof(int));

	}
	if (!skip_obj_upload.get_bool()) {
		models_used.clear();
		auto& all_objs = draw.scene.proxy_list.objects;
		// first fill object buffer

		std::vector<CullObject> cos;
		int index = 0;
		for (auto& [h, o] : all_objs) {
			if (o.proxy.ignore_in_baking) {
				CullObject co{};
				co.bounds_sphere = o.bounding_sphere_and_radius;
				const int ofs = mod_to_ofs[o.proxy.model];;
				co.model_ofs = glm::ivec4(ofs, index, 0, 0);
				cos.push_back(co);
				models_used[o.proxy.model] += 1;
			}
			index += 1;
		}
		object_buffer->upload(cos.data(), cos.size() * sizeof(CullObject));
		cull.num_objects = cos.size();
	}

	// set base instances
	ASSERT(cmd_to_model.size() == commands.size() && commands.size() == material_indirection.size());
	int base_inst_sum = 0;
	for (int i = 0; i < cmd_to_model.size(); i++) {
		const Model* model = cmd_to_model[i];
		commands[i].baseInstance = base_inst_sum;
		base_inst_sum += models_used[model] + 100;
	}
	matindirect->upload(material_indirection.data(), material_indirection.size() * sizeof(int));
	if (!skip_obj_upload.get_bool()) {
		objindirect->upload(nullptr, sizeof(int) * base_inst_sum);
	}
	multidraw_buffer->upload(commands.data(), commands.size() * sizeof(gpu::DrawElementsIndirectCommand));



	{
		const float inv_two_times_tanfov = 1.0 / (tan(draw.get_current_frame_vs().fov * 0.5));
		const float inv_two_times_tanfov_2 = inv_two_times_tanfov * inv_two_times_tanfov;
		cull.inv_two_times_tanfov_2 = inv_two_times_tanfov_2;
		auto& vs = draw.current_frame_view;
		cull.camera_origin =glm::vec4( vs.origin,1);

		Frustum frustum;
		build_a_frustum_for_perspective(frustum, draw.current_frame_view);
		cull.frustum_up = frustum.top_plane;
		cull.frustum_down = frustum.bot_plane;
		cull.frustum_l = frustum.left_plane;
		cull.frustum_r = frustum.right_plane;


	}
	cull_data->upload(&cull, sizeof(CullData));

	{
		GPUSCOPESTART(cull_step);


		auto& device = draw.get_device();

		device.set_shader(cull_compute);
		const int co_size = cull.num_objects;
		const int groups = glm::ceil(int(co_size) / 256.f);

		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, multidraw_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, objindirect->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, model_data_buffer->get_internal_handle());
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, object_buffer->get_internal_handle());
		glBindBufferBase(GL_UNIFORM_BUFFER, 5, cull_data->get_internal_handle());

		glDispatchCompute(groups, 1, 1);

		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	}

	{
		batches.clear();

		// set up mesh batches (groups of drawelements commands, 1 multidraw command)
		Multidraw_Batch batch;
		batch.first = 0;
		batch.count = 1;

		const Model* batch_model = cmd_to_model.at(0);
		auto batch_sort_key = make_key(0);

		for (int i = 1; i < commands.size(); i++)
		{
			
			const Model* this_model = cmd_to_model.at(i);
			auto this_sort_key = make_key(i);

			bool batch_this = false;

			bool same_layer = batch_sort_key.layer == this_sort_key.layer;
			bool same_vao = batch_sort_key.vao == this_sort_key.vao;
			bool same_material = batch_sort_key.texture == this_sort_key.texture;
			bool same_shader = batch_sort_key.shader == this_sort_key.shader;
			bool same_other_state = batch_sort_key.blending == this_sort_key.blending
				&& batch_sort_key.backface == this_sort_key.blending;

			if (true) {
				if (same_vao && same_material && same_other_state && same_shader && same_layer)
					batch_this = true;	// can batch with different meshes
				else
					batch_this = false;

			}
			else {// pass==DEPTH
				// can batch across texture changes as long as its not alpha tested
				if (same_shader && same_vao && same_other_state)
					batch_this = true;
				else
					batch_this = false;
			}

			if (batch_this) {
				batch.count += 1;
			}
			else {
				batches.push_back(batch);
				batch.count = 1;
				batch.first = i;

				batch_model = this_model;
				batch_sort_key = this_sort_key;
			}
		}

		batches.push_back(batch);
	}
}

void setup_batch2(const MaterialInstance* mat, const int i, const int offset)
{

	const program_handle program = matman.get_mat_shader(
		nullptr, mat,
		0
	);
	const BlendState blend = mat->get_master_material()->blend;
	const bool show_backface = false;

	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(VaoType::Lightmapped);

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface;
	state.blend = blend;
	state.depth_testing = true;
	//state.depth_writes = depth_write_enabled;
	state.depth_writes = !mat->get_master_material()->is_translucent();
	//state.depth_less_than = depth_less_than_op;
	draw.get_device().set_pipeline(state);


	draw.shader().set_int("indirect_material_offset", offset);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = 0;// t->gl_id;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}
extern const GLenum MODEL_INDEX_TYPE_GL;
extern ConfigVar test_ignore_bake;
void GpuCullingTest::dodraw()
{
	if (!test_ignore_bake.get_bool())
		return;
	

	IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();
	auto& scene = draw.scene;
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, scene.gpu_render_instance_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, scene.gpu_skinned_mats_buffer);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, material_buffer->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, objindirect->get_internal_handle());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, matindirect->get_internal_handle());

	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, multidraw_buffer->get_internal_handle());

	int offset = 0;
	const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
	for (int i = 0; i < batches.size(); i++) {
		const int count = batches.at(i).count;
		const int mat_ofs = batches.at(i).first;
		//const int count = 1;// list.command_count[i];
		const int incr = count;// pass.batches[i].count;
		if (count != 0) {

			setup_batch2(cmd_mats[mat_ofs], i, offset);

			const GLenum index_type = MODEL_INDEX_TYPE_GL;

			void* indirect_ptr = nullptr;
		
			indirect_ptr = (void*)(int64_t(offset * DEIcmdSz));

			glMultiDrawElementsIndirect(
				GL_TRIANGLES,
				index_type,
				indirect_ptr,
				count,
				sizeof(gpu::DrawElementsIndirectCommand)
			);

		}
		offset += incr;
	}
	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

