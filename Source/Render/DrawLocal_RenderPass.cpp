#include "DrawLocal.h"
#include "Framework/Util.h"
#include "Render/Texture.h"
#include "imgui.h"
#include "glm/ext/matrix_transform.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtx/euler_angles.hpp"
#include "glm/gtc/type_ptr.hpp"
#include "Animation/SkeletonData.h"
#include "Animation/Runtime/Animation.h"
#include "Debug.h"
#include <SDL2/SDL.h>
#include "UI/GUISystemPublic.h"
#include "Assets/AssetDatabase.h"
#include "Game/Components/ParticleMgr.h"
#include "Game/Components/GameAnimationMgr.h"
#include "Render/ModelManager.h"
#include "Render/RenderWindow.h"
#include "tracy/public/tracy/Tracy.hpp"
#include <tracy/public/tracy/TracyOpenGL.hpp>
#include "Framework/ArenaAllocator.h"
#include "IGraphicsDevice.h"
#include "RenderGiManager.h"
#include "GpuCullingTest.h"
#include "Framework/ArenaStd.h"
#include <algorithm>

void Renderer::render_bloom_chain(texhandle scene_color_handle) {
	ZoneScoped;
	GPUFUNCTIONSTART;

	if (!enable_bloom.get_bool())
		return;

	// device.reset_states();

	//	RenderPassSetup setup("bloompass", fbo.bloom, false, false, 0, 0, cur_w, cur_h);
	//	auto scope = device.start_render_pass(setup);

	/// IGraphicsDevice* device = (&gfx());

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.bloom_downsample;
		device.set_pipeline(state);

		//*set_shader(prog.bloom_downsample);
		float src_x = cur_w;
		float src_y = cur_h;

		device.bind_texture(0, scene_color_handle);
		// glBindTextureUnit(0, scene_color_handle);
		gfx().set_clear_color(0, 0, 0, 1);
		for (int i = 0; i < tex.number_bloom_mips; i++) {
			auto& bc = tex.bloom_chain[i];

			// glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0,bc.texture->get_internal_handle(), 0);

			auto setup_pass = [&]() {
				auto color_infos = {ColorTargetInfo(bc.texture)};
				RenderPassState pass;
				pass.color_infos = color_infos;
				gfx().set_render_pass(pass);
			};
			setup_pass();

			shader().set_vec2("srcResolution", vec2(src_x, src_y));
			shader().set_int("mipLevel", i);
			src_x = bc.fsize.x;
			src_y = bc.fsize.y;

			device.set_viewport(0, 0, src_x, src_y);
			device.clear_framebuffer(false, true /* clear color*/);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);

			// glBindTextureUnit(0, bc.texture->get_internal_handle());
			device.bind_texture_ptr(0, bc.texture);
		}
	}

	{
		RenderPipelineState state;
		state.vao = get_empty_vao();
		state.program = prog.bloom_upsample;
		state.blend = BlendState::ADD;
		device.set_pipeline(state);

		for (int i = tex.number_bloom_mips - 1; i > 0; i--) {
			auto& bc = tex.bloom_chain[i - 1];

			// glNamedFramebufferTexture(fbo.bloom, GL_COLOR_ATTACHMENT0, bc.texture->get_internal_handle(), 0);
			auto setup_pass = [&]() {
				auto color_infos = {ColorTargetInfo(bc.texture)};
				RenderPassState pass;
				pass.color_infos = color_infos;
				gfx().set_render_pass(pass);
			};
			setup_pass();

			vec2 destsize = bc.fsize;
			device.set_viewport(0, 0, destsize.x, destsize.y);

			// glBindTextureUnit(0, bc.texture->get_internal_handle());
			device.bind_texture_ptr(0, bc.texture);
			shader().set_float("filterRadius", 0.0001f);

			gfx().draw_arrays(GraphicsPrimitiveType::Triangles, 0, 3);
		}
	}

	// device.reset_states();
}

void setup_batch(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled, bool force_show_backfaces,
				 bool depth_less_than_op, const int i, const int offset) {
	const auto& batch = pass.batches[i];
	const auto& mesh_batch = pass.mesh_batches[batch.first];

	const MaterialInstance* mat = (MaterialInstance*)mesh_batch.material;
	const draw_call_key batch_key = pass.objects[mesh_batch.first].sort_key;
	const program_handle program = (program_handle)batch_key.shader;
	const BlendState blend = (BlendState)batch_key.blending;
	const bool show_backface = batch_key.backface;
	const uint32_t layer = batch_key.layer;
	const VaoType vaoType = (VaoType)batch_key.vao;
	IGraphicsVertexInput* vao_ptr = g_modelMgr.get_vao_ptr(vaoType);

	RenderPipelineState state;
	state.program = program;
	state.vao = vao_ptr->get_internal_handle();
	state.backface_culling = !show_backface && !force_show_backfaces;
	state.blend = blend;
	state.depth_testing = depth_test_enabled;
	// state.depth_writes = depth_write_enabled;
	state.depth_writes = !mat->get_master_material()->is_translucent();
	state.depth_less_than = depth_less_than_op;
	draw.get_device().set_pipeline(state);

	draw.shader().set_int("indirect_material_offset", offset);

	auto& textures = mat->impl->get_textures();

	for (int i = 0; i < textures.size(); i++) {
		Texture* t = textures[i];
		uint32_t id = 0; // t->gl_id;
		if (t->gpu_ptr) {
			id = t->gpu_ptr->get_internal_handle();
		}
		draw.bind_texture(i, id);
	}
}

void draw_model_simple_no_material(Model* model) {
	if (model->get_num_lods() == 0)
		return;
	auto& lod = model->get_lod(0);
	for (int p = 0; p < lod.part_count; p++) {
		auto& part = model->get_part(p);
		gfx().draw_elements_base_vertex(
			GraphicsPrimitiveType::Triangles, part.element_count, MODEL_INDEX_TYPE,
			part.element_offset + model->get_merged_index_ptr(),
			part.base_vertex + model->get_merged_vertex_ofs());
	}
}

ConfigVar use_client_buffer_mdi("use_client_buffer_mdi", "0", CVAR_BOOL, "");
int setup_execute_render_lists(Render_Lists& list, Render_Pass& pass) {
	auto& scene = draw.scene;

	IGraphicsBuffer* material_buffer = matman.get_gpu_material_buffer();

	gfx().bind_storage_buffer_base(2, scene.gpu_instance_buffer);
	gfx().bind_storage_buffer_base_raw(3, scene.gpu_skinned_mats_buffer);
	gfx().bind_storage_buffer_base(4, material_buffer);
	gfx().bind_storage_buffer_base_raw(5, list.glinstance_to_instance);
	int offset_command_bytes = 0;
	if (0) {
		const int size = pass.mesh_batches.size() * sizeof(int);
		gfx().bind_storage_buffer_range_raw(6, list.gldrawid_to_submesh_material, size, size);
		const int command_size = list.commands.size() * sizeof(gpu::DrawElementsIndirectCommand);
		gfx().bind_indirect_buffer_raw(list.gpu_command_list);
		offset_command_bytes = command_size;
		// gfx().bind_storage_buffer_base_raw(6, list.gldrawid_to_submesh_material);
		// if (use_client_buffer_mdi.get_bool())
		//	gfx().bind_indirect_buffer(nullptr);
		// else
		//	gfx().bind_indirect_buffer_raw(list.gpu_command_list);

		auto buf = list.get_count_buf();
		ASSERT(buf);
		gfx().bind_parameter_buffer(buf);
	} else {
		gfx().bind_storage_buffer_base_raw(6, list.gldrawid_to_submesh_material);
		if (use_client_buffer_mdi.get_bool())
			gfx().bind_indirect_buffer(nullptr);
		else
			gfx().bind_indirect_buffer_raw(list.gpu_command_list);
	}

	if (scene.has_lightmap && scene.lightmapObj.lightmap_texture) {
		auto texture = scene.lightmapObj.lightmap_texture;
		draw.bind_texture_ptr(20 /* FIXME, defined to be bound at spot 20,*/, texture->gpu_ptr);
	} else {
		draw.bind_texture_ptr(20 /* FIXME, defined to be bound at spot 20,*/, draw.black_texture);
	}

	return offset_command_bytes;
}

void Renderer::execute_render_lists(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled,
									bool force_show_backfaces, bool depth_less_than_op) {
	const int offset_buffer_start = setup_execute_render_lists(list, pass);
	int offset = 0;
	const int DEIcmdSz = sizeof(gpu::DrawElementsIndirectCommand);
	for (int i = 0; i < pass.batches.size(); i++) {
		const int count = list.command_count[i];
		const int incr = pass.batches[i].count;
		if (count != 0) {

			setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset);

			const void* indirect_ptr = nullptr;
			if (use_client_buffer_mdi.get_bool())
				indirect_ptr = (const void*)(list.commands.data() + offset);
			else
				indirect_ptr = (const void*)(intptr_t)(offset_buffer_start + offset * DEIcmdSz);

			if (0) {
				gfx().multi_draw_elements_indirect_count(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					offset_buffer_start + offset * DEIcmdSz, i * (int)sizeof(uint32), count,
					sizeof(gpu::DrawElementsIndirectCommand));
			} else {
				gfx().multi_draw_elements_indirect(
					GraphicsPrimitiveType::Triangles, MODEL_INDEX_TYPE,
					indirect_ptr, count, sizeof(gpu::DrawElementsIndirectCommand));
			}
			stats.total_draw_calls++;
		}
		offset += incr;
	}
	gfx().bind_indirect_buffer(nullptr);
}

void Renderer::render_lists_old_way(Render_Lists& list, Render_Pass& pass, bool depth_test_enabled,
									bool force_show_backfaces, bool depth_less_than_op) {
	setup_execute_render_lists(list, pass);
	int offset = 0;
	for (int i = 0; i < pass.batches.size(); i++) {
		setup_batch(list, pass, depth_test_enabled, force_show_backfaces, depth_less_than_op, i, offset);

		const int count = list.command_count[i];
		const auto& batch = pass.batches[i];
		for (int dc = 0; dc < batch.count; dc++) {
			auto& cmd = list.commands.at(offset + dc);

			gfx().draw_elements_instanced_base_vertex_base_instance(
				GraphicsPrimitiveType::Triangles, cmd.count, MODEL_INDEX_TYPE,
				cmd.firstIndex * MODEL_BUFFER_INDEX_TYPE_SIZE,
				cmd.primCount, cmd.baseVertex, cmd.baseInstance);

			stats.total_draw_calls++;
		}

		offset += count;
	}
}

void Renderer::render_level_to_target(const Render_Level_Params& params) {
	ZoneScoped;
	// TracyGpuZone("render_to_target");

	device.reset_states();

	bufferhandle what_ubo = params.provied_constant_buffer;
	{
		bool upload = params.upload_constants;
		if (params.provied_constant_buffer == 0) {
			what_ubo = ubo.current_frame;
			upload = true;
		}
		if (upload)
			upload_ubo_view_constants(params.view, what_ubo, params.wireframe_secondpass);
	}

	gfx().bind_uniform_buffer_base_raw(0, what_ubo);

	if (params.pass == Render_Level_Params::SHADOWMAP) {
		gfx().set_polygon_offset(true, params.offset_poly_units, 4 /* this does nothing?*/);
		//*glCullFace(GL_FRONT);
		//*glDisable(GL_CULL_FACE);
	}

	if (params.pass == Render_Level_Params::FORWARD_PASS) {
		// fixme, for lit transparents
		const Texture* reflectionProbeTex = scene.get_reflection_probe_for_render(params.view.origin);
		if (reflectionProbeTex)
			bind_texture_ptr(19, reflectionProbeTex->gpu_ptr);
		else {
			// uh...
			bind_texture_ptr(19, black_texture); // expects a cubemap...
		}
		bind_texture(18, EnviornmentMapHelper::get().integrator.get_texture());
	}

	if (params.rl && params.rp) {
		// shadows map dont have reversed Z, just standard 0,1 depth
		//*if (params.pass != Render_Level_Params::SHADOWMAP)
		//*	glDepthFunc(GL_GREATER);

		const bool force_backface_state =
			params.pass == Render_Level_Params::SHADOWMAP || r_debug_mode.get_integer() != 0;

		const bool depth_less_than =
			params.wants_non_reverse_z; // params.pass == Render_Level_Params::SHADOWMAP;	// else, GL_GREATER
		const bool depth_testing = true;
		// const bool depth_writes = params.pass != Render_Level_Params::TRANSLUCENT;
		if (dont_use_mdi.get_bool()) {
			render_lists_old_way(*params.rl, *params.rp, depth_testing, force_backface_state, depth_less_than);
		} else {
			execute_render_lists(*params.rl, *params.rp, depth_testing, force_backface_state, depth_less_than);
		}
	}

	// glClearDepth(1.0);
	// glDepthFunc(GL_LESS);
	gfx().set_polygon_offset(false, 0, 0);
	// glCullFace(GL_BACK);
	// glEnable(GL_CULL_FACE);

	device.reset_states();
}

void Renderer::render_particles() {
	device.reset_states();
	auto& pobjs = scene.particle_objs.objects;
	auto* default_mat = MaterialInstance::load("particle_default.mm");
	assert(default_mat);
	for (auto& p_ : pobjs) {
		auto& p = p_.type_;
		const MaterialInstance* mat = p.obj.material;
		if (!mat)
			mat = default_mat;

		RenderPipelineState state;
		state.program = matman.get_mat_shader(nullptr, mat, 0);
		state.vao = p.dd.VAO; // meshbuilder->VAO;
		state.backface_culling = mat->get_master_material()->backface;
		state.blend = mat->get_master_material()->blend;
		state.depth_testing = true;
		state.depth_writes = state.blend == BlendState::OPAQUE;
		state.depth_less_than = false;
		device.set_pipeline(state);

		shader().set_uint("FS_IN_Matid", mat->impl->gpu_buffer_offset);
		shader().set_mat4("Model", p.obj.transform);
		shader().set_mat4("ViewProj", current_frame_view.viewproj);

		auto& textures = mat->impl->get_textures();
		for (int i = 0; i < textures.size(); i++) {
			Texture* tex = textures[i];
			IGraphicsTexture* gfx_tex = white_texture;
			if (tex)
				gfx_tex = tex->gpu_ptr;
			bind_texture_ptr(i, gfx_tex);
		}

		gfx().draw_elements(GraphicsPrimitiveType::Triangles, p.dd.num_indicies,
							VertexInputIndexType::uint32, 0);
	}
}

#include <algorithm>

static void build_standard_cpu(Render_Lists& list, Render_Pass& src, Free_List<ROP_Internal>& proxy_list) {
	ZoneScopedN("build_standard_cpu");

	auto& memArena = draw.get_arena();
	ArenaScope memScope(memArena);
	std::span<uint32_t> draw_to_material = memArena.alloc_bottom_span<uint32_t>(src.mesh_batches.size());

	// first build the lists
	list.build_from(src, proxy_list, draw_to_material);

	const int objCount = src.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = src.objects[objIndex];

		uint32_t precount = list.commands[obj.batch_idx].primCount++; // increment count
		uint32_t ofs = list.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = proxy_list.handle_to_obj[obj.render_obj.id];
	}

	// cull objects (just cull every obj, f it)
	// then dispatch a compute that looks in glinstance_to_instance, finds instance vis status
	// then increments

	gfx().upload_buffer_raw(list.gldrawid_to_submesh_material,
							sizeof(uint32_t) * draw_to_material.size(),
							draw_to_material.data());

	gfx().upload_buffer_raw(list.glinstance_to_instance,
							sizeof(uint32_t) * objCount, glinstance_to_instance);

	const int command_list_size_bytes = sizeof(gpu::DrawElementsIndirectCommand) * list.commands.size();
	gfx().upload_buffer_raw(list.gpu_command_list, command_list_size_bytes,
							list.commands.data());
}
void Render_Lists_Gpu_Culled::init(uint32_t drawidsz, uint32_t instbufsz) {
	Render_Lists::init(drawidsz, instbufsz);
	inst_to_obj = gfx().create_buffer({});
	count_buffer = gfx().create_buffer({});
	batches_buf = gfx().create_buffer({});
}

ConfigVar collapse_draw_calls("collapse_draw_calls", "1", CVAR_BOOL | CVAR_DEV, "");
static void build_cascade_cpu(Render_Lists& shadowlist, Render_Pass& shadowpass, Free_List<ROP_Internal>& proxy_list,
							  uint8_t* visiblity) {
	Memory_Arena& memArena = draw.get_arena();
	ArenaScope memScope(memArena);
	std::span<uint32_t> draw_to_material = memArena.alloc_bottom_span<uint32_t>(shadowpass.mesh_batches.size());

	// first build the lists
	shadowlist.build_from(shadowpass, proxy_list, draw_to_material);

	const int objCount = shadowpass.objects.size();
	uint32_t* glinstance_to_instance = memArena.alloc_bottom_type<uint32_t>(objCount);

	for (int objIndex = 0; objIndex < objCount; objIndex++) {
		auto& obj = shadowpass.objects[objIndex];
		int id = proxy_list.handle_to_obj[obj.render_obj.id];
		// ASSERT(visiblity[id]);

		bool visible = true;
		int8_t wantlod = 0;
		// split_input_lod_arr(visiblity[id], visible, wantlod);
		if (!visible)
			continue;

		uint32_t precount = shadowlist.commands[obj.batch_idx].primCount++; // increment count
		uint32_t ofs = shadowlist.commands[obj.batch_idx].baseInstance;

		// set the pointer to the Render_Object strucutre that will be found on the gpu
		glinstance_to_instance[ofs + precount] = id;
	}

	auto collapse_commands = [](Render_Lists& list, std::span<uint32_t> draw_to_material) {
		assert(draw_to_material.size() == list.commands.size());
		int command_ofs = 0;
		for (int j = 0; j < list.command_count.size(); j++) {
			const int cmd_cnt = list.command_count[j];
			std::span<gpu::DrawElementsIndirectCommand> sub_span(&list.commands.at(command_ofs), cmd_cnt);
			std::span<uint32_t> sub_drawid_to_mat_span(&draw_to_material[command_ofs], cmd_cnt);
			int new_count = 0;
			for (int i = 0; i < cmd_cnt; i++) {
				if (sub_span[i].primCount != 0) {
					if (new_count != i) {
						sub_span[new_count] = sub_span[i];
						sub_drawid_to_mat_span[new_count] = sub_drawid_to_mat_span[i];
					}
					new_count += 1;
				}
			}
			list.command_count[j] = new_count;

			command_ofs += cmd_cnt;
		}
	};
	// collapses draw calls so 0 instance calls are removed.
	// this only applies to shadows as gbuffer/transparent passes will always have >= 1 instances
	// seems like a small performance win, but not really nessecary
	if (collapse_draw_calls.get_bool())
		collapse_commands(shadowlist, draw_to_material);

	gfx().upload_buffer_raw(shadowlist.gldrawid_to_submesh_material,
							sizeof(uint32_t) * draw_to_material.size(),
							draw_to_material.data());

	gfx().upload_buffer_raw(shadowlist.glinstance_to_instance,
							sizeof(uint32_t) * objCount, nullptr);
	gfx().sub_upload_buffer_raw(shadowlist.glinstance_to_instance, 0,
								sizeof(uint32_t) * objCount, glinstance_to_instance);

	auto& list = shadowlist;
	const int command_list_size_bytes = sizeof(gpu::DrawElementsIndirectCommand) * list.commands.size();
	gfx().upload_buffer_raw(list.gpu_command_list, command_list_size_bytes,
							list.commands.data());
}
